/*
 * concurrent_stress.c - 多线程并发压力测试
 * 
 * 演示分配器在多线程环境下的性能和正确性
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "allocator.h"

#define NUM_THREADS 4
#define ITERATIONS_PER_THREAD 5000
#define MAX_ALLOC_SIZE 4096

typedef struct {
    int thread_id;
    int alloc_count;
    int free_count;
    size_t total_allocated;
} thread_stats_t;

thread_stats_t g_stats[NUM_THREADS];

/**
 * worker_thread - 工作线程函数
 * 
 * 每个线程随机分配和释放内存
 */
void *worker_thread(void *arg) {
    int tid = (intptr_t)arg;
    thread_stats_t *stats = &g_stats[tid];
    
    printf("[Thread %d] Starting...\n", tid);
    
    void **ptrs = (void **)malloc(sizeof(void *) * ITERATIONS_PER_THREAD);
    if (ptrs == NULL) {
        fprintf(stderr, "[Thread %d] Failed to allocate pointer array\n", tid);
        return NULL;
    }
    
    stats->thread_id = tid;
    stats->alloc_count = 0;
    stats->free_count = 0;
    stats->total_allocated = 0;
    
    // 分配阶段
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        size_t size = (rand() % MAX_ALLOC_SIZE) + 1;
        void *ptr = myalloc(size);
        
        if (ptr != NULL) {
            ptrs[i] = ptr;
            stats->alloc_count++;
            stats->total_allocated += size;
        } else {
            ptrs[i] = NULL;
        }
        
        // 每 500 次迭代打印进度
        if ((i + 1) % 500 == 0) {
            printf("[Thread %d] Allocated %d times\n", tid, i + 1);
        }
    }
    
    printf("[Thread %d] Allocation phase complete: %d allocations\n",
           tid, stats->alloc_count);
    
    // 释放阶段
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        if (ptrs[i] != NULL) {
            if (myfree(ptrs[i]) == 0) {
                stats->free_count++;
            } else {
                fprintf(stderr, "[Thread %d] Free failed at iteration %d\n", tid, i);
            }
        }
    }
    
    printf("[Thread %d] Free phase complete: %d frees\n", tid, stats->free_count);
    
    free(ptrs);
    
    return NULL;
}

int main(void) {
    printf("\n=== Concurrent Stress Test ===\n\n");
    printf("Threads: %d\n", NUM_THREADS);
    printf("Iterations per thread: %d\n", ITERATIONS_PER_THREAD);
    printf("Max allocation size: %d bytes\n\n", MAX_ALLOC_SIZE);
    
    // 初始化分配器（启用多线程）
    if (allocator_init(1) != 0) {
        fprintf(stderr, "Failed to initialize allocator\n");
        return 1;
    }
    
    printf("Allocator initialized with concurrency support\n\n");
    
    // 记录开始时间
    time_t start = time(NULL);
    
    // 创建工作线程
    pthread_t threads[NUM_THREADS];
    printf("Spawning %d threads...\n\n", NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, (void *)(intptr_t)i) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\n=== All Threads Complete ===\n\n");
    
    // 打印统计信息
    printf("Thread Statistics:\n");
    printf("─────────────────────────────────────────\n");
    printf("ID  | Allocations | Frees | Total (bytes)\n");
    printf("─────────────────────────────────────────\n");
    
    size_t total_alloc = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("%2d  | %11d | %5d | %13zu\n",
               g_stats[i].thread_id,
               g_stats[i].alloc_count,
               g_stats[i].free_count,
               g_stats[i].total_allocated);
        total_alloc += g_stats[i].total_allocated;
    }
    printf("─────────────────────────────────────────\n");
    printf("TOTAL allocations: %d\n",
           NUM_THREADS * ITERATIONS_PER_THREAD);
    printf("TOTAL bytes allocated: %zu\n\n", total_alloc);
    
    // 验证分配器一致性
    printf("Verifying allocator...\n");
    if (allocator_verify() != 0) {
        fprintf(stderr, "ERROR: Allocator verification failed!\n");
        return 1;
    }
    printf("✓ Allocator verification passed\n\n");
    
    // 检查无泄漏
    size_t allocated, freed, peak;
    allocator_stats(&allocated, &freed, &peak);
    
    printf("Final Statistics:\n");
    printf("Allocated: %zu bytes\n", allocated);
    printf("Free: %zu bytes\n", freed);
    printf("Peak: %zu bytes\n\n", peak);
    
    if (allocated == 0) {
        printf("✓ No memory leaks detected!\n\n");
    } else {
        fprintf(stderr, "✗ WARNING: %zu bytes still allocated!\n\n", allocated);
        return 1;
    }
    
    // 时间统计
    time_t end = time(NULL);
    long elapsed = end - start;
    long total_ops = (long)NUM_THREADS * ITERATIONS_PER_THREAD * 2;  // alloc + free
    
    printf("Performance:\n");
    printf("Time elapsed: %ld seconds\n", elapsed);
    printf("Total operations: %ld\n", total_ops);
    printf("Operations per second: %.0f\n", elapsed > 0 ? (double)total_ops / elapsed : 0);
    printf("Operations per thread per second: %.0f\n",
           elapsed > 0 ? (double)total_ops / elapsed / NUM_THREADS : 0);
    
    // 清理
    allocator_cleanup();
    
    printf("\n=== Test Complete ===\n\n");
    
    return 0;
}