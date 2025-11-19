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

### 分层模型（4层模型）

```
┌──────────────────────────────────────────────────────────────┐
│                 第4层：公共接口层 (Allocator)                 │
│  void *myalloc(size_t size)  /  int myfree(void *ptr)        │
│  allocator_init()  /  allocator_cleanup()                    │
│                  ↓ 调用           ↑ 返回结果                  │
├──────────────────────────────────────────────────────────────┤
│        第3层：堆管理层 (Heap) - 核心业务逻辑                  │
│  heap_init()  /  heap_allocate()  /  heap_free()             │
│  heap_merge_free_blocks()  /  heap_verify()                  │
│  ├─ 维护内存块链表（有序、无重叠）                           │
│  ├─ 实现分配策略（首适配/最佳适配/最差适配）                 │
│  ├─ 执行块分割和合并                                         │
│  ├─ 管理统计信息                                             │
│  └─ 线程安全（互斥锁保护）                                   │
│                  ↓ 操作             ↑ 返回块指针             │
├──────────────────────────────────────────────────────────────┤
│     第2层：内存块元数据层 (MemBlock) - 数据操作               │
│  mem_block_create()  /  mem_block_split()                    │
│  mem_block_merge()  /  mem_block_verify()                    │
│  ├─ 管理单个块的元数据（地址、大小、状态）                   │
│  ├─ 块的分割操作                                             │
│  ├─ 块的合并操作                                             │
│  ├─ 块的查找和验证                                           │
│  └─ 维护双向链表结构                                         │
│                  ↓ 读写             ↑ 实际地址              │
├──────────────────────────────────────────────────────────────┤
│    第1层：虚拟内存管理层 (VMalloc) - 系统接口                │
│  vmalloc()  /  vmfree()  /  vmalloc_cleanup()                │
│  ├─ 直接调用 mmap/munmap 系统调用                            │
│  ├─ 返回 4096 字节对齐的大块虚拟内存                         │
│  ├─ 追踪已分配的虚拟内存区间                                 │
│  └─ 提供诊断函数（vmalloc_dump）                            │
│                  ↓ 申请              ↑ 虚拟地址             │
├──────────────────────────────────────────────────────────────┤
│                  Linux 内核 (mmap/munmap)                    │
└──────────────────────────────────────────────────────────────┘
```

### 数据结构关系图

```
┌─────────────────────────────────────────────────────────────┐
│                    全局状态                                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  g_allocator (全局分配器指针)                               │
│         ↓                                                    │
│  ┌─ heap_t (全局堆管理器)                                   │
│  │  ├─ blocks_head → mem_block 链表头                       │
│  │  ├─ block_count = 5                                      │
│  │  ├─ total_allocated = 3072                               │
│  │  ├─ total_free = 37888                                   │
│  │  ├─ lock (pthread_mutex_t)                               │
│  │  └─ alloc_strategy = FIRST_FIT                           │
│  │                                                           │
│  └─ vm_manager (全局虚拟内存管理器)                         │
│     ├─ regions_head → vm_region 链表头                      │
│     │  ├─ [Region 0] addr=0x7f..., size=40960 (10页)       │
│     │  ├─ [Region 1] addr=0x7g..., size=81920 (20页)       │
│     │  └─ ...                                               │
│     └─ total_allocated = 122880                             │
│                                                              │
└─────────────────────────────────────────────────────────────┘

mem_block 链表结构：

  NULL ← [Block 0] ↔ [Block 1] ↔ [Block 2] ↔ [Block 3] → NULL
          (ALLOCATED)  (FREE)    (ALLOCATED)  (FREE)

  每个 mem_block_t:
  ┌──────────────────────────────┐
  │ start_addr: 0x10000000       │
  │ size: 1024                   │
  │ state: ALLOCATED             │
  │ prev: 指向前驱               │
  │ next: 指向后继               │
  └──────────────────────────────┘

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
## 场景示例：

### 最简单分配过程：

```
第1步：用户代码调用 myalloc(1024)
│
├─→ allocator.c: myalloc(size_t size)
│   参数：size = 1024
│
├─→ 检查分配器是否初始化
│   如未初始化，自动调用 allocator_init(1)
│
├─→ allocator.c: heap_allocate(g_heap, 1024)
│   调用堆管理器进行实际分配
│
├─→ heap.c: heap_allocate(heap_t *heap, size_t size)
│   参数：heap = g_heap, size = 1024
│
│   步骤1：对齐大小
│   aligned_size = ALIGN_UP(1024, 8) = 1024  （1024已对齐）
│
│   步骤2：加锁
│   pthread_mutex_lock(&heap->lock)
│   （确保多线程安全）
│
│   步骤3：查找空闲块
│   free_block = heap_find_free_block(heap, 1024)
│   遍历 blocks_head 链表，找第一个大小 >= 1024 且状态为 FREE 的块
│
│   假设找到：
│   Block A: [0x10000000, 2048), FREE
│
│   步骤4：分割块
│   mem_block_split(Block A, 1024)
│   
│   结果：
│   Block A: [0x10000000, 1024), FREE   ← 前半部分
│   Block B: [0x10000400, 1024), FREE   ← 后半部分（新块）
│
│   步骤5：标记分配
│   Block A->state = ALLOCATED
│
│   步骤6：更新统计
│   heap->total_allocated += 1024
│   heap->total_free -= 1024
│   heap->peak_allocated = max(peak, total_allocated)
│
│   步骤7：解锁
│   pthread_mutex_unlock(&heap->lock)
│
├─→ 返回分配地址
│   result = 0x10000000
│
└─→ 用户获得指针
    ptr = 0x10000000
    可以开始使用这块内存
