#include "thread_pool.h"
#include "task_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

/* ======== 测试辅助函数 ======== */

static void print_test_header(const char *test_name) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║ %-58s ║\n", test_name);
    printf("╚════════════════════════════════════════════════════════════╝\n");
}

static void print_test_result(const char *test_name, int passed) {
    if (passed) {
        printf("✓ %s PASSED\n", test_name);
    } else {
        printf("✗ %s FAILED\n", test_name);
    }
}

/* ======== 测试任务函数 ======== */

typedef struct {
    int value;
    int sleep_ms;
    pthread_mutex_t *mutex;
    int *counter;
} test_task_arg_t;

static void simple_task(void *arg) {
    int *value = (int *)arg;
    (*value)++;
}

static void counter_task(void *arg) {
    test_task_arg_t *task_arg = (test_task_arg_t *)arg;
    if (task_arg->sleep_ms > 0) {
        usleep(task_arg->sleep_ms * 1000);
    }
    pthread_mutex_lock(task_arg->mutex);
    (*task_arg->counter)++;
    pthread_mutex_unlock(task_arg->mutex);
}

static void cleanup_task_arg(void *arg) {
    free(arg);
}

/* ======== 测试用例 ======== */

void test_queue_basic() {
    print_test_header("Test 1: Queue Basic Operations");
    
    int passed = 1;
    task_queue_t *queue = task_queue_create(10);
    assert(queue != NULL);
    
    // 测试空队列
    assert(task_queue_get_count(queue) == 0);
    
    // 测试入队
    int values[5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        int ret = task_queue_submit(queue, simple_task, &values[i], NULL);
        assert(ret == 0);
    }
    assert(task_queue_get_count(queue) == 5);
    
    // 测试出队
    bool shutdown = false;
    for (int i = 0; i < 5; i++) {
        Task *task = task_queue_pop(queue, &shutdown);
        assert(task != NULL);
        task->function(task->arg);
        free(task);
    }
    assert(task_queue_get_count(queue) == 0);
    
    // 验证任务执行结果
    for (int i = 0; i < 5; i++) {
        assert(values[i] == i + 2);
    }
    
    task_queue_destroy(queue);
    print_test_result("test_queue_basic", passed);
}

void test_pool_create_destroy() {
    print_test_header("Test 2: Thread Pool Create & Destroy");
    
    int passed = 1;
    
    // 测试创建
    thread_pool_cfg_t config = {
        .num_threads = 4,
        .queue_size = 100,
        .stack_size = 0,
        .daemon_threads = false
    };
    
    thread_pool_t *pool = thread_pool_create(&config);
    assert(pool != NULL);
    assert(pool->num_threads == 4);
    assert(pool->state == POOL_RUNNING);
    
    // 等待线程启动
    usleep(100000);
    
    // 打印线程池信息
    thread_pool_print_info(pool);
    
    // 测试销毁
    thread_pool_destroy(pool);
    
    print_test_result("test_pool_create_destroy", passed);
}

void test_pool_concurrent_execution() {
    print_test_header("Test 3: Thread Pool Concurrent Execution");
    
    int passed = 1;
    
    thread_pool_cfg_t config = {
        .num_threads = 4,
        .queue_size = 100,
        .stack_size = 0,
        .daemon_threads = false
    };
    
    thread_pool_t *pool = thread_pool_create(&config);
    assert(pool != NULL);
    
    // 提交并发任务
    const int NUM_TASKS = 50;
    int counter = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    
    for (int i = 0; i < NUM_TASKS; i++) {
        test_task_arg_t *arg = malloc(sizeof(test_task_arg_t));
        arg->value = i;
        arg->sleep_ms = 10;
        arg->mutex = &mutex;
        arg->counter = &counter;
        
        int ret = thread_pool_submit(pool, counter_task, arg, cleanup_task_arg);
        assert(ret == 0);
    }
    
    printf("Submitted %d tasks, waiting for completion...\n", NUM_TASKS);
    
    // 等待所有任务完成
    thread_pool_wait_all(pool);
    
    // 验证结果
    pthread_mutex_lock(&mutex);
    int final_counter = counter;
    pthread_mutex_unlock(&mutex);
    
    printf("Counter value: %d (expected: %d)\n", final_counter, NUM_TASKS);
    assert(final_counter == NUM_TASKS);
    
    thread_pool_print_info(pool);
    
    pthread_mutex_destroy(&mutex);
    thread_pool_destroy(pool);
    
    print_test_result("test_pool_concurrent_execution", passed);
}

