#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "allocator.h"
#include "heap.h"
#include "vmalloc.h"
#include "common.h"

/* ========== 全局变量 ========== */

/** 全局堆管理器 */
static heap_t *g_heap = NULL;

/** 初始化标志 */
static bool g_initialized = false;

/** 初始化互斥锁（保护初始化过程）*/
static pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;

/* ========== 内部辅助函数 ========== */

/** 确保分配器已初始化 */
static int _allocator_ensure_initialized(void) {
    if (g_initialized) return 0;

    pthread_mutex_lock(&g_init_lock);

    if (!g_initialized) {
        if (allocator_init(true) != 0) {
            pthread_mutex_unlock(&g_init_lock);
            ERROR_PRINT("_allocator_ensure_initialized: auto init failed");
            return -1;
        }
    }

    pthread_mutex_unlock(&g_init_lock);
    return 0;
}


/* ========== 公共接口实现 ========== */

void *myalloc(size_t size) {
    DEBUG_PRINT("myalloc: requesting %zu bytes", size);

    // 参数检查
    if (size == 0) {
        DEBUG_PRINT("myalloc: size is 0, returning NULL");
        return NULL;
    }

    // 确保初始化
    if (_allocator_ensure_initialized() != 0) {
        ERROR_PRINT("myalloc: initialization failed");
        return NULL;
    }

    void *ptr = heap_allocate(g_heap, size);
    if (ptr == NULL) {
        DEBUG_PRINT("myalloc: heap_allocate returned NULL");
    } else {
        DEBUG_PRINT("myalloc: allocated %zu bytes at %p", size, ptr);
    }

    return ptr;
}

int myfree(void *ptr) {
    if (ptr == NULL) {
        DEBUG_PRINT("myfree: ptr is NULL, returning 0");
        return 0;
    }

    DEBUG_PRINT("myfree: freeing ptr=%p", ptr);

    // 确保初始化
    if (_allocator_ensure_initialized() != 0) {
        ERROR_PRINT("myfree: allocator not initalized");
        return -1;
    }

    int result = heap_free(g_heap, ptr);
    if (result != 0) {
        ERROR_PRINT("myfree: heap_free failed for %p", ptr);
    } else {
        DEBUG_PRINT("myfree: successfully freed %p", ptr);
    }

    return result;
}

int allocator_init(bool enable_concurrency) {
    if (g_initialized) {
        DEBUG_PRINT("allocator_init: alread initialized");
        return 0;
    }

    DEBUG_PRINT("allocator_init: enable_concurrency:%d", enable_concurrency);

    // 初始化虚拟内存管理器
    if (vmalloc_init() != 0) {
        ERROR_PRINT("allocator_init: vmalloc_init failed");
        return -1;
    }

    DEBUG_PRINT("allocator_init: vmalloc initialized");

    // 初始化堆内存管理器
    g_heap = heap_init(POOL_INIT_SIZE, enable_concurrency);
    if (g_heap == NULL) {
        ERROR_PRINT("allocator_init: heap_init failed");
        vmalloc_cleanup();
        return -1;
    }

    DEBUG_PRINT("allocator_init: heap initialized with %u bytes",
        POOL_INIT_SIZE);

    // 设置初始化标志位
    g_initialized = true;

    DEBUG_PRINT("allocator_init: success");
    
    return 0;
}

int allocator_cleanup(void) {
    if (!g_initialized) {
        DEBUG_PRINT("allocator_cleanup: not initialized");
        return 0;
    }

    DEBUG_PRINT("allocator_cleanup: cleaning up");

    if (g_heap != NULL) {
        if (heap_cleanup(g_heap) != 0) {
            ERROR_PRINT("allocator_cleanup: heap_cleanup failed");
        }
        g_heap = NULL;
    }

    if (vmalloc_cleanup() != 0) {
        ERROR_PRINT("allocator_cleanup: vmalloc_cleanup failed");
    }

    g_initialized = false;

    DEBUG_PRINT("allocator_cleanup: done");

    return 0;
}

int allocator_stats(size_t *allocator_out, size_t *freed_out, size_t *peak_out) {
    if (!g_initialized || g_heap == NULL) {
        ERROR_PRINT("allocator_stats: allocator not initialized");
        return -1;
    }

    return heap_stats(g_heap, allocator_out, freed_out, peak_out);
}

void allocator_dump(void) {
    printf("\n");
    printf("=== Allocator State Dump ===\n");
    printf("Initialized: %s\n", g_initialized ? "yes" : "no");

    if (!g_initialized) {
        printf("(Allocator not initialized)\n");
        printf("=============================\n\n");
        return;
    }

    // 虚拟内存信息
    printf("\n--- Virtual Memory ---\n");
    vmalloc_dump();
    
    // 堆信息
    printf("\n--- Heap State ---\n");
    heap_dump(g_heap);

    // 统计信息
    size_t allocated, freed, peak;
    if (allocator_stats(&allocated, &freed, &peak) == 0) {
        printf("\n--- Statistics ---\n");
        printf("Allocated: %zu bytes\n", allocated);
        printf("Free: %zu bytes\n", freed);
        printf("Peak: %zu bytes\n", peak);
        printf("Total VM: %zu bytes\n", vmalloc_total_allocated());
    }

    // 验证
    printf("\n--- Verification ---\n");
    if (allocator_verify() == 0) {
        printf("Heap verify: OK ✓\n");
    } else {
        printf("Heap verify: FAILED ✗\n");
    }
    
    printf("\n=============================\n\n");
}

int allocator_verify(void) {
    if (!g_initialized || g_heap == NULL) {
        ERROR_PRINT("allocator_verify: allocator not initialized");
        return -1;
    }

    return heap_verify(g_heap);
}


