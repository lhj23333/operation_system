#include "task_queue.h"
#include <string.h>
#include <time.h>

/* ============= 内部函数实现 ============= */

static void _task_destroy(Task *task) {
    DEBUG_PRINT("Destroying task object %p", task);

    if (task == NULL) {
        WARN_PRINT("Attempted to destroy NULL task");
        return;
    }

    // 调用清理函数（如果有）
    if (task->cleanup != NULL) {
        task->cleanup(task->arg);
    }

    free(task);

    DEBUG_PRINT("Task object %p destroyed successfully", task);
    return ;
}

/* ============= 任务对象 API ============= */

int task_queue_submit(task_queue_t *queue, void (*func)(void *), void *arg, void (*cleanup)(void *)) {
    DEBUG_PRINT("Creating new task object %p", func);

    if (queue == NULL || func == NULL) {
        ERROR_PRINT("Queue or function pointer is NULL");
        return -1;
    }
    
    Task *task = (Task *)malloc(sizeof(Task));
    if (task == NULL) {
        ERROR_PRINT("Failed to allocate memory for Task");
        return -1;
    }

    // 初始化任务对象
    task->function = func;
    task->arg = arg;
    task->cleanup = cleanup;
    task->next = NULL;

    DEBUG_PRINT("Task object %p created successfully", task);

    // 将任务加入队列
    int ret = task_queue_push(queue, task);
    if (ret != 0) {
        ERROR_PRINT("Failed to push task %p to queue %p", task
            , queue);
        _task_destroy(task);
        return -1;
    }

    return 0;
}


/* ============= 任务队列 API ============= */

