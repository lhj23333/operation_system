/*
 * test_allocator.c - allocator 层的单元测试
 * 
 * 测试 myalloc/myfree 的公共接口
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include "common.h"
#include "test_runner.h"

/* ========== 测试套件 ========== */

/**
 * test_allocator_simple_alloc_free - 简单分配释放
 */
TEST(test_allocator_simple_alloc_free) {
    printf("Test: Simple alloc/free\n");
    
    // 分配
    void *ptr = myalloc(1024);
    ASSERT_NOT_NULL(ptr, "myalloc should return non-NULL");
    
    // 检查对齐
    ASSERT_TRUE(IS_8BYTE_ALIGNED((uintptr_t)ptr), 
               "returned pointer should be 8-byte aligned");
    
    // 释放
    int ret = myfree(ptr);
    ASSERT_EQ(ret, 0, "myfree should return 0");

    printf("✓ PASS\n\n");
}

/**
 * test_allocator_null_free - NULL 指针释放
 */
TEST(test_allocator_null_free) {
    printf("Test: NULL pointer free\n");
    
    // myfree(NULL) 应该安全返回 0
    int ret = myfree(NULL);
    ASSERT_EQ(ret, 0, "myfree(NULL) should return 0");
    
    printf("✓ PASS\n\n");
    
}

/**
 * test_allocator_multiple_allocs - 多个分配
 */
TEST(test_allocator_multiple_allocs) {
    printf("Test: Multiple allocations\n");

    void *ptrs[10];
    size_t sizes[] = {100, 200, 512, 1024, 2048, 4096, 512, 256, 128, 64};
    
    // 分配 10 个块
    for (int i = 0; i < 10; i++) {
        ptrs[i] = myalloc(sizes[i]);
        ASSERT_NOT_NULL(ptrs[i], "myalloc should not fail");
        ASSERT_TRUE(IS_8BYTE_ALIGNED((uintptr_t)ptrs[i]), 
                   "all pointers should be 8-byte aligned");
    }
    
    // 检查没有重叠
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            uintptr_t ptr_i = (uintptr_t)ptrs[i];
            uintptr_t ptr_j = (uintptr_t)ptrs[j];
            ASSERT_TRUE(ptr_i != ptr_j, "pointers should be different");
        }
    }

    // 释放所有
    for (int i = 0; i < 10; i++) {
        int ret = myfree(ptrs[i]);
        ASSERT_EQ(ret, 0, "myfree should succeed");    
    }
    
    printf("✓ PASS\n\n");
}

/**
 * test_allocator_stats - 统计信息
 */
TEST(test_allocator_stats) {
    printf("Test: Statistics\n");
    
    size_t alloc_before, free_before, peak_before;
    allocator_stats(&alloc_before, &free_before, &peak_before);
    
    // 分配一些内存
    void *ptr1 = myalloc(1000);
    void *ptr2 = myalloc(2000);
    
    size_t alloc_after, free_after, peak_after;
    allocator_stats(&alloc_after, &free_after, &peak_after);
    
    // 验证统计信息
    ASSERT_TRUE(alloc_after > alloc_before, 
               "allocated should increase");
    ASSERT_TRUE(free_after < free_before,
               "free should decrease");
    ASSERT_TRUE(peak_after >= peak_before,
               "peak should not decrease");
    
    // 释放
    myfree(ptr1);
    myfree(ptr2);
    
    size_t alloc_final, free_final, peak_final;
    allocator_stats(&alloc_final, &free_final, &peak_final);
    
    ASSERT_EQ(alloc_final, alloc_before,
             "allocated should return to original");
    
    printf("✓ PASS\n\n");
}

/**
 * test_allocator_no_leak - 无内存泄漏
 */
TEST(test_allocator_no_leak) {
    printf("Test: No memory leaks\n");
    
    size_t alloc_before;
    allocator_stats(&alloc_before, NULL, NULL);
    
    // 分配和释放
    for (int i = 0; i < 100; i++) {
        void *ptr = myalloc(1024);
        ASSERT_NOT_NULL(ptr, "myalloc should not fail");
        ASSERT_EQ(myfree(ptr), 0, "myfree should succeed");
    }
    
    size_t alloc_after;
    allocator_stats(&alloc_after, NULL, NULL);
    
    ASSERT_EQ(alloc_before, alloc_after,
             "allocated size should return to original");
    
    printf("✓ PASS\n\n");
}

/**
 * test_allocator_verify - 验证一致性
 */
TEST(test_allocator_verify) {
    printf("Test: Allocator verification\n");
    
    // 分配一些内存
    void *ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = myalloc((i + 1) * 1024);
    }
    
    // 验证堆的一致性
    ASSERT_EQ(allocator_verify(), 0,
             "allocator should pass verification");
    
    // 释放内存
    for (int i = 0; i < 5; i++) {
        myfree(ptrs[i]);
    }
    
    // 再次验证
    ASSERT_EQ(allocator_verify(), 0,
             "allocator should still pass verification");
    
    printf("✓ PASS\n\n");
}

/* ========== 测试运行器 ========== */

int main(void) {
    printf("\n========== Allocator Tests ==========\n\n");
    
    int passed = 0;
    int failed = 0;
    
    TRYCATCH(test_allocator_simple_alloc_free, passed, failed);
    TRYCATCH(test_allocator_null_free, passed, failed);
    TRYCATCH(test_allocator_multiple_allocs, passed, failed);
    TRYCATCH(test_allocator_stats, passed, failed);
    TRYCATCH(test_allocator_no_leak, passed, failed);
    TRYCATCH(test_allocator_verify, passed, failed);
    
    printf("\n========== Test Results ==========\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Total:  %d\n", passed + failed);
    printf("==================================\n\n");
    
    // 清理
    allocator_cleanup();
    
    return failed > 0 ? 1 : 0;
}