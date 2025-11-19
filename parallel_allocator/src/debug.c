/*
 * debug.c - 调试和诊断工具实现
 */

#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "allocator.h"
#include "vmalloc.h"

/* ========== 分配追踪 ========== */

typedef struct {
    char *file;
    int line;
    size_t size;
    void *ptr;
    int is_alloc;  // 1 = alloc, 0 = free
} alloc_record_t;

static bool g_tracking_enabled = false;
static alloc_record_t *g_records = NULL;
static size_t g_record_count = 0;
static size_t g_record_capacity = 1000;

/**
 * debug_report_leak - 报告内存泄漏
 */
size_t debug_report_leak(void) {
    size_t allocated, freed, peak;
    
    if (allocator_stats(&allocated, &freed, &peak) != 0) {
        fprintf(stderr, "Error: allocator not initialized\n");
        return 0;
    }
    
    if (allocated > 0) {
        fprintf(stderr, "[LEAK] %zu bytes not freed (peak was %zu)\n",
               allocated, peak);
        return allocated;
    }
    
    fprintf(stdout, "[OK] No memory leaks detected\n");
    return 0;
}

/**
 * debug_print_memory_layout - 打印内存布局
 */
void debug_print_memory_layout(void) {
    printf("\n=== Memory Layout Visualization ===\n\n");
    
    allocator_dump();
}

/**
 * debug_check_consistency - 检查一致性
 */
int debug_check_consistency(void) {
    printf("\nChecking allocator consistency...\n");
    
    if (allocator_verify() == 0) {
        printf("✓ All consistency checks passed\n\n");
        return 0;
    } else {
        printf("✗ Consistency check failed!\n\n");
        return 1;
    }
}

/**
 * debug_enable_allocation_tracking - 启用追踪
 */
void debug_enable_allocation_tracking(void) {
    if (g_tracking_enabled) {
        return;
    }
    
    g_records = (alloc_record_t *)malloc(sizeof(alloc_record_t) * g_record_capacity);
    if (g_records == NULL) {
        fprintf(stderr, "Failed to allocate tracking buffer\n");
        return;
    }
    
    g_tracking_enabled = true;
    g_record_count = 0;
    
    printf("Allocation tracking enabled\n");
}

/**
 * debug_disable_allocation_tracking - 禁用追踪
 */
void debug_disable_allocation_tracking(void) {
    if (!g_tracking_enabled) {
        return;
    }
    
    g_tracking_enabled = false;
    
    if (g_records != NULL) {
        free(g_records);
        g_records = NULL;
    }
    
    printf("Allocation tracking disabled\n");
}

/**
 * debug_print_allocation_trace - 打印追踪
 */
void debug_print_allocation_trace(void) {
    if (!g_tracking_enabled || g_records == NULL) {
        printf("Allocation tracking not enabled\n");
        return;
    }
    
    printf("\n=== Allocation Trace ===\n");
    printf("Total records: %zu\n\n", g_record_count);
    
    for (size_t i = 0; i < g_record_count; i++) {
        alloc_record_t *rec = &g_records[i];
        const char *op = rec->is_alloc ? "ALLOC" : "FREE";
        printf("[%zu] %s %zu bytes → %p (%s:%d)\n",
               i, op, rec->size, rec->ptr, rec->file, rec->line);
    }
    
    printf("\n");
}