void test_queue_backpressure() {
    print_test_header("Test 4: Queue Backpressure");
    
    int passed = 1;
    
    // 创建小队列测试背压
    const int QUEUE_SIZE = 5;
    thread_pool_cfg_t config = {
        .num_threads = 2,
        .queue_size = QUEUE_SIZE,
        .stack_size = 0,
        .daemon_threads = false
    };
    
    thread_pool_t *pool = thread_pool_create(&config);
    assert(pool != NULL);
    
    int counter = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    
    // 提交慢任务填满队列
    for (int i = 0; i < QUEUE_SIZE + 2; i++) {
        test_task_arg_t *arg = malloc(sizeof(test_task_arg_t));
        arg->value = i;
        arg->sleep_ms = 100;  // 100ms 慢任务
        arg->mutex = &mutex;
        arg->counter = &counter;
        
        printf("Submitting task %d...\n", i);
        int ret = thread_pool_submit(pool, counter_task, arg, cleanup_task_arg);
        assert(ret == 0);
    }
    
    printf("Queue size: %zu\n", task_queue_get_count(pool->task_queue));
    
    // 等待任务完成
    thread_pool_wait_all(pool);
    
    pthread_mutex_lock(&mutex);
    int final_counter = counter;
    pthread_mutex_unlock(&mutex);
    
    printf("All tasks completed, counter: %d\n", final_counter);
    assert(final_counter == QUEUE_SIZE + 2);
    
    pthread_mutex_destroy(&mutex);
    thread_pool_destroy(pool);
    
    print_test_result("test_queue_backpressure", passed);
}

void test_pool_performance() {
    print_test_header("Test 5: Thread Pool Performance");
    
    int passed = 1;
    
    thread_pool_cfg_t config = {
        .num_threads = 8,
        .queue_size = 1000,
        .stack_size = 0,
        .daemon_threads = false
    };
    
    thread_pool_t *pool = thread_pool_create(&config);
    assert(pool != NULL);
    
    const int NUM_TASKS = 1000;
    int counter = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // 提交大量任务
    for (int i = 0; i < NUM_TASKS; i++) {
        test_task_arg_t *arg = malloc(sizeof(test_task_arg_t));
        arg->value = i;
        arg->sleep_ms = 1;
        arg->mutex = &mutex;
        arg->counter = &counter;
        
        int ret = thread_pool_submit(pool, counter_task, arg, cleanup_task_arg);
        assert(ret == 0);
    }
    
    thread_pool_wait_all(pool);
    
    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                     (end.tv_usec - start.tv_usec) / 1000.0;
    
    pthread_mutex_lock(&mutex);
    int final_counter = counter;
    pthread_mutex_unlock(&mutex);
    
    printf("Performance Results:\n");
    printf("  Tasks completed: %d\n", final_counter);
    printf("  Time elapsed: %.2f ms\n", elapsed);
    printf("  Throughput: %.2f tasks/sec\n", (NUM_TASKS * 1000.0) / elapsed);
    
    assert(final_counter == NUM_TASKS);
    
    thread_pool_print_info(pool);
    task_queue_print_stats(pool->task_queue);
    
    pthread_mutex_destroy(&mutex);
    thread_pool_destroy(pool);
    
    print_test_result("test_pool_performance", passed);
}

/* ======== 主测试函数 ======== */

int main() {
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("         Thread Pool Test Suite\n");
    printf("════════════════════════════════════════════════════════════\n");
    
    test_queue_basic();
    test_pool_create_destroy();
    test_pool_concurrent_execution();
    test_queue_backpressure();
    test_pool_performance();
    
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("         All Tests Completed\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    return 0;
}