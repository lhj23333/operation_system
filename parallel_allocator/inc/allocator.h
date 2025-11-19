/*
 * allocator.h - 并行内存分配器公共接口
 * 
 * 设计概述：
 * 这是用户可见的最高级接口，提供 myalloc/myfree，类似 malloc/free。
 * 
 * 职责：
 * 1. 初始化和清理全局分配器
 * 2. 提供 myalloc/myfree 的公共接口
 * 3. 管理全局堆对象
 * 4. 提供统计和诊断功能
 * 5. 处理参数验证和错误处理
 * 
 * 使用流程：
 * 1. 可选：显式调用 allocator_init(1) 初始化
 *    （如果不调用，myalloc 会自动初始化）
 * 2. 使用 myalloc/myfree 分配和释放内存
 * 3. 可选：调用 allocator_stats 获取统计信息
 * 4. 程序退出前调用 allocator_cleanup 清理
 */

#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ========== 公共接口 ========== */

/**
 * @name myalloc
 * @brief 分配内存
 * 
 * @param
 *      size - 要分配的大小（byte)
 * 
 * @return
 *      successful - 分配的内存起始地址
 *      failed - NULL
 * 
 * @attention
 *      - 错误处理：
 *          - 如果无法分配足够的内存，返回 NULL
 *          - 如果 size == 0， 可返回 NULL 或最小块
 */
void *myalloc(size_t size);


/**
 * @name myfree
 * @brief 释放内存
 * 
 * @param
 *      ptr - 要释放的内存地址（必须是之前 myalloc的返回值）
 * 
 * @return
 *      0  - successful 
 *      -1 - failed 
 * 
 * @attention
 *      - 错误处理：
 *          - 如果 ptr == NULL, 安全返回 0 (无操作)
 *          - 如果 ptr 非法（未分配），返回 -1
 *          - 如果 ptr 被重复释放（double free），返回 -1
 */
int myfree(void *ptr);


/**
 * @name allocator_init
 * @brief 初始化分配器
 * 
 * @param
 *      enable_concurrency - 是否启用多线程支持
 * 
 * 
 * @return 
 *      0  - initial successful
 *      -1 - initial failed
 * 
 * @attention
 *      - 必须在第一次调用 myalloc/myfree 前调用
 *      - 如果未显式初始化，myalloc 会自动初始化（启用并发）
 */
int allocator_init(bool enable_concurrency);


/** 
 * @name allocator_cleanup
 * @brief 清理分配器
 * 
 * @return
 *      0  - cleanup successful
 *      -1 - cleanup falied
 * 
 * @attention
 *      - 后置条件
 *          - 所有虚拟内存已释放
 *          - 所有内部锁已销毁
 *          - 再次调用 myalloc/myfree 前应重新初始化
 */
int allocator_cleanup(void);


/** 
 * @name allocator_stats
 * @brief 获取分配器统计信息
 * 
 * @param
 *      allocated_out - 输出：当前已分配大小
 * @param
 *      free_out      - 输出：空闲大小
 * @param
 *      peak_out      - 输出：峰值分配大小
 * 
 * @return
 *      0  - successful
 *      -1 - failed
 */
int allocator_stats(size_t *allocator_out, size_t *free_out, size_t *peak_out);


/**
 * @name allocator_dump
 * @brief 转储分配器内部状态 - 用于调试
 */
void allocator_dump(void);


/**
 * @name allocator_verify
 * @brief 验证分配器的完整性 - 检查内部数据的一致性和完整性
 * 
 * @return 
 *      0  - 分配器状态良好
 *      -1 - 发现错误（可能堆损坏）
 */
int allocator_verify(void);

#endif /* __ALLOCATOR_H__ */