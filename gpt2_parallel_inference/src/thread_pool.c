#include "thread_pool.h"
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <unistd.h> 

/* ======== 内部函数 ======== */

/**
 * 线程信息（用于监控）
 */
typedef struct {
	thread_pool_t *pool;
	thread_info_t *info;
} thread_arg_t;

static void *_work_thread(void *arg) {
    thread_arg_t *thread_arg = (thread_arg_t *)arg;
    thread_pool_t *pool = thread_arg->pool;
    thread_info_t *info = thread_arg->info;
    
    // 保存线程 ID
    info->tid = pthread_self();
    INFO_PRINT("Worker thread started (id: %d, tid: %lu)",
            info->id, (unsigned long)info->tid);
    free(thread_arg);

    while (true) {
        // 线程安全地检查退出标志
        pthread_mutex_lock(&pool->state_lock);
        bool should_exit = info->should_exit;
        pthread_mutex_unlock(&pool->state_lock);
        
        if (should_exit) {
            DEBUG_PRINT("Worker %d exiting as requested", info->id);
            break;
        }

        info->is_active = true;
        struct timeval start, end;
        gettimeofday(&start, NULL);

        // 直接传递 pool->shutdown 的地址，确保读取到最新值
        int ret = task_queue_pop_and_execute(pool->task_queue, (bool*)&pool->shutdown);
        
        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                         ((double)end.tv_usec - start.tv_usec) / 1000.0;

        if (ret == 1) {
            DEBUG_PRINT("Worker %d received shutdown signal", info->id);
            break;
        } else if (ret == 0) {
            info->tasks_completed++;
            DEBUG_PRINT("Worker %d completed task (%.2f ms)", info->id, elapsed);
        }
        
        info->is_active = false;
    }

    INFO_PRINT("Worker thread exiting (id: %d, tid: %lu)",
            info->id, pthread_self());

    return NULL;
}

