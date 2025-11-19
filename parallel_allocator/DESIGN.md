# 并行内存分配器 - 详细设计文档

## 目录

1. [系统概述](#系统概述)
2. [架构设计](#架构设计)
3. [核心数据结构](#核心数据结构)
4. [算法设计](#算法设计)
5. [线程安全](#线程安全)
6. [验证和测试](#验证和测试)

---

## 系统概述

### 需求

实现一个满足以下要求的内存分配器：

1. **原子性**: 多处理器并发访问时的正确性
2. **无重叠**: 分配块互不重叠
3. **8字节对齐**: 返回地址低3位为0
4. **无泄漏**: 完整的回收和重用
5. **错误处理**: 无足够内存时返回 NULL
6. **未初始化**: 返回的内存未初始化

### 约束

- 只能使用 `vmalloc/vmfree` 申请/释放大块虚拟内存
- 不能使用其他库函数（除 POSIX 标准库）
- 必须支持多线程

---

## 架构设计

### 分层模型

```
Layer 4: allocator    (公共接口)
    ↓
Layer 3: heap         (堆管理)
    ↓
Layer 2: mem_block    (块管理)
    ↓
Layer 1: vmalloc      (虚拟内存)
    ↓
Linux Kernel (mmap/munmap)
```

### 职责划分

| 层 | 模块 | 职责 | 关键结构 |
|----|------|------|---------|
| 4 | allocator | 公共接口、全局管理 | - |
| 3 | heap | 分配策略、块合并、线程安全 | heap_t |
| 2 | mem_block | 块元数据、分割/合并 | mem_block_t |
| 1 | vmalloc | 系统调用包装、区间追踪 | vm_region_t |

---

## 核心数据结构

### 1. vm_region_t (虚拟内存区域)

```c
typedef struct vm_region {
    uintptr_t start_addr;      // 起始地址
    size_t length;             // 长度（4096 倍数）
    int prot_flags;            // 保护标志
    int map_flags;             // 映射标志
    struct vm_region *next;    // 链表指针
} vm_region_t;
```

**特点**:
- 追踪通过 vmalloc 申请的每个大块内存
- 组织成单向链表

### 2. mem_block_t (内存块)

```c
typedef struct mem_block {
    uintptr_t start_addr;      // 块的起始地址
    size_t size;               // 块的大小
    mem_state_t state;         // FREE 或 ALLOCATED
    struct mem_block *prev;    // 前驱指针
    struct mem_block *next;    // 后继指针
} mem_block_t;
```

**设计原则**:
- 元数据与用户数据分离
- 所有块按地址有序维护
- 块之间无间隙（地址连续）
- 不存在相邻的 FREE 块

### 3. heap_t (堆管理器)

```c
typedef struct {
    mem_block_t *blocks_head;   // 块链表头
    size_t block_count;         // 块总数
    size_t total_allocated;     // 已分配大小
    size_t total_free;          // 空闲大小
    size_t peak_allocated;      // 峰值
    pthread_mutex_t lock;       // 互斥锁
    bool lock_enabled;          // 锁启用标志
    int alloc_strategy;         // 分配策略
} heap_t;
```

---

## 算法设计

### 1. 分配算法 (Allocation)

#### 首适配 (First Fit)

```
输入: size (要分配的大小)
输出: address (分配的地址)

1. 对齐大小: aligned_size = ALIGN_UP(size, 8)
2. 遍历块链表，找第一个:
   - state == FREE
   - size >= aligned_size
3. 如果找到:
   a. 如果块大小 > aligned_size，分割块
   b. 标记为 ALLOCATED
   c. 更新统计信息
   d. 返回块地址
4. 如果未找到:
   a. 调用 vmalloc 扩展堆
   b. 创建新的 FREE 块
   c. 递归分配
5. 返回 NULL (失败)

时间复杂度: O(n)，其中 n 是块数
```

#### 最佳适配 (Best Fit)

```
找大小最接近 aligned_size 的 FREE 块
时间复杂度: O(n)，但碎片更少
```

### 2. 释放算法 (Deallocation)

```
输入: ptr (要释放的地址)
输出: 0 (成功) / -1 (失败)

1. 查找包含 ptr 的块
2. 验证块状态为 ALLOCATED
3. 标记为 FREE
4. 与前驱块合并（如果前驱是 FREE）
5. 与后继块合并（如果后继是 FREE）
6. 更新统计信息
7. 返回 0

时间复杂度: O(n)
```

### 3. 块分割算法 (Split)

```
输入: block (要分割的 FREE 块), size (前半部分大小)
输出: 指向新块的指针

前置条件:
  - block.state == FREE
  - size > 0 且 size < block.size
  - size 8 字节对齐

步骤:
1. 创建新块:
   new_block.start = block.start + size
   new_block.size = block.size - size
   new_block.state = FREE
2. 链接到链表:
   new_block.prev = block
   new_block.next = block.next
   block.next = new_block
3. 更新原块:
   block.size = size
4. 返回 new_block

结果:
  block: [start, start+size), FREE
  new_block: [start+size, start+block.size), FREE
```

### 4. 块合并算法 (Merge)

```
输入: block1 (前块), block2 (后块，必须相邻且都是 FREE)
输出: 指向合并后块的指针

前置条件:
  - block1.state == FREE && block2.state == FREE
  - block1.start + block1.size == block2.start

步骤:
1. 合并大小:
   block1.size += block2.size
2. 更新链表:
   block1.next = block2.next
   if (block2.next) block2.next.prev = block1
3. 销毁 block2
4. 返回 block1

结果:
  block1: [block1.start, block1.start + merged_size), FREE
  block2 被销毁
```

---

## 线程安全

### 全局锁策略

```
heap_t 中包含 pthread_mutex_t 锁

所有修改堆状态的操作都要原子化:
- heap_allocate()
- heap_free()
- heap_merge_free_blocks()

操作模式:
┌──────────────────────────────┐
│ lock(&heap->lock)            │
│   临界区代码                 │
│   - 修改 blocks_head         │
│   - 修改统计信息             │
│   - 块状态转换               │
│ unlock(&heap->lock)          │
└──────────────────────────────┘
```

### 死锁预防

- 全局只有一个锁（无嵌套）
- 锁的持有时间尽量短（仅覆盖临界区）
- 不在持有锁时调用可能阻塞的函数

### 初始化线程安全

```c
static pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;

// 双检查锁定（Double-Checked Locking）
if (!g_initialized) {
    pthread_mutex_lock(&g_init_lock);
    if (!g_initialized) {
        // 执行初始化
        g_initialized = true;
    }
    pthread_mutex_unlock(&g_init_lock);
}
```

---

## 验证和测试

### 堆不变式检查

```
heap_verify() 检查以下不变式：

1. 块按地址严格递增:
   for each block in heap:
     if (block.prev):
       assert(block.prev.start < block.start)

2. 块之间无间隙:
   for each block in heap:
     if (block.prev):
       assert(block.prev.start + block.prev.size == block.start)

3. 无相邻 FREE 块:
   for each block in heap:
     if (block.state == FREE && block.next.state == FREE):
       assert(fail)  // 应该已合并

4. 统计一致:
   assert(sum_allocated == heap.total_allocated)
   assert(sum_free == heap.total_free)
```

### 测试策略

#### 单元测试

- vmalloc/vmfree 功能测试
- mem_block 分割/合并测试
- heap 分配/释放测试
- allocator 接口测试

#### 集成测试

- 复杂分配序列
- 碎片管理
- 内存重用

#### 并发测试

- 多线程分配/释放
- 竞态条件检测
- 性能基准测试

#### 工具验证

- Valgrind 内存检测
- ThreadSanitizer 竞态检测
- Cppcheck 代码分析

---

## 性能考虑

### 时间复杂度

| 操作 | 平均情况 | 最坏情况 |
|------|---------|---------|
| myalloc (首适配) | O(n) | O(n) |
| myfree | O(n) | O(n) |
| 块合并 | O(1) | O(1) |

其中 n 是块的数量

### 空间复杂度

| 结构 | 空间 |
|------|------|
| 每个块元数据 | ~48 字节 |
| 堆管理器 | ~64 字节 |
| 虚拟内存管理 | ~32 字节/区间 |

### 优化建议

1. **红黑树替代链表** → O(log n) 查找
2. **分离 free list** → O(1) 找到 FREE 块
3. **内存池** → 加速元数据分配
4. **NUMA 感知** → 提高多 socket 系统性能
5. **分配预热** → 减少缺页中断

---

## 已知限制

1. **单堆设计**: 所有线程共享一个全局堆
   - 优点: 简单、好实现
   - 缺点: 锁竞争，可能成为性能瓶颈
   
2. **首适配策略**: 默认使用首适配
   - 优点: 速度快
   - 缺点: 碎片可能多

3. **无超大页支持**: 仅支持 4K 页面
   
4. **无 NUMA 感知**: 不考虑 NUMA 亲和性

---

## 未来改进方向

- [ ] 每线程堆 (Per-Thread Heap)
- [ ] 红黑树数据结构
- [ ] 大页支持
- [ ] NUMA 感知分配
- [ ] 统计信息持久化
- [ ] 可配置的分配策略
