#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "heap.h"
#include "vmalloc.h"

/* ========== 内部辅助函数 ========== */

/** 加锁 */
static void _heap_lock(heap_t *heap) {
    if (heap && heap->lock_enabled) {
        pthread_mutex_lock(&heap->lock);
    }
}

/** 解锁 */
static void _heap_unlock(heap_t *heap) {
    if (heap && heap->lock_enabled) {
        pthread_mutex_unlock(&heap->lock);
    }
}

/** 尝试合并相邻的 FREE 块 */
static int _heap_try_merge_adjacent(mem_block_t *block) {
    if (block == NULL || block->state != MEM_FREE) return 0;

    int merge_count = 0;
    // 尝试与后继合并
    if (block->next != NULL && block->next->state == MEM_FREE) {
        mem_block_t *next = block->next;
        DEBUG_PRINT("_heap_try_merge_adjacent: merging with next block");
        if (mem_block_merge(block, next) != NULL) {
            merge_count ++;
        }
    }

    // 尝试与前驱合并
    if (block->prev != NULL && block->prev->state == MEM_FREE) {
        mem_block_t *prev = block->prev;
        DEBUG_PRINT("_heap_try_merge_adjacent: merging with prev block");
        if (mem_block_merge(prev, block) != NULL) {
            merge_count ++;
        }
    }

    return merge_count;
}

/* ========== 公共接口实现 ========== */

heap_t *heap_init(size_t initial_size, bool enable_lock) {
    if (initial_size == 0 || (initial_size % PAGE_SIZE) != 0) {
        ERROR_PRINT("heap_init: invalid initial_size %zu", initial_size);
        return NULL;
    }

    DEBUG_PRINT("heap_init: initial_size=%zu enable_lock=%d",
        initial_size, enable_lock);

    // 申请 heap_t 结构
    heap_t *heap = (heap_t *)malloc(sizeof(heap_t));
    if (heap == NULL) {
        ERROR_PRINT("heap_init: malloc failed for heap structure");
        return NULL;
    }

    // 调用 vmalloc 
    void *vm_ptr = vmalloc(NULL, initial_size);
    if(vm_ptr == NULL) {
        ERROR_PRINT("heap_init: vmalloc failed for %zu bytes", initial_size);
        free(heap);
        return NULL;
    }

    DEBUG_PRINT("heap_init: vmalloc succeeded, ptr=%p", vm_ptr);

    // 创建初始 FREE 块
    mem_block_t *init_block = mem_block_create(
        (uintptr_t)vm_ptr,
        initial_size,
        MEM_FREE
    );

    if(init_block == NULL) {
        ERROR_PRINT("heap_init: failed to create initial block");
        vmfree(vm_ptr, initial_size);
        free(heap);
        return NULL;
    }

    // 初始化 heap 字段
    heap->blocks_head = init_block;
    heap->block_count = 1;
    heap->total_allocated = 0;
    heap->total_free = initial_size;
    heap->peak_allocated = 0;
    heap->lock_enabled = enable_lock;
    heap->alloc_strategy = HEAP_STRATEGY_FIRST_FIT;

    // 初始化互斥锁
    if (heap->lock_enabled) {
        if (pthread_mutex_init(&heap->lock, NULL) != 0) {
            ERROR_PRINT("heap_init: pthread_mutex_init failed");
            mem_block_destory(init_block);
            vmfree(vm_ptr, initial_size);
            free(heap);
            return NULL;
        }
    } 

    DEBUG_PRINT("heap_init: success");
    return heap;
}

