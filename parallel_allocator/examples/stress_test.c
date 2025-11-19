/*
 * stress_test.c - 单线程压力测试
 * 
 * 对分配器进行大量分配和释放操作以测试其稳定性
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "allocator.h"

#define NUM_ITERATIONS 10000
#define MAX_ALLOC_SIZE 10240

int main(void) {
    printf("\n=== Stress Test (Single-threaded) ===\n\n");
    printf("Iterations: %d\n", NUM_ITERATIONS);
    printf("Max allocation size: %d bytes\n\n", MAX_ALLOC_SIZE);
    
    // 初始化
    if (allocator_init(0) != 0) {  // 0 = 单线程模式（更快）
        fprintf(stderr, "Failed to initialize allocator\n");
        return 1;
    }
    
    // 记录开始时间
    time_t start = time(NULL);
    
    // 压力测试：随机分配和释放
    void **ptrs = (void **)malloc(sizeof(void *) * NUM_ITERATIONS);
    if (ptrs == NULL) {
        fprintf(stderr, "Failed to allocate pointer array\n");
        return 1;
    }
    
    int success_count = 0;
    int fail_count = 0;
    size_t total_allocated = 0;
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // 随机大小
        size_t size = (rand() % MAX_ALLOC_SIZE) + 1;
        
        // 分配
        void *ptr = myalloc(size);
        if (ptr != NULL) {
            ptrs[i] = ptr;
            total_allocated += size;
            success_count++;
        } else {
            ptrs[i] = NULL;
            fail_count++;
        }
        
        // 每 1000 次迭代打印进度
        if ((i + 1) % 1000 == 0) {
            printf("Progress: %d/%d allocations\n", i + 1, NUM_ITERATIONS);
        }
    }
    
    printf("\nAllocation phase complete:\n");
    printf("  Success: %d\n", success_count);
    printf("  Failed: %d\n", fail_count);
    printf("  Total allocated: %zu bytes\n\n", total_allocated);
    
    // 验证
    if (allocator_verify() != 0) {
        fprintf(stderr, "ERROR: Allocator verification failed after allocation!\n");
        return 1;
    }
    printf("✓ Allocator passed verification after allocation phase\n\n");
    
    // 释放所有
    printf("Freeing all allocations...\n");
    int free_success = 0;
    int free_fail = 0;
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        if (ptrs[i] != NULL) {
            if (myfree(ptrs[i]) == 0) {
                free_success++;
            } else {
                free_fail++;
                fprintf(stderr, "ERROR: Free failed for ptr[%d]\n", i);
            }
        }
    }
    
    printf("Free phase complete:\n");
    printf("  Success: %d\n", free_success);
    printf("  Failed: %d\n\n", free_fail);
    
    // 最终验证
    if (allocator_verify() != 0) {
        fprintf(stderr, "ERROR: Allocator verification failed after free!\n");
        return 1;
    }
    printf("✓ Allocator passed verification after free phase\n\n");
    
    // 检查无泄漏
    size_t allocated, freed, peak;
    allocator_stats(&allocated, &freed, &peak);
    
    if (allocated == 0) {
        printf("✓ No memory leaks! All memory properly freed.\n");
    } else {
        fprintf(stderr, "✗ WARNING: %zu bytes still allocated!\n", allocated);
    }
    
    // 时间统计
    time_t end = time(NULL);
    printf("\nTime elapsed: %ld seconds\n", end - start);
    printf("Operations per second: %.0f\n", 
           (double)(NUM_ITERATIONS * 2) / (end - start));
    
    // 清理
    free(ptrs);
    allocator_cleanup();
    
    printf("\n=== Test Complete ===\n\n");
    
    return (fail_count > 0 || free_fail > 0) ? 1 : 0;
}