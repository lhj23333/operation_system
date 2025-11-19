/*
 * simple_alloc.c - 简单的分配器使用示例
 * 
 * 演示基本的 myalloc/myfree 使用
 */

#include <stdio.h>
#include <string.h>
#include "allocator.h"

int main(void) {
    printf("\n=== Simple Allocator Example ===\n\n");
    
    // 初始化（可选，myalloc 会自动初始化）
    printf("1. Initializing allocator...\n");
    if (allocator_init(1) != 0) {
        fprintf(stderr, "Failed to initialize allocator\n");
        return 1;
    }
    printf("   ✓ Allocator initialized with concurrency support\n\n");
    
    // 分配内存
    printf("2. Allocating memory...\n");
    void *ptr1 = myalloc(1024);
    if (ptr1 == NULL) {
        fprintf(stderr, "Failed to allocate 1024 bytes\n");
        return 1;
    }
    printf("   ✓ Allocated 1024 bytes at %p\n", ptr1);
    
    void *ptr2 = myalloc(2048);
    if (ptr2 == NULL) {
        fprintf(stderr, "Failed to allocate 2048 bytes\n");
        return 1;
    }
    printf("   ✓ Allocated 2048 bytes at %p\n", ptr2);
    
    void *ptr3 = myalloc(512);
    if (ptr3 == NULL) {
        fprintf(stderr, "Failed to allocate 512 bytes\n");
        return 1;
    }
    printf("   ✓ Allocated 512 bytes at %p\n\n", ptr3);
    
    // 使用内存
    printf("3. Using allocated memory...\n");
    memset(ptr1, 'A', 1024);
    memset(ptr2, 'B', 2048);
    memset(ptr3, 'C', 512);
    printf("   ✓ Filled memory with data\n\n");
    
    // 获取统计信息
    printf("4. Getting statistics...\n");
    size_t allocated, freed, peak;
    if (allocator_stats(&allocated, &freed, &peak) == 0) {
        printf("   Allocated: %zu bytes\n", allocated);
        printf("   Free: %zu bytes\n", freed);
        printf("   Peak: %zu bytes\n\n", peak);
    }
    
    // 转储堆状态
    printf("5. Allocator state:\n");
    allocator_dump();
    
    // 释放内存
    printf("6. Freeing memory...\n");
    if (myfree(ptr1) == 0) {
        printf("   ✓ Freed ptr1\n");
    }
    if (myfree(ptr2) == 0) {
        printf("   ✓ Freed ptr2\n");
    }
    if (myfree(ptr3) == 0) {
        printf("   ✓ Freed ptr3\n\n");
    }
    
    // 验证无泄漏
    printf("7. Final check...\n");
    if (allocator_stats(&allocated, &freed, &peak) == 0) {
        if (allocated == 0) {
            printf("   ✓ No memory leaks! All memory freed.\n\n");
        } else {
            printf("   ✗ Warning: %zu bytes still allocated\n\n", allocated);
        }
    }
    
    // 清理
    printf("8. Cleaning up...\n");
    if (allocator_cleanup() == 0) {
        printf("   ✓ Allocator cleaned up\n\n");
    }
    
    printf("=== Example Complete ===\n\n");
    
    return 0;
}