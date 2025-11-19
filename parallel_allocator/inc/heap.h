/*
 * heap.h - 堆管理器接口
 * 
 * 该文件定义了堆管理器，维护一个不相交区间的集合：
 * - 空闲区间集合（空闲块链表）
 * - 已分配区间集合（分配块链表）
 * 
 * 设计目标：
 * - 维护内存不重叠不变式
 * - 支持高效的查找和合并
 * - 支持线程安全操作（后期扩展）
 */

#ifndef __HEAP_H__
#define __HEAP_H__

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include "mem_block.h"

/* ========== 堆管理器结构体 ========== */

typedef struct {
    /* 内存块链表 - 所有块按地址有序 */
    mem_block_t *blocks_head;       // 有序链表头
    size_t block_count;             // 总块数

    /* 统计信息 */
    size_t total_allocated;         // 已分配总大小
    size_t total_free;              // 空闲总大小
    size_t peak_allocated;          // 峰值分配大小

    /* 线程安全 */
    pthread_mutex_t lock;           // 全局互斥锁
    bool lock_enabled;              // 是否启用锁

    /* 分配策略 */
    int alloc_strategy;             // 0: 首适配；1: 最佳适配；2：最差适配；
} heap_t;

/* ========== 分配策略常量 ========== */

#define HEAP_STRATEGY_FIRST_FIT     0   // 首适配：返回第一个足够大的空闲块
#define HEAP_STRATEGY_BEST_FIT      1   // 最佳适配：返回最小的足够大的空闲块
#define HEAP_STRATEGY_WORST_FIT     2   // 最差适配：返回最大的空闲块

/* ========== 堆管理器接口 ========== */

/**
 * @name heap_init
 * @brief 初始化管理器
 * 
 * @param 
 *      initial_size - 初始化虚拟内存大小（必须是 PAGE_SIZE 倍数）
 * @param
 *      enable_lock - 是否启用线程安全锁
 * 
 * @return
 *      successful - 指向创建的堆管理器指针
 *      failed - NULL
 * 
 * @attention
 *      - 若 enable_lock = true, 则所有操作都是原子的
 *      - 若 enable_lock = false, 则仅支持单线程
 */
heap_t *heap_init(size_t initial_size, bool enable_lock);

/** 
 * @name heap_allocate
 * @brief 从堆中分配指定大小的内存
 * 
 * @param
 *      heap - 堆管理器指针
 * @param
 *      size - 要分配的大小（自动向上8字节对齐）
 * 
 * @return 
 *      successful - 分配的内存起始地址
 *      failed - NULL（内存不足或其他错误）
 * 
 * @attention
 *      - 返回地址8字节对齐
 *      - 返回内存块不与任何其他块重叠
 *      - 返回内存块标记为已分配  
 */
void *heap_allocate(heap_t *heap, size_t size);


/**
 * @name heap_free
 * @brief 释放堆中的内存块
 * 
 * @param
 *      heap - 堆管理器指针
 * @param
 *      addr - 要释放的内存地址
 * 
 * @return
 *      0  - successful
 *      <0 - failed（见error_code_t）
 * 
 * @attention
 *      - ERR_INVALID_ADDR：addr 不是有效分配
 *      - ERR_DOUBLE_FREE：addr 已被释放
 */
int heap_free(heap_t *heap, void *addr);


/**
 * @name heap_find_block
 * @brief 查找包含指定地址的内存块
 * 
 * @param
 *      heap - 堆管理器指针
 * @param
 *      addr - 目标地址
 * 
 * @return
 *      successful - 指向包含addr的mem_block指针
 *      failed - NULL(地址无效)
 */
mem_block_t *heap_find_block(heap_t *heap, void *addr);


/**
 * @name heap_find_free_block
 * @brief 查找满足大小要求的空闲块
 * 
 * @param
 *      heap - 堆管理器指针
 * @param
 *      size - 需求大小（已对齐）
 * 
 * @return
 *      successful - 指向合适的空闲块指针
 *      failed - NULL(没有足够空闲空间)
 * 
 * @attention
 *      - 使用 heap->alloc_strategy 指定的策略查找
 *      - 不分割块，仅返回指针
 */
mem_block_t *heap_find_free_block(heap_t *heap, size_t size);


/**
 * @name heap_merge_free_blocks
 * @brief 合并所有相邻的空闲块
 * 
 * @param
 *      heap - 堆管理器指针
 * @return
 *      成功合并的块数
 */
int heap_merge_free_blocks(heap_t *heap);


/**
 * @name heap_stats
 * @brief 获取堆的统计信息
 * 
 * @param
 *      heap - 堆管理器指针
 * @param
 *      allocated_out - 输出：已分配大小
 * @param
 *      free_out      - 输出：空闲大小
 * @param
 *      peak_out      - 输出：峰值分配
 * 
 * @return 
 *      0 - successful
 */
int heap_stats(heap_t *heap, size_t *allocated_out, size_t *free_out, size_t *peak_out);


/**
 * @name heap_cleanup
 * @brief 清理堆管理器
 * 
 * @param
 *      heap - 堆管理器指针     
 * 
 * @return 
 *      0  - successful
 *      <0 - failed
 * 
 * @attention
 *      - 释放所有分配的内存和元数据
 *      - 通常在程序退出时调用
 */
int heap_cleanup(heap_t *heap);


/**
 * @name heap_verify
 * @brief 验证堆的不变式
 * 
 * @param
 *      heap - 堆管理器指针
 * 
 * @return 
 *      0  - 堆状态正常
 *      -1 - 堆状态异常（可能已损坏）
 * 
 * @attention
 *      - 检查：
 *          - 所有块有序且不重叠
 *          - 已分配块和空闲块的集合不相交
 *          - 总大小一致性
 */
int heap_verify(heap_t *heap);


/**
 * @name heap_dump
 * @brief 转储堆的所有块信息
 *      - 用于调试和验证堆的内部状态
 * 
 * @param
 *      heap - 堆管理器指针
 */
void heap_dump(heap_t *heap);

#endif /* __HEAP_H__ */