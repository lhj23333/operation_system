#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "mem_block.h"

/* ========== 调试宏 ========== */

#ifdef DEBUG
    static uint32_t g_alloc_seq = 0;
    // static uint32_t g_free_seq = 0;

    static uint64_t _get_timestamp(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    }
#endif

/* ========== 公共接口实现 ========== */

mem_block_t *mem_block_create(uintptr_t start_addr, size_t size, mem_state_t state) {
    // 参数检查
    if (size == 0) {
        ERROR_PRINT("mem_block_create: invalid size (0)");
        return NULL;
    }

    if (!IS_ALIGNED(start_addr, ALIGN_SIZE)) {
        ERROR_PRINT("mem_block_create: addr 0x%lx not 8-byte aligned", start_addr);
        return NULL;
    }

    DEBUG_PRINT("mem_block_create: addr=0x%lx size=%zu state=%d",
        start_addr, size, state);

    // 申请元数据结构
    mem_block_t *block = (mem_block_t *)malloc(sizeof(mem_block_t));
    if (block == NULL) {
        ERROR_PRINT("mem_block_create: malloc failed");
        return NULL;
    }

    // 初始化元数据
    block->start_addr = start_addr;
    block->size = size;
    block->state = state;
    block->next = NULL;
    block->prev = NULL;
    
    #ifdef DEBUG
        // 初始化调试信息
        if (state == MEM_ALLOCATED) {
            block->alloc_seq = g_alloc_seq ++;
            block->alloc_timestamp = _get_timestamp();
            block->free_seq = 0;
            block->free_timestamp = 0;
            DEBUG_PRINT("mem_block_create: alloc_seq=%u", block->alloc_seq);
        } else {
            block->alloc_seq = 0;
            block->alloc_timestamp = 0;
            block->free_seq = 0;
            block->free_timestamp = 0;
        }
    #endif

    return block;
}

int mem_block_destory(mem_block_t *block) {
    if (block == NULL) {
        ERROR_PRINT("mem_block_destory: NULL block");
        return -1;
    }

    DEBUG_PRINT("mem_block_destory: addr=0x%lx size = %zu",
        block->start_addr, block->size);
    free(block);
    return 0;
} 

mem_block_t *mem_block_split(mem_block_t *block, size_t size) {
    // 验证参数有效性
    if (block == NULL || block->state != MEM_FREE) {
        ERROR_PRINT("mem_block_split: block not free or NULL");
        return NULL;
    }

    if (size == 0 || size >= block->size || !IS_ALIGNED(size, ALIGN_SIZE)) {
        ERROR_PRINT("mem_block_split: invalid size %zu for block size %zu",
            size, block->size);
        return NULL;
    }

    DEBUG_PRINT("mem_block_split: addr=0x%lx orig_size=%zu, size=%zu",
        block->start_addr, block->size, size);

    // 计算新块信息
    size_t new_block_size = block->size - size;
    uintptr_t new_block_addr = block->start_addr + size;

    // 创建新块
    mem_block_t *new_block = mem_block_create(new_block_addr, new_block_size, MEM_FREE);
    if (new_block == NULL) {
        ERROR_PRINT("mem_block_split: failed to create new block");
        return NULL;
    }

    // 更新链表状态
    new_block->next = block->next;
    new_block->prev = block;

    if (block->next != NULL) {
        block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = size;

    DEBUG_PRINT("mem_block_spilt: done, new_block addr=0x%lx, size=%zu",
        new_block->start_addr, new_block->size);

    return new_block;
}

mem_block_t *mem_block_merge(mem_block_t *block1, mem_block_t *block2) {
    if (block1 == NULL || block2 == NULL ||
        block1->state != MEM_FREE || block2->state != MEM_FREE) {
        ERROR_PRINT("mem_block_merge: blocks not free or NULL");
        return NULL;
    }

    // 验证相邻性
    if (!mem_block_is_adjacent(block1, block2)) {
        ERROR_PRINT("mem_block_merge: blocks not adjacent");
        return NULL;
    }

    // 计算合并块信息
    block1->size += block2->size;

    // 更新链表
    block1->next = block2->next;
    if (block2->next != NULL) {
        block2->next->prev = block1;
    }

    // 销毁 block2
    mem_block_destory(block2);

    DEBUG_PRINT("mem_block_merge: done, block1 new size=%zu", block1->size);

    return block1;
}

bool mem_block_is_adjacent(const mem_block_t *block1, const mem_block_t *block2) {
    if (block1 == NULL || block2 == NULL) return false;

    return (block1->start_addr + block1->size) == block2->start_addr;
}

bool mem_block_contains(const mem_block_t *block, uintptr_t addr) {
    if (block == NULL) return false;

    uintptr_t end_addr = block->start_addr + block->size;
    return addr >= block->start_addr && addr < end_addr;
}

bool mem_block_can_satisfy(const mem_block_t *block, size_t size) {
    if (block == NULL) return false;

    return block->state == MEM_FREE && block->size >= size;
}

void mem_block_dump(const mem_block_t *block) {
    if (block == NULL) {
        printf("[MemBlock] NULL\n");
        return ;
    }

    const char *state_str = (block->state == MEM_FREE) ? "FREE" : "ALLOCATED";

    printf("[MemBlock] addr=0x%lx size=%zu state=%s\n",
        block->start_addr, block->size, state_str);
    printf("           prev=%p next=%p\n",
        (void *)block->prev, (void *)block->next);

    #ifdef DEBUG
        if (block->state == MEM_ALLOCATED) {
            printf("           alloc_seq=%u\n", block->alloc_seq);
        }
    #endif
}

int mem_block_verify(const mem_block_t *block) {
    if (block == NULL) return -1;

    if (!IS_ALIGNED(block->start_addr, ALIGN_SIZE)) {
        ERROR_PRINT("mem_block_verify: addr 0x%lx not aligned",
            block->start_addr);
        return -1;
    }

    if (block->size == 0) {
        ERROR_PRINT("mem_block_verify: size is 0");
        return -1;
    }

    if (block->start_addr + block->size < block->start_addr) {
        ERROR_PRINT("mem_block_verify: address overflow");
        return -1;
    } 

    if (block->state != MEM_FREE && block->state != MEM_ALLOCATED) {
        ERROR_PRINT("mem_block_verify: invaild state %d", block->state);
        return -1;
    }
    return 0;
}