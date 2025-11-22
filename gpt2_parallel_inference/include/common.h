#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ========== 调试宏 ========== */

#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
        do { \
            fprintf(stderr, "[DEBUG] %s:%d (%s): " fmt "\n", \
                    __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
            fflush(stderr); \
        } while(0)

    #define ASSERT(cond, msg) \
        do { \
            if (!(cond)) { \
                fprintf(stderr, "[ASSERT FAILED] %s\n", msg); \
                fprintf(stderr, "  File: %s, Line: %d, Function: %s\n", \
                        __FILE__, __LINE__, __func__); \
                abort(); \
            } \
        } while(0)
#else
    #define DEBUG_PRINT(fmt, ...) do {} while(0)
    #define ASSERT(cond, msg) do {} while(0)
#endif /* DEBUG */

#define ERROR_PRINT(fmt, ...) \
    do { \
        fprintf(stderr, "[ERROR] %s:%d (%s): " fmt "\n", \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        fflush(stderr); \
    } while(0)

#define INFO_PRINT(fmt, ...) \
    do { \
        fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__); \
        fflush(stdout); \
    } while(0)

#define WARN_PRINT(fmt, ...) \
    do { \
        fprintf(stderr, "[WARN] %s:%d: " fmt "\n", \
                __FILE__, __LINE__, ##__VA_ARGS__); \
        fflush(stderr); \
    } while(0)

/* ========== 实用函数宏 ========== */

/**
 * MIN - 返回两个值中的较小值
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * MAX - 返回两个值中的较大值
 */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * ARRAY_SIZE - 获取静态数组的元素个数
 * 
 * 注意：只适用于编译期已知大小的数组
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * 内存对齐宏
 */
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

/**
 * 错误处理宏
 */
#define CHECK_NULL(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            ERROR_PRINT("%s: NULL pointer", msg); \
            return NULL; \
        } \
    } while(0)

#define CHECK_NULL_VOID(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            ERROR_PRINT("%s: NULL pointer", msg); \
            return; \
        } \
    } while(0)

#define CHECK_MALLOC(ptr) \
    do { \
        if ((ptr) == NULL) { \
            ERROR_PRINT("Memory allocation failed: %s", strerror(errno)); \
            return NULL; \
        } \
    } while(0)

#endif /* __COMMON_H__ */