```

#### 数据变化过程：

```
初始状态（申请前）：
┌────────────────────────────────────┐
│ Block: [0x10000000, 2048), FREE    │  ← 一个大的空闲块
└────────────────────────────────────┘

申请 myalloc(1024) 后：
┌────────────────────────────────────┐
│ Block A: [0x10000000, 1024), ALLOC │  ← 被分配的部分
├────────────────────────────────────┤
│ Block B: [0x10000400, 1024), FREE  │  ← 剩余的空闲部分
└────────────────────────────────────┘

内存布局（虚拟内存中）：
   0x10000000       0x10000400       0x10000800
   ↓                ↓                ↓
   ┌────────────┬────────────┐
   │ ALLOCATED  │    FREE    │
   │  1024B     │   1024B    │
   └────────────┴────────────┘
   ↑                        ↑
   ptr 指向这里             还可用的空间
```

### 堆内存不足的分配过程

```
第1步：用户调用 myalloc(50000)

第2步：进入 heap_allocate(heap, 50000)
│
│   对齐：aligned_size = 50000
│
│   查找空闲块：heap_find_free_block(heap, 50000)
│   ▼
│   遍历链表：
│   Block 0: size=1024, state=ALLOCATED  ✗ 不行
│   Block 1: size=1024, state=FREE       ✗ 太小
│   Block 2: ...                         ✗ 都太小或已分配
│   Block N: ...                         ✗ 没找到
│   
│   结果：free_block = NULL （内存不足）
│
├─→ 需要扩展堆（第一次 vmalloc 调用）
│
├─→ vmalloc.c: vmalloc(NULL, extend_size)
│   extend_size = ALIGN_UP(50000, 4096) = 53248 字节
│   
│   调用 mmap 系统调用：
│   mmap(NULL,                       // 由OS选择地址
│         53248,                     // 大小
│         PROT_READ | PROT_WRITE,    // 可读写
│         MAP_PRIVATE | MAP_ANONYMOUS,  // 私有匿名
│         -1,                        // 无文件关联
│         0)                         // 无偏移
│   
│   ▼ 系统调用进入内核
│   内核分配虚拟内存页表
│   ▼ 返回分配地址
│   result = 0x7f1234567000
│   
├─→ 记录到 vm_manager
│   创建 vm_region 结构
│   region->start_addr = 0x7f1234567000
│   region->length = 53248
│   region->next = 旧的 regions_head
│   g_vm_manager->regions_head = region
│   g_vm_manager->total_allocated += 53248
│
├─→ 在堆中创建新块
│   new_block = mem_block_create(
│       0x7f1234567000,
│       53248,
│       MEM_FREE
│   )
│
├─→ 添加到堆的块链表
│   （插入到链表末尾，保持地址有序）
│   ...existing blocks... ↔ [new_block]
│   heap->block_count++
│   heap->total_free += 53248
│
├─→ 重新查找空闲块
│   free_block = new_block （53248, FREE）
│
├─→ 分割块
│   mem_block_split(new_block, 50000)
│   
│   结果：
│   allocated_part: [0x7f1234567000, 50000), ALLOCATED
│   free_part:      [0x7f1234568c80, 3248),  FREE
│
├─→ 标记、更新统计、解锁
│
└─→ 返回 0x7f1234567000
```

#### 虚拟内存和堆块的关系：

```
虚拟内存池（通过 vmalloc 申请）：