static int _thread_pool_expand(thread_pool_t *pool, int new_size, int cur_size) {
    INFO_PRINT("Expanding thread pool by %d threads", new_size - cur_size);

    pthread_mutex_lock(&pool->state_lock);

    // 分配新线程信息数组
    pthread_t *new_threads = realloc(pool->threads, new_size * sizeof(pthread_t));
    if (new_threads == NULL) {
        ERROR_PRINT("Failed to reallocate threads array");
        pthread_mutex_unlock(&pool->state_lock);
        return -1;
    }
    pool->threads = new_threads;
    
    thread_info_t *new_infos = realloc(pool->thread_infos, new_size * sizeof(thread_info_t));
    if (new_infos == NULL) {
        ERROR_PRINT("Failed to reallocate thread_infos array");
        pthread_mutex_unlock(&pool->state_lock);
        return -1;
    }
    pool->thread_infos = new_infos;

    // 创建新线程
    for (int i = cur_size; i < new_size; i ++) {
        pool->thread_infos[i].id = i;
        pool->thread_infos[i].tasks_completed = 0;
        pool->thread_infos[i].is_active = false;
        pool->thread_infos[i].should_exit = false;
        pool->thread_infos[i].tid = 0;

        // 准备线程参数
        thread_arg_t *thread_arg = (thread_arg_t *)malloc(sizeof(thread_arg_t));
        if (thread_arg == NULL) {
            ERROR_PRINT("Failed to allocate thread argument");
            // 清理已创建的新线程
            for (int j = cur_size; j < i; j++) {
                pool->thread_infos[j].should_exit = true;
            }
            pthread_mutex_unlock(&pool->state_lock);
            pthread_cond_broadcast(&pool->task_queue->cond_not_empty);
            for (int j = cur_size; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            pthread_mutex_lock(&pool->state_lock);
            pool->num_threads = cur_size;
            pthread_mutex_unlock(&pool->state_lock);
            return -1;
        }
        thread_arg->pool = pool;
        thread_arg->info = &pool->thread_infos[i];

        // 创建线程
        int ret = pthread_create(&pool->threads[i], NULL, _work_thread, thread_arg);
        if (ret != 0) {
            ERROR_PRINT("Failed to create worker thread %d: %s",
                    i, strerror(ret));
            free(thread_arg);
            // 清理已创建的新线程
            for (int j = cur_size; j < i; j++) {
                pool->thread_infos[j].should_exit = true;
            }
            pthread_mutex_unlock(&pool->state_lock);
            pthread_cond_broadcast(&pool->task_queue->cond_not_empty);
            for (int j = cur_size; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            pthread_mutex_lock(&pool->state_lock);
            pool->num_threads = cur_size;
            pthread_mutex_unlock(&pool->state_lock);
            return -1;
        }

        DEBUG_PRINT("Created new thread %d (tid: %lu)",
                i, (unsigned long)pool->threads[i]);
    }

    // 更新线程池信息
    pool->num_threads = new_size;

    pthread_mutex_unlock(&pool->state_lock);

    INFO_PRINT("Thread pool expanded successfully");
    return 0;
}

static int _thread_pool_shrink(thread_pool_t *pool, int new_size, int cur_size) {
    INFO_PRINT("Shrinking thread pool by %d threads", cur_size - new_size);

    // 在持锁状态下设置退出标志
    pthread_mutex_lock(&pool->state_lock);
    for (int i = new_size; i < cur_size; i ++) {
        pool->thread_infos[i].should_exit = true;
        DEBUG_PRINT("Signaled thread %d to exit", i);
    }
    pthread_mutex_unlock(&pool->state_lock);

    // 唤醒所有等待线程
    pthread_mutex_lock(&pool->task_queue->mutex);
    pthread_cond_broadcast(&pool->task_queue->cond_not_empty);
    pthread_mutex_unlock(&pool->task_queue->mutex);

    // 等待线程退出
    for (int i = new_size; i < cur_size; i ++) {
        DEBUG_PRINT("Joining thread %d ....", i);
        int ret = pthread_join(pool->threads[i], NULL);
        if (ret != 0) {
            ERROR_PRINT("Failed to join thread %d: %s",
                    i, strerror(ret));
        } else {
            DEBUG_PRINT("Thread %d joined successfully", i);
        }
    }

    // 更新线程池信息
    pthread_mutex_lock(&pool->state_lock);
    pool->num_threads = new_size;
    pthread_mutex_unlock(&pool->state_lock);

    // 缩小线程数组大小以释放内存
    pthread_t *new_threads = realloc(pool->threads, new_size * sizeof(pthread_t));
    thread_info_t *new_thread_infos = realloc(pool->thread_infos, new_size * sizeof(thread_info_t));
    if (new_threads != NULL) {
        pool->threads = new_threads;
    }
    if (new_thread_infos != NULL) {
        pool->thread_infos = new_thread_infos;
    }

    INFO_PRINT("Thread pool shrunk successfully");
    return 0;
}

/* ======== 线程池 API ======== */
thread_pool_t *thread_pool_create(const thread_pool_cfg_t *config) {
	if (config == NULL) {
		ERROR_PRINT("Invalid thread pool configuration");
		return NULL;
	}

	if (config->num_threads <= 0) {
		ERROR_PRINT("Invalid thread count: %d", config->num_threads);
		return NULL;
	}

	INFO_PRINT("Creating thread pool (threads: %d, queue: %d)",
			config->num_threads, config->queue_size);


	// 分配线程池结构
	thread_pool_t *pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
	if (pool == NULL) {
		ERROR_PRINT("Failed to allocate thread pool");
		return NULL;
	}

	// 初始化线程池
	pool->num_threads =config->num_threads;
	pool->state = POOL_CREATED;
	pool->shutdown = false;

	if (pthread_mutex_init(&pool->state_lock, NULL) != 0) {
		ERROR_PRINT("Failed to initialize mutex");
		free(pool);
		return NULL;
	}

	if (pthread_cond_init(&pool->state_changed, NULL) != 0) {
		ERROR_PRINT("Failed to initialize condition variable");
		pthread_mutex_destroy(&pool->state_lock);
		free(pool);
		return NULL;
	}

	// 创建任务队列
	pool->task_queue = task_queue_create(config->queue_size);
	if (pool->task_queue == NULL) {
		ERROR_PRINT("Failed to create task queue");
		pthread_mutex_destroy(&pool->state_lock);
		pthread_cond_destroy(&pool->state_changed);
		free(pool);
		return NULL;
	}

	// 分配线程信息数组
	pool->threads = (pthread_t *)malloc(config->num_threads * sizeof(pthread_t));
	pool->thread_infos = (thread_info_t *)malloc(config->num_threads * sizeof(thread_info_t));
	if (pool->threads == NULL || pool->thread_infos == NULL) {
		ERROR_PRINT("Failed to allocate thread info arrays");
		task_queue_destroy(pool->task_queue);
		free(pool->threads);
		free(pool->thread_infos);
		pthread_mutex_destroy(&pool->state_lock);
		pthread_cond_destroy(&pool->state_changed);
		free(pool);
		return NULL;
	}

	// 创建工作线程
	for (int i = 0; i < config->num_threads; i ++) {
		pool->thread_infos[i].id = i;
		pool->thread_infos[i].tasks_completed = 0;
		pool->thread_infos[i].is_active = false;
		pool->thread_infos[i].should_exit = false;
		pool->thread_infos[i].tid = 0;

		// 准备线程参数
		thread_arg_t *thread_arg = (thread_arg_t *)malloc(sizeof(thread_arg_t));
		if (thread_arg == NULL) {
			ERROR_PRINT("Failed to allocate thread argument");
			// 清理已创建的线程
			pool->shutdown = true;
			pthread_cond_broadcast(&pool->task_queue->cond_not_empty);
			for (int j = 0; j < i; j ++) {
				pthread_join(pool->threads[j], NULL);
			}
			task_queue_destroy(pool->task_queue);
			free(pool->threads);
			free(pool->thread_infos);
			pthread_mutex_destroy(&pool->state_lock);
			pthread_cond_destroy(&pool->state_changed);
			free(pool);
			return NULL;
		}
		thread_arg->pool = pool;
		thread_arg->info = &pool->thread_infos[i];

		// 设置线程属性
		pthread_attr_t attr;
		pthread_attr_init(&attr);

		if (config->stack_size > 0) {
			pthread_attr_setstacksize(&attr, config->stack_size);
		}
		if (config->daemon_threads) {
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		}

		int ret = pthread_create(&pool->threads[i], &attr, _work_thread, thread_arg);
		pthread_attr_destroy(&attr);
		if (ret != 0) {
			ERROR_PRINT("Failed to create worker thread %d: %s",
						i, strerror(ret));
			free(thread_arg);

			// 清理已创建的线程
			pool->shutdown = true;
			pthread_cond_broadcast(&pool->task_queue->cond_not_empty);

			for (int j = 0; j < i; j ++) {
				pthread_join(pool->threads[j], NULL);
			}

			task_queue_destroy(pool->task_queue);
			free(pool->threads);
			free(pool->thread_infos);
			pthread_mutex_destroy(&pool->state_lock);
			pthread_cond_destroy(&pool->state_changed);
			free(pool);
			return NULL;
		}
		
		DEBUG_PRINT("Thread %d creaded (tid: %lu)", 
					i, pool->threads[i]);
	}

	// 更新线程池状态
	pthread_mutex_lock(&pool->state_lock);
	pool->state = POOL_RUNNING;
	pthread_cond_broadcast(&pool->state_changed);
	pthread_mutex_unlock(&pool->state_lock);

	INFO_PRINT("Thread pool created successfully");

	return pool;
}

void thread_pool_destroy(thread_pool_t *pool) {
	if (pool == NULL) return;

	INFO_PRINT("Destroying thread pool...");

	// 立即关闭线程池
	pthread_mutex_lock(&pool->state_lock);
	pool->state = POOL_STOPPING;
	pool->shutdown = true;
	pthread_mutex_unlock(&pool->state_lock);

	// 唤醒所有等待线程
	pthread_mutex_lock(&pool->task_queue->mutex);
	pthread_cond_broadcast(&pool->task_queue->cond_not_empty);
	pthread_mutex_unlock(&pool->task_queue->mutex);

	// 等待所有线程退出
	for (int i = 0; i < pool->num_threads; i ++) {
		DEBUG_PRINT("Joining thread %d ....", i);
		pthread_join(pool->threads[i], NULL);
	}

	// 清理资源
	task_queue_destroy(pool->task_queue);
	free(pool->threads);
	free(pool->thread_infos);

	pthread_mutex_lock(&pool->state_lock);
	pool->state = POOL_STOPPED;
	pthread_cond_broadcast(&pool->state_changed);
	pthread_mutex_unlock(&pool->state_lock);

	pthread_mutex_destroy(&pool->state_lock);
	pthread_cond_destroy(&pool->state_changed);

	free(pool);

	INFO_PRINT("Thread pool destroyed");

	return;
}

int thread_pool_submit(thread_pool_t *pool, void (*func)(void *), void *arg, void (*cleanup)(void *)) {
    if (pool == NULL || func == NULL) {
        ERROR_PRINT("Invalid arguments");
        return -1;
    }

    pthread_mutex_lock(&pool->state_lock);
    // 检查线程池状态
    if (pool->state != POOL_RUNNING) {
        ERROR_PRINT("Thread pool is not running");
        pthread_mutex_unlock(&pool->state_lock);
        return -1;
    }
    pthread_mutex_unlock(&pool->state_lock);

    int ret = task_queue_submit(pool->task_queue, func, arg, cleanup);
    if (ret != 0) {
        ERROR_PRINT("Failed to submit task to queue");
        return -1;
    }

    return 0;
}

void thread_pool_wait_all(thread_pool_t *pool) {
	if (pool == NULL) return;

	INFO_PRINT("Waiting for all tasks to complete");
	task_queue_wait_empty(pool->task_queue);
	INFO_PRINT("All tasks completed");
	
	return;
}

void thread_pool_shutdown(thread_pool_t *pool) {
	if (pool == NULL) return ;

	INFO_PRINT("Shutting down thread pool gracefully...");

	// 等待现有任务完成
	thread_pool_wait_all(pool);

	// 摧毁线程池
	thread_pool_destroy(pool);
	
	return;
}

int thread_pool_resize(thread_pool_t *pool, int new_size) {
	if (pool == NULL || new_size <= 0) {
		ERROR_PRINT("Invalid arguments");
		return -1;
	}

	INFO_PRINT("Resizing thread pool from %d to %d threads",
			pool->num_threads, new_size);

	pthread_mutex_lock(&pool->state_lock);
	int cur_size = pool->num_threads;
	pthread_mutex_unlock(&pool->state_lock);

	if (new_size == cur_size) {
		INFO_PRINT("Thread pool size unchanged");
		return 0;
	}

	if (new_size > cur_size) {
		// 扩容
		return _thread_pool_expand(pool, new_size, cur_size);
	} else {
		// 缩容
		return _thread_pool_shrink(pool, new_size, cur_size);
	}
}

void thread_pool_print_info(const thread_pool_t *pool) {
    if (pool == NULL) return;
    
    pthread_mutex_lock((pthread_mutex_t *)&pool->state_lock);
    thread_pool_state_t state = pool->state;
    int num_threads = pool->num_threads;
    
    thread_info_t *thread_infos_snapshot = (thread_info_t *)malloc(num_threads * sizeof(thread_info_t));
    if (thread_infos_snapshot != NULL) {
        memcpy(thread_infos_snapshot, pool->thread_infos, num_threads * sizeof(thread_info_t));
    }
    pthread_mutex_unlock((pthread_mutex_t *)&pool->state_lock);
    
    size_t pending_tasks = task_queue_get_count(pool->task_queue);
    
    printf("╔════════════════════════════════════╗\n");
    printf("║     Thread Pool Information        ║\n");
    printf("╠════════════════════════════════════╣\n");
    printf("║ Status:          %-18s║\n", 
           state == POOL_RUNNING ? "RUNNING" : 
           state == POOL_STOPPING ? "STOPPING" : 
           state == POOL_STOPPED ? "STOPPED" : "CREATED");
    printf("║ Num threads:     %-18d║\n", num_threads);
    printf("║ Pending tasks:   %-18zu║\n", pending_tasks);
    printf("║                                    ║\n");
    printf("║ Worker threads:                    ║\n");
    
    if (thread_infos_snapshot != NULL) {
        for (int i = 0; i < num_threads; i++) {
            printf("║   [%2d] tasks=%-8zu %-13s║\n",
                   i,
                   thread_infos_snapshot[i].tasks_completed,
                   thread_infos_snapshot[i].is_active ? "ACTIVE" : "IDLE");
        }
        free(thread_infos_snapshot);
    }
    
    printf("╚════════════════════════════════════╝\n");
}