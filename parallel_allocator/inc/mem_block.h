/*
 * mem_block.h - 内存块结构定义
 * 
 * 职责：
 * 1. 定义单个内存块的元数据结构
 * 2. 提供块操作接口：创建、销毁、分割、合并
 * 3. 维护块的生命周期
 * 
 * 设计理念：
 * - 每个块由 mem_block 结构表示
 * - 所有块按地址有序链接形成一个双向链表
 * - 块有两种状态：已分配(ALLOCATED)、空闲(FREE)
 * - 块的不变式：相邻的空闲块必须已合并（不存在相邻的两个FREE块）
 */


#ifndef __MEM_BLOCK_H__
#define __MEM_BLOCK_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "common.h"

/* ========== 内存块状态枚举 ========== */

typedef enum {
    MEM_FREE = 0,       // 空闲状态
    MEM_ALLOCATED = 1,  // 已分配状态
} mem_state_t;

/* ========== 内存块元数据结构 ========== */

typedef struct mem_block {
    uintptr_t start_addr;       // 块起始地址
    size_t size;                // 块大小（包括对齐）
    mem_state_t state;          // 块状态（已分配或空闲）          

    /* 链表指针 - 用于维护有序的块列表 */
    struct mem_block *prev;
    struct mem_block *next;

    /* 红黑树实现 */
    // struct mem_block *left;
    // struct mem_block *right;
    // int color;  /* for red-black tree */

    #ifdef DEBUG
        uint32_t alloc_seq;         // 分配序列号（用于追踪）
        uint64_t alloc_timestamp;   // 分配时间戳
        uint32_t free_seq;          // 释放序列号
        uint64_t free_timestamp;    // 释放时的时间戳
    #endif
} mem_block_t;


/* ========== 内存块操作接口 ========== */
/** 
 * @name mem_block_create
 * @brief 创建新的内存块元数据
 * 
 * @param
 *      start_addr - 块的起始地址
 * @param
 *      size - 块大小
 * @param
 *      state - 块初始状态
 * 
 * @return
 *      successful - 指向新创建的 mem_block 结构的指针
 *      failed - NULL
 * 
 * @attention
 *      该函数申请的元数据本身通过 vmalloc 管理
 */
mem_block_t *mem_block_create(uintptr_t start_addr, size_t size, mem_state_t state);


/** 
 * @name mem_block_destory
 * @brief 销毁内存块元数据
 * 
 * @param 
 *      block - 要销毁的内存块指针
 * 
 * @return
 *      0  - successful
 *      -1 - failed
 */
int mem_block_destory(mem_block_t *block);


/**
 * @name mem_block_split
 * @brief 分割内存块
 *      - 将一个空闲块分割成两个块：原块保留前半部分，新块保留后半部分
 * 
 * @param
 *      block - 要分割的空闲块指针
 * @param
 *      size  - 前半部分的大小
 * 
 * @return
 *      successful - 指向新分割出的块的指针
 *      failed - NULL
 */
mem_block_t *mem_block_split(mem_block_t *block, size_t size);


/**
 * @name mem_block_merge
 * @brief 合并两个相邻的空闲块
 *      - 将两个相邻的空闲块合并为一个更大的块
 *      - 后块的内容合并到前块
 * 
 * @param
 *      block1 - 前块指针
 * @param
 *      block2 - 后块指针
 * 
 * @return
 *      successful - 指向合并后的块指针
 *      failed - NULL
 * 
 * @attention
 *      - 前置条件：
 *          - block1->state == MEM_FREE && block2->state == MEM_FREE
 *          - block1->start_addr + block1->size == block2->start_addr
 * 
 *      - 后置条件：
 *          - block1->size = 原block1->size + 原block2->size
 *          - block2 被从链表中移除并销毁
 */
mem_block_t *mem_block_merge(mem_block_t *block1, mem_block_t *block2);


/**
 * @name mem_block_is_adjacent
 * @brief 检查两个块是否相邻
 * 
 * @param
 *      block1 - 块1指针
 * @param
 *      block2 - 块2指针
 * @return
 *      true  - 块1与块2相邻（地址连续）
 *      false - 不相邻
 */
bool mem_block_is_adjacent(const mem_block_t *block1, const mem_block_t *block2);


/**
 * @name mem_block_contains
 * @brief 检查块是否包含指定地址
 * 
 * @param
 *      block - 内存块指针
 * @param
 *      addr - 内存地址
 * 
 * @return 
 *      true  - 地址在块范围内
 *      false - 地址不在块范围内
 */
bool mem_block_contains(const mem_block_t *block, uintptr_t addr);


/**
 * @name mem_block_can_satisfy
 * @brief 检查块是否能满足分配需求
 * 
 * @param
 *      block - 要检查的块
 * @param
 *      size - 需要的大小
 * 
 * @return
 *      true  - 块是空闲且大小 >= size
 *      false - 块已分配或太小
 */
bool mem_block_can_satisfy(const mem_block_t *block, size_t size);


/**
 * @name mem_block_dump
 * @brief 转储单个块信息 - 用于调试
 * 
 * @param
 *      block - 所要转储块指针
 */
void mem_block_dump(const mem_block_t *block);


/** 
 * @name mem_block_verify
 * @brief 验证单个块的完整性
 * 
 * @return 
 *      0 - 块状态正常
 *      1 - 块状态异常（可能已损坏）
 */
int mem_block_verify(const mem_block_t *block);

#endif /* __MEM_BLOCK_H__ */