vmalloc(NULL, 40960) → 0x10000000
┌─────────────────────────────────────────────┐
│   40960 字节虚拟内存（10 页）                │
│   0x10000000 ~ 0x1000a000                   │
└─────────────────────────────────────────────┘
         ↑ 初始化为一个 FREE 块 40960


vmalloc(NULL, 53248) → 0x7f1234567000
┌──────────────────────────────────────────────┐
│   53248 字节虚拟内存（13 页）                 │
│   0x7f1234567000 ~ 0x7f123456d000           │
└──────────────────────────────────────────────┘
         ↑ 初始化为一个 FREE 块 53248


堆管理层的块链表（跨越两个虚拟内存区域）：

Block 0: [0x10000000, 1024)   ALLOCATED
Block 1: [0x10000400, 1024)   FREE
...
Block N: [0x7f1234567000, 50000)  ALLOCATED
Block N+1: [0x7f1234568c80, 3248)  FREE

注意：块可能跨越不同的虚拟内存区域，
      但它们在堆管理器中保持全局有序
```

### 内存释放过程：

调用：myfree(ptr); 其中 ptr = 0x10000000

```
第1步：用户调用 myfree(0x10000000)

第2步：进入 allocator.c: myfree(void *ptr)
│
├─→ 参数检查：ptr != NULL
│
├─→ 调用 heap_free(g_heap, ptr)

第3步：进入 heap.c: heap_free(heap_t *heap, void *addr)
│   参数：heap = g_heap, addr = 0x10000000
│
│   加锁：pthread_mutex_lock(&heap->lock)
│
├─→ 步骤1：查找包含该地址的块
│   block = heap_find_block(heap, 0x10000000)
│   
│   遍历 blocks_head 链表，用 mem_block_contains 检查
│   Block 0: [0x10000000, 1024) ✓ 包含地址 0x10000000
│   result: Block 0
│
├─→ 步骤2：验证块状态
│   检查 block->state == ALLOCATED （是的）
│
├─→ 步骤3：标记为释放
│   block->state = FREE
│   
│   现在的链表状态：
│   Block 0: [0x10000000, 1024)   FREE
│   Block 1: [0x10000400, 1024)   FREE   ← 相邻的 FREE 块！
│
├─→ 步骤4：尝试与相邻块合并
│   _heap_try_merge_adjacent(block)
│   
│   检查后继块是否是 FREE
│   Block 0->next == Block 1
│   Block 1->state == FREE ✓
│   Block 0 和 Block 1 相邻 ✓
│   
│   执行合并：mem_block_merge(Block 0, Block 1)
│   
│   结果：
│   Block 0: [0x10000000, 2048)   FREE
│   （Block 1 被销毁）
│   
│   链表现在：
│   Block 0: [0x10000000, 2048)   FREE
│   Block 2: [..., ...]
│
├─→ 步骤5：检查与前驱块的合并
│   Block 0->prev = NULL （没有前驱）
│
├─→ 步骤6：更新统计信息
│   heap->total_allocated -= 1024
│   heap->total_free += 1024
│
│   解锁：pthread_mutex_unlock(&heap->lock)
│
└─→ 返回 0 （成功）
```

#### 内存状态变化：

```
释放前（两个相邻的块，都已分配）：
┌────────────────────┬────────────────────┐
│ ALLOCATED (1024)   │ ALLOCATED (1024)   │
│ [0x10000000]       │ [0x10000400]       │
└────────────────────┴────────────────────┘

myfree(0x10000000) 第一步（标记为 FREE）：
┌────────────────────┬────────────────────┐
│ FREE (1024)        │ ALLOCATED (1024)   │
│ [0x10000000]       │ [0x10000400]       │
└────────────────────┴────────────────────┘

myfree 继续，与后继合并：
┌─────────────────────────────────────────┐
│ FREE (2048)                             │
│ [0x10000000]  已与后继合并              │
└─────────────────────────────────────────┘

最终状态：块被释放，内存可重新分配
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
