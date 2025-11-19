#ifndef __COMMON_H_
#define __COMMON_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ========== 常量定义 ========== */

/** Linux x86_64 系统页大小 */
#define PAGE_SIZE           4096                    

/** 内存对齐大小（8 字节） */
#define ALIGN_SIZE          8

/** 最小分配单位 */
#define MIN_ALLOC_SIZE      ALIGN_SIZE

/** 初始堆大小（10 页） */
#define POOL_INIT_SIZE      (PAGE_SIZE * 10)

/** 堆扩展大小增量（20 页） */
#define HEAP_EXTEND_SIZE    (PAGE_SIZE * 20)


/* ========== 对齐宏定义 ========== */

#define ALIGN_UP(addr, align)       (((addr) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(addr, align)     ((addr) & ~((align) - 1))
#define IS_ALIGNED(addr, align)     (((addr) & ((align) - 1)) == 0)
#define IS_8BYTE_ALIGNED(addr)     (IS_ALIGNED(addr, ALIGN_SIZE))

/* ========== 错误代码 ========== */

typedef enum {
    ERR_SUCCESS = 0,                // 成功
    ERR_INVALID_ADDR = -1,          // 无效地址
    ERR_INVALID_SIZE = -2,          // 无效大小
    ERR_NO_MEMORY = -3,             // 内存不足
    ERR_NOMEM_AVAIL = -4,           // 无可用内存
    ERR_DOUBLE_FREE = -5,           // 重复释放
    ERR_CORRUPTED = -6,             // 内存损坏
    ERR_LOCK_FAILED = -7,           // 锁失败
} error_code_t;

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
                fprintf(stderr, "ASSERTION FAILED: %s\n", msg); \
                fprintf(stderr, "  File: %s, Line: %d\n", __FILE__, __LINE__); \
                abort(); \
            } \
        } while(0)
#else
    #define DEBUG_PRINT(fmt, ...)
    #define ASSERT(cond, msg)
#endif

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

/* ========== 类型定义 ========== */

/**
 * mem_range_t - 内存范围
 * 
 * 用于表示 [start, start+size) 的内存范围
 */
typedef struct {
    uintptr_t start;
    size_t size;
} mem_range_t;  

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

/* ========== 内存操作工具 ========== */

/**
 * safe_strlen - 安全的字符串长度获取
 * 
 * 返回值：
 *   有效字符串 - 字符串长度
 *   NULL - 0
 */
static inline size_t safe_strlen(const char *str) {
    return str != NULL ? strlen(str) : 0;
}

/**
 * safe_memcpy - 安全的内存复制
 * 
 * 检查指针有效性
 */
static inline void safe_memcpy(void *dst, const void *src, size_t size) {
    if (dst != NULL && src != NULL && size > 0) {
        memcpy(dst, src, size);
    }
}

#endif /* __COMMON_H__ */