/*
 * vmalloc.h - 虚拟内存分配接口
 * 
 * 该文件定义了对 mmap/munmap 的封装，提供虚拟内存管理的基础设施。
 * 
 * 使用场景：
 * - 分配大块连续的虚拟内存
 * - 释放虚拟内存区间
 * - 追踪虚拟内存使用情况
 */

#ifndef __VMALLOC_H__
#define __VMALLOC_H__

#include <stddef.h>
#include <stdint.h>
#include "common.h"

/* ========== 虚拟内存区域定义 ========== */

typedef struct vm_region{
    uintptr_t start_addr;       // 区间起始地址
    size_t length;              // 区间长度（必须是 PAGE_SIZE 倍数）
    int prot_flags;             // 保护标志位（PROT_READ | PROT_WRITE)
    int map_flags;              // 映射标志 (MAP_PRIVATE | MAP_ANONYMOUS | MAP_SHARED)
    struct vm_region *next;     // 链表指针
} vm_region_t;

/* ========== 虚拟内存管理器 ========== */

typedef struct {
    vm_region_t *regions_head;  // 已分配区间链表
    size_t total_allocated;     // 总分配大小
    size_t region_count;        // 区间数量
} vm_manager_t;


/* ========== 虚拟内存管理器接口 ========== */
/**
 * @name vmalloc_init 
 * @brief 初始化虚拟内存管理器
 * @return
 *      0 - sucessful
 *      <0 - failed (见 error_code_t)
 */
int vmalloc_init(void);


/**
 * @name vmalloc
 * @brief 申请虚拟内存区间
 * @param
 *      addr   - 期望的地址（NULL 表示由 OS 自动分配）
 * @param     
 *      length - 申请大小，必须是 PAGE_SIZE(4096) 的倍数
 * @return
 *      successful - 分配的内存起始地址
 *      failed     - NULL (errno 设置为对应错误)  
 */
void *vmalloc(void *addr, size_t length);


/**
 * @name vmfree
 * @brief 释放虚拟内存区间
 * @param
 *      addr - 要释放的内存起始地址
 * @param 
 *      length - 要释放的大小，必须与 vmalloc 时一致 
 * @return
 *      0 - successful 
 *      1 - failed (errno 设置为对应错误)  
 */
int vmfree(void *addr, size_t length);


/** 
 * @name vmalloc_total_allocated 
 * @brief 获取总分配大小
 * @return
 *      已分配的总虚拟内存大小（byte）
 */
size_t vmalloc_total_allocated(void);


/**
 * @name vmalloc_region_count
 * @brief 获取区间数量
 * @return
 *      当前虚拟内存管理器中的区间数量 
 */
size_t vmalloc_region_count(void);


/**
 * @name vmalloc_cleanup(void);
 * @brief 清理虚拟内存管理器
 *      - 释放所有已分配的虚拟内存和元数据
 *      - 通常在程序退出时调用
 * 
 * @return
 *      0 - successful
 *      <0 - failed
 */
int vmalloc_cleanup(void);


/** 
 * @name vmalloc_dump
 * @brief 诊断函数：转储存所有虚拟内存区间信息
 *      - 用于调试验证虚拟内存状态
 */
void vmalloc_dump(void);


#endif /* __VMALLOC_H__ */