task_queue_t* task_queue_create(size_t max_size) {
    INFO_PRINT("task_queue_create: Creating task queue with max size %zu", max_size);

    task_queue_t *queue = (task_queue_t *)malloc(sizeof(task_queue_t));
    if (queue == NULL) {
        ERROR_PRINT("task_queue_create: Failed to allocate memory for task queue");
        return NULL;
    }

    queue->front = NULL;
    queue->back = NULL;
    queue->count = 0;
    queue->max_count = max_size;
    queue->total_processed = 0;
    queue->active_tasks = 0;

    // 初始化同步对象
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        ERROR_PRINT("task_queue_create: Failed to initialize mutex");
        free(queue);
        return NULL;
    }

    if (pthread_cond_init(&queue->cond_not_empty, NULL) != 0) {
        ERROR_PRINT("task_queue_create: Failed to initialize cond_not_empty");
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }

    if (pthread_cond_init(&queue->cond_not_full, NULL) != 0) {
        ERROR_PRINT("task_queue_create: Failed to initialize cond_not_full");
        pthread_cond_destroy(&queue->cond_not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }

    if (pthread_cond_init(&queue->cond_empty, NULL) != 0) {
        ERROR_PRINT("Failed to initialize cond_empty");
        pthread_cond_destroy(&queue->cond_not_full);
        pthread_cond_destroy(&queue->cond_not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }

    if (pthread_cond_init(&queue->cond_all_done, NULL) != 0) {
        ERROR_PRINT("Failed to initialize cond_all_done");
        pthread_cond_destroy(&queue->cond_empty);
        pthread_cond_destroy(&queue->cond_not_full);
        pthread_cond_destroy(&queue->cond_not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }

    // 初始化统计信息
    memset(&queue->stats, 0, sizeof(queue->stats));

    INFO_PRINT("task_queue_create: Task queue created successfully at %p", queue);

    return queue;
}

void task_queue_destroy(task_queue_t *queue) {
    DEBUG_PRINT("task_queue_destroy: Destroying task queue %p", queue);

    if (queue == NULL) {
        WARN_PRINT("task_queue_destroy: Attempted to destroy NULL queue");
        return;
    }

    INFO_PRINT("Destroying task queue ....");

    pthread_mutex_lock(&queue->mutex);

    // 清空队列中的所有任务
    Task *task = queue->front;
    int count = 0;
    while (task != NULL) {
        Task *next = task->next;
        _task_destroy(task);
        task = next;
        count ++;
    }
    if (count > 0) {
        WARN_PRINT("task_queue_destroy: Destroyed %d pending tasks in the queue", count);
    }

    // 通知所有等待的线程退出
    pthread_cond_broadcast(&queue->cond_not_empty);
    pthread_cond_broadcast(&queue->cond_not_full);
    pthread_cond_broadcast(&queue->cond_empty);
    pthread_cond_broadcast(&queue->cond_all_done);

    pthread_mutex_unlock(&queue->mutex);

    // 销毁同步对象
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond_not_empty);
    pthread_cond_destroy(&queue->cond_not_full);
    pthread_cond_destroy(&queue->cond_empty);
    pthread_cond_destroy(&queue->cond_all_done);

    free(queue);
    
    INFO_PRINT("Task queue destroyed successfully");

    return ;
}


int task_queue_push(task_queue_t *queue, Task *task) {
    DEBUG_PRINT("Pushing task %p to queue %p", task, queue);

    if (queue == NULL || task == NULL) {
        ERROR_PRINT("Queue or task is NULL");
        return -1;
    }

    pthread_mutex_lock(&queue->mutex);

    while (queue->max_count > 0 && queue->count >= queue->max_count) {
        DEBUG_PRINT("Queue full (%zu/%zu), waiting...",
            queue->count, queue->max_count);

        // 等待队列空间
        // pthread_cond_wait 会自动释放锁，等待被唤醒后重新获取锁
        pthread_cond_wait(&queue->cond_not_full, &queue->mutex);
    }

    // === 添加任务到队列尾部 ===s
    task->next = NULL;

    if (queue->back != NULL) {
        queue->back->next = task;
    } else {
        // 队列为空，更新 front 指针
        queue->front = task;
    }

    queue->back = task;
    queue->count ++;
    queue->stats.total_enqueued ++;

    DEBUG_PRINT("Task %p pushed successfully, queue size now %zu",
        task, queue->count);

    // === 唤醒等待的消费者线程 ===
    pthread_cond_signal(&queue->cond_not_empty);

    pthread_mutex_unlock(&queue->mutex);

    return 0;
}


Task* task_queue_pop(task_queue_t *queue, volatile bool *shutdown) {
    DEBUG_PRINT("Popping task from queue %p", queue);

    if (queue == NULL) {
        ERROR_PRINT("Queue is NULL");
        return NULL;
    }

    pthread_mutex_lock(&queue->mutex);

    // === 等待任务或关闭信号 ===
    while (queue->count == 0 && !(*shutdown)) {
        DEBUG_PRINT("Queue empty, waiting for tasks...");

        // 此时自动释放锁，等待被唤醒后重新获取锁
        pthread_cond_wait(&queue->cond_not_empty, &queue->mutex);
    }

    // 检查是否收到关闭信号
    if (*shutdown && queue->count == 0) {
        DEBUG_PRINT("Shutdown signal received and queue empty");
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    // === 从队列头部取出任务 ===
    Task *task = queue->front;

    if (task != NULL) {
        queue->front = task->next;

        if (queue->front == NULL) {
            // 队列现在为空，更新 back 指针
            queue->back = NULL;
        }

        queue->count --;
        queue->stats.total_dequeued ++;
        queue->total_processed ++;

        DEBUG_PRINT("Task %p popped successfully, queue size now %zu",
            task,  queue->count);
        
        // === 唤醒等待的生产者线程 ===
        if (queue->count == 0) {
            pthread_cond_broadcast(&queue->cond_empty);
        }
        
        if (queue->max_count > 0) {
            pthread_cond_signal(&queue->cond_not_full);
        }
    }

    pthread_mutex_unlock(&queue->mutex);

    return task;
}


void task_queue_wait_empty(task_queue_t *queue) {
    DEBUG_PRINT("task_queue_wait_empty: Waiting for queue %p to become empty", queue);

    if (queue == NULL) {
        WARN_PRINT("task_queue_wait_empty: Attempted to wait on NULL queue");
        return;
    }

    INFO_PRINT("task_queue_wait_empty: Waiting for all tasks to complete...");
    
    pthread_mutex_lock(&queue->mutex);
    
    while (queue->count > 0 || queue->active_tasks > 0) {
        DEBUG_PRINT("task_queue_wait_empty: count=%zu, active_tasks=%zu", 
                    queue->count, queue->active_tasks);
        pthread_cond_wait(&queue->cond_all_done, &queue->mutex);
    }
    pthread_mutex_unlock(&queue->mutex);
    
    INFO_PRINT("task_queue_wait_empty: All tasks completed");
}

size_t task_queue_get_count(task_queue_t *queue) {
    DEBUG_PRINT("Getting task count for queue %p", queue);

    if (queue == NULL) {
        ERROR_PRINT("Queue is NULL");
        return 0;
    }

    pthread_mutex_lock(&queue->mutex);
    size_t count = queue->count;
    pthread_mutex_unlock(&queue->mutex);

    DEBUG_PRINT("Queue %p has %zu tasks", queue, count);

    return count;
}

int task_queue_pop_and_execute(task_queue_t *queue, bool *should_shutdown) {
    DEBUG_PRINT("task_queue_pop_and_execute: Waiting for task from queue %p", queue);

    if (queue == NULL || should_shutdown == NULL) {
        ERROR_PRINT("task_queue_pop_and_execute: Invalid arguments");
        return -1;
    }

    pthread_mutex_lock(&queue->mutex);

    // 等待任务或关闭信号
    while (queue->count == 0 && !(*should_shutdown)) {
        DEBUG_PRINT("task_queue_pop_and_execute: Queue empty, waiting for tasks...");
        pthread_cond_wait(&queue->cond_not_empty, &queue->mutex);
    }

    // 检查是否收到关闭信号
    if (*should_shutdown && queue->count == 0) {
        DEBUG_PRINT("task_queue_pop_and_execute: Shutdown signal received and queue empty");
        pthread_mutex_unlock(&queue->mutex);
        return 1; // 返回 1 表示应该退出
    }

    // 从队列头部取出任务
    Task *task = queue->front;
    if (task == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    queue->front = task->next;
    if (queue->front == NULL) {
        queue->back = NULL;
    }

    queue->count--;
    queue->stats.total_dequeued++;
    queue->total_processed++;
    
    // 增加活跃任务计数
    queue->active_tasks++;

    DEBUG_PRINT("task_queue_pop_and_execute: Task %p popped, queue size now %zu, active tasks %zu", 
                task, queue->count, queue->active_tasks);

    // 唤醒等待的生产者线程
    if (queue->max_count > 0) {
        pthread_cond_signal(&queue->cond_not_full);
    }

    pthread_mutex_unlock(&queue->mutex);

    // 在锁外执行任务（避免长时间持锁）
    DEBUG_PRINT("task_queue_pop_and_execute: Executing task %p", task);
    task->function(task->arg);

    // 销毁任务
    _task_destroy(task);

    DEBUG_PRINT("task_queue_pop_and_execute: Task executed and destroyed successfully");

    // 任务执行完成，减少活跃任务计数
    pthread_mutex_lock(&queue->mutex);
    queue->active_tasks--;
    
    // 如果队列为空且没有活跃任务，唤醒等待的线程
    if (queue->count == 0 && queue->active_tasks == 0) {
        DEBUG_PRINT("task_queue_pop_and_execute: All tasks completed, signaling cond_all_done");
        pthread_cond_broadcast(&queue->cond_all_done);
    }
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

void task_queue_print_stats(task_queue_t *queue) {
    if (queue == NULL) {
        WARN_PRINT("Attempted to print stats for NULL queue");
        return;
    }

    INFO_PRINT("Task Queue Statistics:");

    pthread_mutex_lock(&queue->mutex);

    printf("╔════════════════════════════════════╗\n");
    printf("║     Task Queue Statistics          ║\n");
    printf("╠════════════════════════════════════╣\n");
    printf("║ Pending tasks:   %zu%-17s║\n", queue->count, "");
    printf("║ Total enqueued:  %zu%-17s║\n", queue->stats.total_enqueued, "");
    printf("║ Total dequeued:  %zu%-17s║\n", queue->stats.total_dequeued, "");
    printf("║ Processed:       %zu%-17s║\n", queue->total_processed, "");
    printf("╚════════════════════════════════════╝\n");

    pthread_mutex_unlock(&queue->mutex);

    return ;
}