void *heap_allocate(heap_t *heap, size_t size) {
    if(heap == NULL || size == 0) {
        ERROR_PRINT("heap_allocate: invalid parameter");
        return NULL;
    }

    // 对齐大小
    size_t aligned_size = ALIGN_UP(size, ALIGN_SIZE);

    DEBUG_PRINT("heap_allocate: requesting %zu bytes (aligned to %zu)",
        size, aligned_size);

    // 开始进行分配内存操作
    _heap_lock(heap);

    // 查找空闲块
    mem_block_t *block = heap_find_free_block(heap, aligned_size);
    if (block == NULL) {
        // 如果未找到，尝试拓展推
        DEBUG_PRINT("heap_allocate: no free block found, extending heap");

        // 计算需要扩展的大小（至少 1 页）
        size_t extend_size = ALIGN_UP(aligned_size, PAGE_SIZE);

        void *extend_ptr = vmalloc(NULL, extend_size);
        if (extend_ptr == NULL) {
            ERROR_PRINT("heap_allocate: vmalloc expansion failed");
            _heap_unlock(heap);
            return NULL;
        }

        mem_block_t *new_block = mem_block_create(
            (uintptr_t)extend_ptr,
            extend_size,
            MEM_FREE
        );
        if (new_block == NULL) {
            ERROR_PRINT("heap_allocate: failed to create new block");
            vmfree(extend_ptr, extend_size);
            _heap_unlock(heap);
            return NULL;
        }

        mem_block_t *curr = heap->blocks_head;
        mem_block_t *prev_block = NULL;

        while (curr != NULL && curr->start_addr < (uintptr_t)extend_ptr) {
            prev_block = curr;
            curr = curr->next;
        }

        if (prev_block == NULL) {
            new_block->next = heap->blocks_head;
            if(heap->blocks_head != NULL) {
                heap->blocks_head->prev = new_block;
            }
            heap->blocks_head = new_block;
        } else {
            new_block->prev = prev_block;
            new_block->next = prev_block->next;
            prev_block->next = new_block;
            if (prev_block->next != NULL) {
                prev_block->next->prev = new_block;
            }
        }

        heap->block_count ++;
        heap->total_free += extend_size;

        block = new_block;
    }

    // 分割块（如需
    if(block->size > aligned_size) {
        DEBUG_PRINT("heap_allocate: splitting block (orig=%zu, need=%zu)",
            block->size, aligned_size);

        mem_block_t *split_result = mem_block_split(block, aligned_size);
        if (split_result == NULL) {
            ERROR_PRINT("heap_allocate: split failed");
            _heap_unlock(heap);
            return NULL;
        }
        heap->block_count ++;
    }

    // 更新堆内存管理信息
    block->state = MEM_ALLOCATED;

    heap->total_allocated += block->size;
    heap->total_free -= block->size;
    if(heap->total_allocated > heap->peak_allocated) {
        heap->peak_allocated = heap->total_allocated;
    }
    
    uintptr_t result_addr = block->start_addr;

    DEBUG_PRINT("heap_allocate: allocate %zu bytes at 0x%lx, state: %d",
        block->size, result_addr, block->state);

    _heap_unlock(heap);

    return (void *)result_addr;
}

int heap_free(heap_t *heap, void *addr) {
    if (heap == NULL || addr == NULL) {
        ERROR_PRINT("heap_free: invalid parameter");
        return -1;
    }

    DEBUG_PRINT("heap_free: releasing addr=%p", addr);

    _heap_lock(heap);
    
    // 查找块
    mem_block_t *block = heap_find_block(heap, addr);
    if(block == NULL) {
        ERROR_PRINT("heap_free: block not found at %p", addr);
        _heap_unlock(heap);
        return -1;
    }

    // 验证块已分配    
    if(block->state != MEM_ALLOCATED) {
        ERROR_PRINT("heap_free: double free at %p (state=%d)", addr, block->state);
        _heap_unlock(heap);
        return -1;
    }
    
    // 更新堆内存管理信息
    block->state = MEM_FREE;

    heap->total_allocated -= block->size;
    heap->total_free += block->size;
    // 尝试合并相邻块
    heap->block_count -= _heap_try_merge_adjacent(block);

    _heap_unlock(heap);
    return 0;
}

mem_block_t *heap_find_block(heap_t *heap, void *addr) {
    if (heap == NULL || addr == NULL) return NULL;

    uintptr_t addr_int = (uintptr_t)addr;
    mem_block_t *find_block = heap->blocks_head;
    
    while (find_block != NULL) {
        if (mem_block_contains(find_block, addr_int)) {
            return find_block;
        }
        find_block = find_block->next;
    }
    return NULL;
}

mem_block_t *heap_find_free_block(heap_t *heap, size_t size) {
    if(heap == NULL || size == 0) return NULL;

    mem_block_t *block = heap->blocks_head;
    mem_block_t *selected = NULL;

    switch (heap->alloc_strategy) {
        case HEAP_STRATEGY_FIRST_FIT:
            // 返回第一个足够大的 FREE 块
            while (block != NULL) {
                if(mem_block_can_satisfy(block, size)) {
                    return block;
                }
                block = block->next;
            }
            break;
        
        case HEAP_STRATEGY_BEST_FIT:
            // 返回最小的但足够大的 FREE 块
            while (block != NULL) {
                if (mem_block_can_satisfy(block, size)) {
                    if (selected == NULL || block->size < selected->size) {
                        selected = block;
                    }
                }
                block = block->next;
            }
            return selected;
            break;

        case HEAP_STRATEGY_WORST_FIT:
            // 返回最大的 FREE 块
            while (block != NULL) {
                if (mem_block_can_satisfy(block, size)) {
                    if (selected == NULL || block->size > selected->size) {
                        selected = block;
                    }
                }
                block = block->next;
            }
            return selected;
            break;

        default:
            ERROR_PRINT("heap_find_free_block: unknow strategy %d",
                heap->alloc_strategy);
    }
    return NULL;
}

