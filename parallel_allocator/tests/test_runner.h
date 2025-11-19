/*
 * test_runner.h - 测试框架
 * 
 * 提供简单的单元测试宏和工具函数
 */

#ifndef __TEST_RUNNER_H__
#define __TEST_RUNNER_H__

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

/* ========== 测试宏 ========== */

/**
 * TEST - 定义一个测试函数
 */
#define TEST(name) void name(void)

/**
 * ASSERT_EQ - 相等断言
 */
#define ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n", (msg)); \
            fprintf(stderr, "  Expected: %ld, Got: %ld\n", \
                    (long)(expected), (long)(actual)); \
            exit(1); \
        } \
    } while(0)

/**
 * ASSERT_NOT_NULL - 非空断言
 */
#define ASSERT_NOT_NULL(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n", (msg)); \
            exit(1); \
        } \
    } while(0)

/**
 * ASSERT_NULL - 空断言
 */
#define ASSERT_NULL(ptr, msg) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n", (msg)); \
            exit(1); \
        } \
    } while(0)

/**
 * ASSERT_TRUE - 布尔真断言
 */
#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n", (msg)); \
            exit(1); \
        } \
    } while(0)

/**
 * ASSERT_FALSE - 布尔假断言
 */
#define ASSERT_FALSE(cond, msg) \
    do { \
        if ((cond)) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n", (msg)); \
            exit(1); \
        } \
    } while(0)

/**
 * TRYCATCH - 安全地运行测试，捕获异常
 */
#define TRYCATCH(test_func, passed, failed) \
    do { \
        printf("Running: %s\n", #test_func); \
        int err = setjmp(_test_jmp_buf); \
        if (err == 0) { \
            test_func(); \
            (passed)++; \
        } else { \
            (failed)++; \
        } \
    } while(0)

static jmp_buf _test_jmp_buf;

#endif /* __TEST_RUNNER_H__ */