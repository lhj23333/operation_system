/*
 * debug.h - 调试和诊断工具
 * 
 * 提供高级的诊断功能，帮助开发者理解和调试内存分配器
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stddef.h>

/* ========== 诊断功能 ========== */

/**
 * debug_report_leak - 报告内存泄漏
 * 
 * 检查当前已分配的内存，如果非零则报告泄漏
 * 
 * 返回值：
 *   0 - 没有泄漏（所有内存都已释放）
 *   > 0 - 泄漏的字节数
 */
size_t debug_report_leak(void);

/**
 * debug_print_memory_layout - 打印内存布局
 * 
 * 以可视化方式打印所有已分配和空闲块
 */
void debug_print_memory_layout(void);

/**
 * debug_check_consistency - 检查一致性
 * 
 * 执行各种一致性检查，返回发现的错误数量
 */
int debug_check_consistency(void);

/**
 * debug_enable_allocation_tracking - 启用分配追踪
 * 
 * 记录每次 myalloc/myfree 调用，便于调试
 */
void debug_enable_allocation_tracking(void);

/**
 * debug_disable_allocation_tracking - 禁用分配追踪
 */
void debug_disable_allocation_tracking(void);

/**
 * debug_print_allocation_trace - 打印分配追踪
 * 
 * 输出所有记录的 myalloc/myfree 调用
 */
void debug_print_allocation_trace(void);

#endif /* __DEBUG_H__ */