int heap_merge_free_blocks(heap_t *heap) {
    if (heap == NULL) return 0;

    int merge_count = 0;
    mem_block_t *block = heap->blocks_head;

    while (block != NULL && block ->next != NULL) {
        if(block->state == MEM_FREE && block->next->state == MEM_FREE) {
            if(mem_block_is_adjacent(block, block->next)) {
                mem_block_t *next = block->next;
                mem_block_merge(block, next);
                heap->block_count --;
                merge_count ++;

                continue;       // 不移动 block 指针，继续检查下一个
            }
        }
        block = block->next;
    }
    return merge_count;
}

int heap_stats(heap_t *heap, size_t *allocated_out, size_t *free_out, size_t *peak_out) {
    if(heap == NULL) return -1;

    _heap_lock(heap);

    DEBUG_PRINT("heap_stats: total_allocate: %zu byte", heap->total_allocated);
    DEBUG_PRINT("heap_stats: free_out: %zu byte", heap->total_free);
    DEBUG_PRINT("heap_stats: peak_out: %zu byte", heap->peak_allocated);

    if(allocated_out) *allocated_out = heap->total_allocated;
    if(free_out) *free_out = heap->total_free;
    if(peak_out) *peak_out = heap->peak_allocated;

    _heap_unlock(heap);

    return 0;
}

void heap_dump(heap_t *heap) {
    if(heap == NULL) {
        ERROR_PRINT("heap dump: NULL heap");
        return ;
    }

    _heap_lock(heap);

    printf("=== Heap Dump ===\n");
    printf("Total blocks: %zu\n", heap->block_count);
    printf("Allocated: %zu bytes\n", heap->total_allocated);
    printf("Free: %zu bytes\n", heap->total_free);
    printf("Peak: %zu bytes\n", heap->peak_allocated);

    mem_block_t *block = heap->blocks_head;
    int i = 0;
    while(block != NULL) {
        const char *state_str = (block->state == MEM_FREE) ? "FREE" : "ALLOCATED";
        printf("[Block %d] addr=0x%lx size=%zu state=%s\n",
            i++, block->start_addr, block->size, state_str);
        block = block->next; 
    }

    printf("=================\n");
    
    _heap_unlock(heap);
    
    return ;
}


int heap_verify(heap_t *heap) {
    if(heap == NULL) return -1;

    _heap_lock(heap);

    mem_block_t *block = heap->blocks_head;
    size_t counted_allocated = 0;
    size_t counted_free = 0;
    size_t block_cout = 0;

    while (block != NULL) {
        // 块完整性检验
        if (mem_block_verify(block) != 0) {
            ERROR_PRINT("heap_verify: invalid block at 0x%lx", block->start_addr);
            _heap_unlock(heap);
            return -1;
        }

        // 地址递增检测
        if (block->prev != NULL && block->prev->start_addr >= block->start_addr) {
            ERROR_PRINT("heap_verify: blocks not in order");
            _heap_unlock(heap);
            return -1;
        }

        // // 地址连续检测
        // if(block->prev != NULL) {
        //     if (block->prev->start_addr + block->prev->size != block->start_addr) {
        //         ERROR_PRINT("heap_verify: gap between blocks");
        //         _heap_unlock(heap);
        //         return -1;
        //     }
        // }
        
        // 不存在相邻 FREE 块
        if (block->state == MEM_FREE && block->next != NULL && 
            block->next->state == MEM_FREE) {
            if (mem_block_is_adjacent(block, block->next)) {
                ERROR_PRINT("heap_verify: adjacent FREE blocks");
                _heap_unlock(heap);
                return -1;
            }
        }

        if (block->state == MEM_FREE) counted_free += block->size;
        else counted_allocated += block->size;
        
        block_cout ++;
    
        block = block->next;
    }

    // 块计数一致检验
    if (block_cout != heap->block_count) {
        ERROR_PRINT("heap_verify: block count mismath");
        _heap_unlock(heap);
        return -1;
    }

    // 统计信息一致检验
    if (counted_allocated != heap->total_allocated || 
        counted_free != heap->total_free) {
        ERROR_PRINT("heap_verify: statistics mismath");
        _heap_unlock(heap);
        return -1;
    }

    _heap_unlock(heap);

    DEBUG_PRINT("heap_verify: OK");

    return 0;
}


int heap_cleanup(heap_t *heap) {
    if (heap == NULL) return -1;

    DEBUG_PRINT("heap_cleanup: cleaning up heap");

    _heap_lock(heap);

    // 释放所有内存块
    mem_block_t *block = heap->blocks_head;
    while (block != NULL) {
        mem_block_t *next = block->next;
        vmfree((void *)block->start_addr, block->size);
        mem_block_destory(block);
        
        block = next;
    }

    heap->blocks_head = NULL;
    heap->block_count = 0;
    heap->total_allocated = 0;
    heap->total_free = 0;
    heap->peak_allocated = 0;   

    _heap_unlock(heap);

    // 摧毁互斥锁
    if(heap->lock_enabled) {
        pthread_mutex_destroy(&heap->lock);
    }
    free(heap);

    return 1;
}
