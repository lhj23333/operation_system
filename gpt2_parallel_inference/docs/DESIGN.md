

### Tensor 设计

#### 1. 张量（Tensor）基础概念

张量（Tensor）是数学中对多维数据的一种统一抽象，可以看作是标量、向量、矩阵向更高维度的推广，是机器学习与深度学习的核心数据结构。

##### 1.1 张量的维度（Rank）

| 类型 | 维度 | 示例 | 解释 |
|------|------|------|------|
| 标量 Scalar | 0D | `3.14` | 单个数值 |
| 向量 Vector | 1D | `[1, 2, 3]` | 一串有序数字 |
| 矩阵 Matrix | 2D | `[[1,2],[3,4]]` | 二维表格 |
| 三维张量 Tensor | 3D | 图像（H×W×C） | 多通道数据 |
| N 维张量 | ND | `[batch, seq, hidden]` | 深度学习核心结构 |

##### 1.2 张量的形状（Shape）

张量的形状定义每一维的大小，例如：

- `(3,)`：3 个元素的向量  
- `(3, 4)`：3 行 4 列矩阵  
- `(2, 3, 4)`：2 个 3×4 的矩阵堆叠  

##### 1.3 内存连续性（Contiguous Memory）

大多数深度学习框架采用 **连续内存块** 来存储张量，以提升：

- CPU/GPU Cache 命中率  
- SIMD/VECTOR 指令利用率  
- Batch 运算性能  

---

#### 2. 从标量到多维张量的演进
```
维度演进：
├─ 0维 (标量 Scalar)      → 一个数字
│   例：温度 = 25.6°C
│
├─ 1维 (向量 Vector)       → 一串数字
│   例：位置 = [x, y, z] = [1.0, 2.0, 3.0]
│
├─ 2维 (矩阵 Matrix)       → 表格
│   例：图像灰度值 = [[255, 128, 64],
│                      [200, 150, 100]]
│
├─ 3维 (三维张量)          → 立方体
│   例：彩色图像 = [宽, 高, RGB通道]
│
└─ N维 (高维张量)          → GPT-2 的核心数据结构
    例：[batch_size, seq_length, hidden_dim]
         [2, 1024, 768]
```

---

#### 3. 深度学习中张量的典型使用
张量用于表示：

- 输入（图像/文本 token）
- 中间状态（Attention Q/K/V）
- 模型参数（权重矩阵）
- 输出 logits

以下为 GPT-2 Small 的典型张量形状：

```
/* GPT-2 Small 的典型张量维度 */

// 输入：Token IDs
// shape: [batch_size, seq_length]
// 例：[4, 512] → 4个句子，每句512个token

// Embedding 层输出
// shape: [batch_size, seq_length, hidden_dim]
// 例：[4, 512, 768] → 每个token变成768维向量

// Attention 权重矩阵
// shape: [num_heads, seq_length, seq_length]
// 例：[12, 512, 512] → 12个注意力头

// 最终输出 Logits
// shape: [batch_size, seq_length, vocab_size]
// 例：[4, 512, 50257] → 每个位置预测50257个词的概率
```

#### 4. 操作系统视角：张量的内存布局

- 从系统/底层角度来看，一个张量本质上是一个结构体：

```c
typedef struct {
    float *data;        // 数据指针（堆分配）
    size_t *shape;      // 各维度数组，如[batch, seq_len, hidden_dm]
    size_t *stride;     // 各维度步长数组
    size_t ndim;        // 维度数量
    size_t size;        // 总元素数量
    size_t offset;      // 数据偏移（用于切片等共享场景）  
    bool owns_data;     // 是否拥有数据所有权（用于切片等共享场景）
} Tensor;
```

其中：
- shape：描述结构
- stride：决定访问方式
- offset：切片视图的关键

##### 4.1 行优先（Row-Major）存储
- C/C++ 默认采用行优先，即 最右侧维度在内存中最连续。

```
/*
 * 二维数组 A[2][3] 的内存布局
 * 
 * 逻辑视图（人类思维）：
 *     A = | 1  2  3 |
 *         | 4  5  6 |
 * 
 * 物理内存（计算机存储）：
 *     [1] [2] [3] [4] [5] [6]
 *      ↑       ↑       ↑
 *    行0     行0     行1
 * 
 * 索引公式：A[i][j] → offset = i * cols + j
 *          A[1][2] → offset = 1 * 3 + 2 = 5 ✓
 */

// C 语言默认是行优先
int A[2][3] = {
    {1, 2, 3},  // 连续存储
    {4, 5, 6}
};

// 指针访问验证
int *p = &A[0][0];
for (int i = 0; i < 6; i++) {
    printf("%d ", p[i]);  // 输出: 1 2 3 4 5 6
}
```

##### 4.2 三维张量（T[D][H][W]）的内存布局

```
/*
 * 三维张量 T[2][3][4] 的内存映射
 * 
 * 维度含义：[深度][行][列]
 * 
 * 逻辑视图：
 *   层0:  | 0  1  2  3 |      层1:  | 12 13 14 15 |
 *         | 4  5  6  7 |            | 16 17 18 19 |
 *         | 8  9 10 11 |            | 20 21 22 23 |
 * 
 * 物理内存（一维）：
 *   [0][1][2][3][4][5]...[22][23]
 *    └─────────┘ └────────┘
 *       层0行0      层0行1
 * 
 * 索引公式（行优先）：
 *   offset = i * (rows * cols) + j * cols + k
 *          = i * 12 + j * 4 + k
 * 
 * 例：T[1][2][3] → offset = 1*12 + 2*4 + 3 = 23 ✓
 */
```

#### 5. 张量切片与视图
- 切片是从原张量中选取部分数据，生成一个子张量。
- 绝大多数深度学习框架（PyTorch、TF）采用 共享内存的 View 方式实现切片（除非强制 deep copy）。

##### 5.1 直观理解
```
┌─────────────────────────────────────────┐
│  原张量 (Shape: [5, 6])                 │
│                                         │
│  0  1  2  3  4  5                       │
│  6  7  8  9  10 11                      │
│  12 13 14 15 16 17  ← start_row = 1   │
│  18 19 20 21 22 23  ← end_row = 4     │
│  24 25 26 27 28 29                      │
│              ↑              ↑            │
│          col_start=2   col_end=5        │
└─────────────────────────────────────────┘
                  ↓ slice
        ┌──────────────────┐
        │ 8  9  10         │
        │ 14 15 16         │  Shape: [3, 3]
        │ 20 21 22         │
        └──────────────────┘
```

##### 5.2 实现策略：

① 策略 1：`View` 模式（View/Shared Data）
- 特点:
    - 只改变 shape/stride/offset，共享底层数据
    - 高效（O(ndim)）
    - 不消耗额外内存
- 缺点：
    - 修改切片会影响原张量
    - 原张量被释放后视图失效

② 策略 2：`Copy` 模式（Copy/Deep Copy）
- 特点:
    - 分配新内存复制数据
    - 独立安全，修改互不影响
    - 适合 GPU 计算和独立存储
- 缺点：
    - 低效（O(slice_size)）
    - 内存消耗大

③ 策略 3：`Mixed` 模式（Mixed/Smart Copy）
- 特点
    - 根据场景智能选择 View 或 Copy（例如 pyTorch 的 `.contiguous()`）   
    - 灵活高效


##### 5.3 切片的整体框架设计

```
┌─────────────────────────────────────────────────────────────┐
│                    tensor_slice() 接口                      │
└─────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┴──────────────┐
                │                            │
                ▼                            ▼
    tensor_slice_view()         tensor_slice_copy()
    
    ┌─────────────────────┐     ┌──────────────────────┐
    │  创建视图张量       │     │  创建新张量          │
    │  ✓ 快速 O(ndim)     │     │  ✓ 独立安全          │
    │  ✓ 无额外内存       │     │  ✗ 慢 O(size)        │
    │  ✗ 共享数据         │     │  ✗ 内存消耗          │
    │  ✗ 修改影响原张量   │     │                      │
    │                     │     │  包含步骤：          │
    │  元数据修改：       │     │  1. 创建新元数据      │
    │  1. 调整 shape      │     │  2. 分配新数据       │
    │  2. 更新 offset     │     │  3. 逐元素拷贝      │
    │  3. 继承 stride     │     │  4. 重置 stride      │
    └─────────────────────┘     └──────────────────────┘
              │                            │
              └────────────┬───────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │  返回 Tensor* 指针        │
              │  使用统一接口              │
              │  tensor_get/tensor_set    │
              │  tensor_free              │
              └──────────────────────────┘
```


### 事件队列-生产者消费者线程池设计

#### 1. 整体架构设计

##### 1.1 三层架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         用户层 (User Layer)                      │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  thread_pool_create()  thread_pool_submit()              │   │
│  │  thread_pool_wait_all()  thread_pool_destroy()           │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     线程池层 (Thread Pool Layer)                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  thread_pool_t                                           │   │
│  │  ┌────────────────┐  ┌────────────────┐                 │   │
│  │  │  Worker Thread │  │  Worker Thread │  ...            │   │
│  │  │   (Thread 0)   │  │   (Thread 1)   │                 │   │
│  │  └────────┬───────┘  └────────┬───────┘                 │   │
│  │           │                   │                          │   │
│  │           └───────────┬───────┘                          │   │
│  │                       ▼                                  │   │
│  │           ┌─────────────────────┐                        │   │
│  │           │   task_queue_t      │                        │   │
│  │           └─────────────────────┘                        │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     任务队列层 (Task Queue Layer)                │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  task_queue_t (FIFO Queue)                               │   │
│  │  ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐             │   │
│  │  │ Task │ → │ Task │ → │ Task │ → │ Task │ → NULL      │   │
│  │  └──────┘   └──────┘   └──────┘   └──────┘             │   │
│  │   front                              back                │   │
│  │                                                          │   │
│  │  Synchronization:                                        │   │
│  │  • mutex (互斥锁)                                        │   │
│  │  • cond_not_empty (非空条件变量)                         │   │
│  │  • cond_not_full  (非满条件变量)                         │   │
│  │  • cond_all_done  (全部完成条件变量)                     │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

##### 1.2 数据流向图

```
                    ┌──────────────────────────────┐
                    │   Main Thread (Producer)     │
                    └──────────────┬───────────────┘
                                   │ submit task
                                   ▼
                    ┌──────────────────────────────┐
                    │      task_queue_submit()     │
                    │   创建 Task 并入队           │
                    └──────────────┬───────────────┘
                                   │
                    ┌──────────────▼───────────────┐
                    │      Task Queue (FIFO)       │
                    │  ┌──────┐  ┌──────┐         │
                    │  │ Task │→ │ Task │→ ...    │
                    │  └──────┘  └──────┘         │
                    │  count: 2, active_tasks: 0  │
                    └──────────────┬───────────────┘
                                   │ cond_not_empty signal
                    ┌──────────────▼───────────────┐
                    │   Worker Threads (Consumers) │
                    ├──────────────────────────────┤
                    │  Thread 0  Thread 1  Thread 2│
                    │     ▼          ▼         ▼   │
                    │  task_queue_pop_and_execute()│
                    └──────────────┬───────────────┘
                                   │
                    ┌──────────────▼───────────────┐
                    │  1. 从队列取出 Task          │
                    │     count--, active_tasks++  │
                    │  2. 在锁外执行 task->func()  │
                    │  3. 调用 cleanup (如果有)    │
                    │  4. active_tasks--           │
                    │  5. 如果 count==0 &&         │
                    │     active_tasks==0          │
                    │     → broadcast cond_all_done│
                    └──────────────────────────────┘
```

##### 1.3 生产者-消费者模型

```
┌─────────────────────────────────────────────────────────────┐
│                    生产者-消费者模型                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  生产者 (Main Thread)         任务队列        消费者 (Workers)│
│  ┌──────────────┐           ┌──────┐         ┌────────────┐│
│  │              │  submit   │      │  pop    │  Worker 0  ││
│  │ User Code    │─────────→ │ FIFO │←────────│            ││
│  │              │           │ Queue│         │  Worker 1  ││
│  └──────────────┘           │      │         │            ││
│                             │      │←────────│  Worker 2  ││
│  ┌──────────────┐           └──────┘         │            ││
│  │  Backpressure│                            │  Worker 3  ││
│  │  当队列满时   │                            └────────────┘│
│  │  阻塞生产者   │                                          │
│  └──────────────┘                                          │
│                                                             │
│  关键同步点:                                                │
│  • cond_not_full:  队列满 → 阻塞生产者                      │
│  • cond_not_empty: 队列空 → 阻塞消费者                      │
│  • cond_all_done:  所有任务完成 → 唤醒 wait_all()          │
└─────────────────────────────────────────────────────────────┘
```

---

#### 2. 核心数据结构

##### 2.1 结构体关系图

```
thread_pool_t
├── thread_pool_cfg_t (配置)
│   ├── num_threads
│   ├── queue_size
│   ├── stack_size
│   └── daemon_threads
│
├── pthread_t *threads (线程数组)
│   └── [thread_0, thread_1, ..., thread_n]
│
├── thread_info_t *thread_infos (线程信息数组)
│   └── [info_0, info_1, ..., info_n]
│       ├── tid (线程ID)
│       ├── id (编号)
│       ├── tasks_completed (完成任务数)
│       ├── is_active (是否活跃)
│       └── should_exit (退出标志)
│
├── task_queue_t *task_queue (任务队列)
│   ├── Task *front (队头)
│   ├── Task *back (队尾)
│   ├── count (当前任务数)
│   ├── active_tasks (执行中任务数) ★ 关键
│   ├── max_count (最大容量)
│   ├── pthread_mutex_t mutex
│   ├── pthread_cond_t cond_not_empty
│   ├── pthread_cond_t cond_not_full
│   ├── pthread_cond_t cond_empty
│   └── pthread_cond_t cond_all_done ★ 关键
│
├── thread_pool_state_t state (线程池状态)
├── pthread_mutex_t state_lock
├── pthread_cond_t state_changed
└── volatile bool shutdown (关闭标志)
```

##### 2.2 Task 结构体设计

```c
typedef struct task {
    void (*function)(void *arg);    // 任务函数指针
    void *arg;                      // 任务参数
    void (*cleanup)(void *arg);     // 清理函数 (可选)
    struct task *next;              // 链表指针
} Task;
```

**设计要点:**
- **函数指针**: 支持任意类型的任务
- **cleanup 机制**: 自动释放资源，避免内存泄漏
- **链表结构**: 简单高效的 FIFO 实现

##### 2.3 线程池状态机

```
┌──────────────┐
│ POOL_CREATED │ 初始状态
└──────┬───────┘
       │ thread_pool_create()
       │ 创建所有工作线程
       ▼
┌──────────────┐
│ POOL_RUNNING │ 正常运行，接受任务
└──────┬───────┘
       │ thread_pool_destroy()
       │ thread_pool_shutdown()
       ▼
┌──────────────┐
│POOL_STOPPING │ 停止中，不接受新任务
└──────┬───────┘
       │ 等待所有线程退出
       ▼
┌──────────────┐
│ POOL_STOPPED │ 已停止
└──────────────┘
```

---

#### 3. 接口设计与调用流程

##### 3.1 核心 API

###### 3.1.1 线程池 API

```c
// 创建线程池
thread_pool_t* thread_pool_create(const thread_pool_cfg_t *cfg);

// 提交任务 (支持 cleanup 函数)
int thread_pool_submit(thread_pool_t *pool, 
                       void (*func)(void *), 
                       void *arg,
                       void (*cleanup)(void *));

// 等待所有任务完成
void thread_pool_wait_all(thread_pool_t *pool);

// 优雅关闭 (等待任务完成后销毁)
void thread_pool_shutdown(thread_pool_t *pool);

// 立即销毁 (不等待任务完成)
void thread_pool_destroy(thread_pool_t *pool);

// 动态调整线程数
int thread_pool_resize(thread_pool_t *pool, int new_size);
```

#### 3.1.2 任务队列 API

```c
// 创建任务队列
task_queue_t* task_queue_create(size_t max_count);

// 提交任务 (高层 API)
int task_queue_submit(task_queue_t *queue, 
                      void (*func)(void*), 
                      void *arg,
                      void (*cleanup)(void*));

// 取出并执行任务 (工作线程使用)
int task_queue_pop_and_execute(task_queue_t *queue, 
                                bool *should_shutdown);

// 等待队列为空且所有任务执行完成
void task_queue_wait_empty(task_queue_t *queue);

// 销毁队列
void task_queue_destroy(task_queue_t *queue);
```

##### 3.2 典型调用流程

###### 场景 1: 提交任务并等待完成

```
User Thread                Thread Pool              Worker Threads
     │                          │                         │
     │ 1. thread_pool_create()  │                         │
     ├─────────────────────────→│                         │
     │                          │ 创建工作线程             │
     │                          ├────────────────────────→│
     │                          │                         │ _work_thread()
     │                          │                         │ 等待任务...
     │                          │                         │
     │ 2. thread_pool_submit()  │                         │
     ├─────────────────────────→│                         │
     │                          │ task_queue_submit()     │
     │                          ├─────────────────┐       │
     │                          │                 ▼       │
     │                          │         ┌────────────┐  │
     │                          │         │Task Queue  │  │
     │                          │         │ [Task]     │  │
     │                          │         └────────────┘  │
     │                          │                 │       │
     │                          │ signal cond_not_empty   │
     │                          ├────────────────────────→│
     │                          │                         │ task_queue_pop_and_execute()
     │                          │                         │ active_tasks++
     │                          │                         │ 执行 task->function()
     │                          │                         │ active_tasks--
     │                          │                         │ signal cond_all_done
     │                          │                         │
     │ 3. thread_pool_wait_all()│                         │
     ├─────────────────────────→│                         │
     │                          │ task_queue_wait_empty() │
     │                          │ wait cond_all_done      │
     │                          │←────────────────────────│
     │←─────────────────────────│                         │
     │                          │                         │
     │ 4. thread_pool_destroy() │                         │
     ├─────────────────────────→│                         │
     │                          │ shutdown = true         │
     │                          │ signal all threads      │
     │                          ├────────────────────────→│
     │                          │                         │ 退出
     │←─────────────────────────│                         │
```

---

#### 4. 同步机制设计

##### 4.1 多级锁设计

```
┌─────────────────────────────────────────────────────────┐
│                    锁的层次结构                          │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  Level 1: pool->state_lock                              │
│  ├── 保护线程池状态 (state, shutdown)                   │
│  ├── 保护线程信息数组 (thread_infos)                    │
│  └── 保护线程数量 (num_threads)                         │
│                                                          │
│  Level 2: task_queue->mutex                             │
│  ├── 保护任务队列 (front, back, count)                  │
│  ├── 保护活跃任务计数 (active_tasks) ★ 关键             │
│  └── 配合多个条件变量                                   │
│                                                          │
│  注意: 避免死锁！不要同时持有两个锁                      │
└─────────────────────────────────────────────────────────┘
```

##### 4.2 条件变量设计

```c
┌──────────────────────────────────────────────────────────┐
│               条件变量及其唤醒条件                        │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  1. cond_not_empty (队列非空)                            │
│     等待条件: count == 0                                 │
│     唤醒时机: task_queue_push() 入队后                   │
│     唤醒对象: 工作线程 (消费者)                          │
│                                                           │
│  2. cond_not_full (队列未满)                             │
│     等待条件: count >= max_count                         │
│     唤醒时机: task_queue_pop_and_execute() 出队后        │
│     唤醒对象: 主线程 (生产者)                            │
│                                                           │
│  3. cond_all_done (所有任务完成) ★ 核心                 │
│     等待条件: count > 0 || active_tasks > 0              │
│     唤醒时机: active_tasks-- 后                          │
│               且 count==0 && active_tasks==0             │
│     唤醒对象: thread_pool_wait_all()                     │
│                                                           │
│  4. cond_empty (队列为空) [已废弃]                       │
│     被 cond_all_done 替代，更精确                        │
└──────────────────────────────────────────────────────────┘
```

##### 4.3 关键同步点

###### 4.3.1 生产者阻塞 (背压机制)

```c
// task_queue_push() 中
pthread_mutex_lock(&queue->mutex);

while (queue->max_count > 0 && queue->count >= queue->max_count) {
    // 队列满，等待消费者取走任务
    pthread_cond_wait(&queue->cond_not_full, &queue->mutex);
}

// 入队操作...
queue->count++;

// 唤醒等待的消费者
pthread_cond_signal(&queue->cond_not_empty);
pthread_mutex_unlock(&queue->mutex);
```

###### 4.3.2 消费者等待

```c
// task_queue_pop_and_execute() 中
pthread_mutex_lock(&queue->mutex);

while (queue->count == 0 && !(*should_shutdown)) {
    // 队列空，等待生产者提交任务
    pthread_cond_wait(&queue->cond_not_empty, &queue->mutex);
}

// 出队操作...
queue->count--;
queue->active_tasks++;  // ★ 增加活跃任务计数

pthread_mutex_unlock(&queue->mutex);

// 在锁外执行任务
task->function(task->arg);

// 任务完成，更新计数
pthread_mutex_lock(&queue->mutex);
queue->active_tasks--;

if (queue->count == 0 && queue->active_tasks == 0) {
    // ★ 关键: 队列空且没有活跃任务，唤醒 wait_all()
    pthread_cond_broadcast(&queue->cond_all_done);
}
pthread_mutex_unlock(&queue->mutex);
```

###### 4.3.3 等待所有任务完成

```c
// task_queue_wait_empty() 中
pthread_mutex_lock(&queue->mutex);

while (queue->count > 0 || queue->active_tasks > 0) {
    // ★ 等待两个条件都满足:
    //   1. 队列中没有待处理任务 (count == 0)
    //   2. 没有正在执行的任务 (active_tasks == 0)
    pthread_cond_wait(&queue->cond_all_done, &queue->mutex);
}

pthread_mutex_unlock(&queue->mutex);
```

---

#### 5. 关键实现细节

##### 5.1 工作线程主循环

```c
static void *_work_thread(void *arg) {
    thread_pool_t *pool = ...;
    thread_info_t *info = ...;
    
    while (true) {
        // 1. 检查退出标志 (用于 resize 缩容)
        pthread_mutex_lock(&pool->state_lock);
        bool should_exit = info->should_exit;
        pthread_mutex_unlock(&pool->state_lock);
        
        if (should_exit) break;
        
        // 2. 获取并执行任务
        info->is_active = true;
        
        // ★ 关键: 直接传递 pool->shutdown 地址，确保读取最新值
        int ret = task_queue_pop_and_execute(pool->task_queue, 
                                             (bool*)&pool->shutdown);
        
        if (ret == 1) {
            // 收到关闭信号
            break;
        } else if (ret == 0) {
            // 任务执行成功
            info->tasks_completed++;
        }
        
        info->is_active = false;
    }
    
    return NULL;
}
```

**设计要点:**
1. **双重退出机制**: `should_exit` (resize) 和 `shutdown` (销毁)
2. **直接传递 `pool->shutdown` 地址**: 避免局部变量拷贝导致的不同步问题
3. **统计信息更新**: 记录完成任务数，用于监控

##### 5.2 任务执行的完整生命周期

```
┌─────────────────────────────────────────────────────────┐
│            Task 生命周期 (task_queue_pop_and_execute)   │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  1. 持锁等待任务                                         │
│     pthread_mutex_lock(&queue->mutex)                   │
│     while (count == 0 && !shutdown)                     │
│         pthread_cond_wait(&cond_not_empty, &mutex)      │
│                                                          │
│  2. 从队列取出任务                                       │
│     task = queue->front                                 │
│     queue->front = task->next                           │
│     count--                                             │
│     active_tasks++ ★ 标记为执行中                       │
│                                                          │
│  3. 释放锁                                              │
│     pthread_mutex_unlock(&queue->mutex)                 │
│                                                          │
│  4. 在锁外执行任务 (避免长时间持锁)                      │
│     task->function(task->arg)                           │
│                                                          │
│  5. 调用清理函数                                         │
│     if (task->cleanup)                                  │
│         task->cleanup(task->arg)                        │
│                                                          │
│  6. 释放 Task 结构体                                     │
│     free(task)                                          │
│                                                          │
│  7. 更新活跃任务计数                                     │
│     pthread_mutex_lock(&queue->mutex)                   │
│     active_tasks-- ★ 标记为已完成                       │
│     if (count == 0 && active_tasks == 0)                │
│         pthread_cond_broadcast(&cond_all_done) ★ 唤醒   │
│     pthread_mutex_unlock(&queue->mutex)                 │
└─────────────────────────────────────────────────────────┘
```

##### 5.3 动态调整线程数 (Resize)

###### 扩容流程

```c
static int _thread_pool_expand(thread_pool_t *pool, int new_size, int cur_size) {
    pthread_mutex_lock(&pool->state_lock);
    
    // 1. realloc 扩展数组
    pool->threads = realloc(pool->threads, new_size * sizeof(pthread_t));
    pool->thread_infos = realloc(pool->thread_infos, new_size * sizeof(thread_info_t));
    
    // 2. 创建新线程
    for (int i = cur_size; i < new_size; i++) {
        // 初始化线程信息
        pool->thread_infos[i].id = i;
        pool->thread_infos[i].should_exit = false;
        
        // 创建线程
        pthread_create(&pool->threads[i], NULL, _work_thread, &arg);
    }
    
    pool->num_threads = new_size;
    pthread_mutex_unlock(&pool->state_lock);
    
    return 0;
}
```

###### 缩容流程

```c
static int _thread_pool_shrink(thread_pool_t *pool, int new_size, int cur_size) {
    // 1. 设置退出标志
    pthread_mutex_lock(&pool->state_lock);
    for (int i = new_size; i < cur_size; i++) {
        pool->thread_infos[i].should_exit = true;
    }
    pthread_mutex_unlock(&pool->state_lock);
    
    // 2. 唤醒所有等待的线程
    pthread_cond_broadcast(&pool->task_queue->cond_not_empty);
    
    // 3. 等待线程退出
    for (int i = new_size; i < cur_size; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // 4. 缩小数组
    pool->threads = realloc(pool->threads, new_size * sizeof(pthread_t));
    pool->thread_infos = realloc(pool->thread_infos, new_size * sizeof(thread_info_t));
    
    return 0;
}
```

##### 5.4 优雅关闭

```c
void thread_pool_shutdown(thread_pool_t *pool) {
    // 1. 等待所有任务完成
    thread_pool_wait_all(pool);
    
    // 2. 销毁线程池
    thread_pool_destroy(pool);
}

void thread_pool_destroy(thread_pool_t *pool) {
    // 1. 设置关闭标志
    pthread_mutex_lock(&pool->state_lock);
    pool->state = POOL_STOPPING;
    pool->shutdown = true;  // ★ 关键: 通知所有工作线程
    pthread_mutex_unlock(&pool->state_lock);
    
    // 2. 唤醒所有等待的工作线程
    pthread_cond_broadcast(&pool->task_queue->cond_not_empty);
    
    // 3. 等待所有线程退出
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // 4. 清理资源
    task_queue_destroy(pool->task_queue);
    free(pool->threads);
    free(pool->thread_infos);
    // ...
}
```

---

#### 6. 性能优化与最佳实践

##### 6.1 锁优化

###### ❌ 反模式: 持锁时间过长

```c
// 错误: 在持锁状态下执行任务
pthread_mutex_lock(&queue->mutex);
Task *task = task_queue_pop(queue);
task->function(task->arg);  // ❌ 长时间持锁
pthread_mutex_unlock(&queue->mutex);
```

#### ✅ 正确做法: 锁外执行

```c
pthread_mutex_lock(&queue->mutex);
Task *task = queue->front;
queue->front = task->next;
queue->count--;
queue->active_tasks++;
pthread_mutex_unlock(&queue->mutex);  // ✅ 及时释放锁

// 在锁外执行任务
task->function(task->arg);

// 更新活跃任务计数
pthread_mutex_lock(&queue->mutex);
queue->active_tasks--;
// ...
pthread_mutex_unlock(&queue->mutex);
```

##### 6.2 避免惊群效应

```c
// ✅ 使用 signal 而不是 broadcast (当只需唤醒一个线程时)
pthread_cond_signal(&queue->cond_not_empty);  // 只唤醒一个消费者

// ✅ 使用 broadcast (当需要唤醒所有线程时)
pthread_cond_broadcast(&queue->cond_all_done);  // 唤醒所有 wait_all()
```

##### 6.3 内存管理

###### Cleanup 函数机制

```c
// 用户代码
typedef struct {
    int *data;
    size_t size;
} my_task_arg_t;

void my_task_cleanup(void *arg) {
    my_task_arg_t *task_arg = (my_task_arg_t *)arg;
    free(task_arg->data);  // 释放内部资源
    free(task_arg);        // 释放结构体本身
}

void my_task(void *arg) {
    my_task_arg_t *task_arg = (my_task_arg_t *)arg;
    // 处理任务...
    // 注意: 不要在这里 free(task_arg)，由 cleanup 函数处理
}

// 提交任务
my_task_arg_t *arg = malloc(sizeof(my_task_arg_t));
arg->data = malloc(1024);
thread_pool_submit(pool, my_task, arg, my_task_cleanup);
```

##### 6.4 线程数配置建议

```c
// CPU 密集型任务
config.num_threads = get_cpu_cores();

// I/O 密集型任务
config.num_threads = get_cpu_cores() * 2;

// 混合型任务
config.num_threads = get_cpu_cores() + 2;

// 队列大小设置
config.queue_size = config.num_threads * 10;  // 避免频繁阻塞
```

##### 6.5 常见陷阱

###### 陷阱 1: shutdown 标志的局部拷贝

```c
// ❌ 错误: 局部变量不会更新
bool shutdown = pool->shutdown;
task_queue_pop_and_execute(queue, &shutdown);  // shutdown 是局部拷贝

// ✅ 正确: 直接传递地址
task_queue_pop_and_execute(queue, (bool*)&pool->shutdown);
```

###### 陷阱 2: active_tasks 遗漏

```c
// ❌ 错误: 没有跟踪正在执行的任务
void task_queue_wait_empty(task_queue_t *queue) {
    while (queue->count > 0) {  // ❌ 只检查队列中的任务
        pthread_cond_wait(&queue->cond_empty, &queue->mutex);
    }
    // 可能还有任务正在执行！
}

// ✅ 正确: 同时检查队列和执行中的任务
void task_queue_wait_empty(task_queue_t *queue) {
    while (queue->count > 0 || queue->active_tasks > 0) {  // ✅
        pthread_cond_wait(&queue->cond_all_done, &queue->mutex);
    }
}
```

###### 陷阱 3: 死锁风险

```c
// ❌ 错误: 嵌套锁可能导致死锁
pthread_mutex_lock(&pool->state_lock);
pthread_mutex_lock(&pool->task_queue->mutex);  // ❌ 危险
// ...
pthread_mutex_unlock(&pool->task_queue->mutex);
pthread_mutex_unlock(&pool->state_lock);

// ✅ 正确: 避免同时持有多个锁
pthread_mutex_lock(&pool->state_lock);
bool shutdown = pool->shutdown;
pthread_mutex_unlock(&pool->state_lock);

pthread_mutex_lock(&pool->task_queue->mutex);
// 使用 shutdown 变量...
pthread_mutex_unlock(&pool->task_queue->mutex);
```

---

#### 7. 测试与调试

##### 7.1 关键测试场景

```c
// 1. 基本功能测试
test_queue_basic();              // 队列基本操作
test_pool_create_destroy();      // 创建销毁

// 2. 并发测试
test_pool_concurrent_execution(); // 多任务并发执行

// 3. 背压测试
test_queue_backpressure();       // 队列满时的阻塞

// 4. 性能测试
test_pool_performance();         // 大量任务吞吐量

// 5. 边界测试
test_pool_resize();              // 动态调整线程数
test_pool_shutdown();            // 优雅关闭
```

##### 7.2 调试技巧

```c
// 使用 thread_pool_print_info() 查看状态
thread_pool_print_info(pool);

// 输出示例:
// ╔════════════════════════════════════╗
// ║     Thread Pool Information        ║
// ╠════════════════════════════════════╣
// ║ Status:          RUNNING           ║
// ║ Num threads:     4                 ║
// ║ Pending tasks:   10                ║
// ║                                    ║
// ║ Worker threads:                    ║
// ║   [0] tasks=25       ACTIVE        ║
// ║   [1] tasks=30       IDLE          ║
// ║   [2] tasks=28       ACTIVE        ║
// ║   [3] tasks=27       IDLE          ║
// ╚════════════════════════════════════╝
```

---

#### 8. 总结

##### 核心设计亮点

1. ✅ **active_tasks 计数器**: 精确跟踪正在执行的任务，确保 `wait_all()` 正确等待
2. ✅ **cleanup 机制**: 自动资源管理，避免内存泄漏
3. ✅ **背压机制**: 队列满时阻塞生产者，防止内存溢出
4. ✅ **动态调整**: 支持运行时调整线程数
5. ✅ **优雅关闭**: 等待任务完成后再销毁

##### 关键同步点

- **cond_not_empty**: 队列空 → 阻塞消费者
- **cond_not_full**: 队列满 → 阻塞生产者
- **cond_all_done**: 所有任务完成 → 唤醒 `wait_all()`

##### 性能考虑

- 锁外执行任务，减少持锁时间
- 使用 signal 而非 broadcast，减少惊群
- 合理配置线程数和队列大小


### 并行计算理论基础

#### 1. Amdahl 定律（Amdahl's Law）

Amdahl 定律描述了：

> **若一个程序只有部分能并行化，则整体加速比会受到不可并行部分的限制。**

##### **公式：**

```text
Speedup = 1 / ( S + P / N )
```

* **S**：程序中不可并行（串行）的比例
* **P**：可并行部分的比例（S + P = 1）
* **N**：处理器（核）数量
* **Speedup**：总体加速比

---

##### 1.1 Amdahl 定律直观图解

```
 总运行时间 = 串行部分 + 并行部分
   S             P
```

- 即使并行部分 (P) 很大，但 **S 是固定不能并行的** → 决定性能上限。

---

##### 1.2 示例：95% 可并行程序（S = 0.05）

| 核心数 N | 加速比 Speedup |
| ----- | ----------- |
| 1     | 1.00        |
| 2     | 1.90        |
| 4     | 3.48        |
| 8     | 5.92        |
| ∞     | 20.00       |

当 N → ∞ 时：

```
Speedup_max = 1 / S = 1 / 0.05 = 20
```

##### **图示：加速比随核心数增长（S=0.05）**

```
Speedup
↑
|                          ______ 20（上限）
|                       __/
|                    __/
|                 __/
|              __/
|          ___/
|     ____/
|____/
+--------------------------------→ N
      1   2   4   8      →∞
```

**结论：** 即使核心数无限多，加速比也不超过 20。

---

#### 2. 矩阵乘法的 Amdahl 定律分析

- 矩阵乘法：C = A × B （MxK @ KxN = MxN）

C 中每个元素可独立计算 → **高可并行度**。

##### 2.1 计算量（完全可并行）

- 朴素三重循环：
```c
for (M) for (N) for (K) C[i][j] += A[i][k] * B[k][j];
```

```
乘法次数 = M × N × K
加法次数 = M × N × K
总 FLOP  = 2 × M × N × K
```

##### 2.2 内存访问量（不可完全并行）

* 读 A：M × K 次
* 读 B：K × N 次
* 读/写 C：M × N × 2 次

---

#### 3. 算法强度（Arithmetic Intensity, AI）

- 但在实际分析时，我们更关注算法强度（Arithmetic Intensity）：

$\text{AI} = \frac{\text{浮点运算次数}}{\text{内存字节传输量}} \approx \frac{2n^3}{O(n^2)} = O(n)$ 

含义：

* **矩阵越大，计算量增长更快 → AI 越高 → 更接近计算绑定（compute-bound）**
* **矩阵越小或缓存足够 → 访问全部命中缓存 → 几乎完全并行**

---

#### 4. 两种实际情况的并行效率

**情况1：矩阵可以放入缓存（理想情况）**

* 内存访问代价接近 0（缓存命中率极高）
* 所有 FLOP 都可并行 → P → 1
* 效果：

```
Speedup ≈ N（接近线性加速）
```

BLAS（OpenBLAS、MKL）正是依靠“分块”技巧让矩阵块进入缓存 → 达到接近 90%~95% 峰值性能。

---

** 情况2：大矩阵 → 远超缓存（实际最常见）**

- 此时程序变成内存带宽绑定（memory-bound）- 读写矩阵 A、B、C 的内存访问成为瓶颈。 
- 关键观察： 
    - 计算量：2 * M * N * K 次浮点运算 
    - 内存访问量：读 A (M*K), 读 B (K*N), 读/写 C (M*N) → 总计约 2 * M * N + M * K + K * N 次内存访问 
        - (即使分块优化，最有情况下也只能接近这个下界，著名的“3n² 界”) 
    - 计算与内存访问比（AI）： 
        $\text{AI} = \frac{2 * M * N * K}{\text{内存访问量}} \approx \frac{2 * M * N * K}{2 * M * N + M * K + K * N}$

当矩阵足够大、核心数增加时：

* 每个核心继续执行 FLOP
* 但所有核心共享 **有限内存带宽**

---

#### 5. 内存带宽绑定下的 Amdahl 分析

##### 思路：

**内存带宽是串行资源（无法并行扩展） → 限制整体加速比。**

设：

* 单核带宽：B_single
* 总带宽（共享）：B_total

若程序完全受带宽限制：

```
Speedup_max = B_total / B_single
```

---

##### 示例：Intel Skylake / Cascade Lake（真实测试数据）

* 单核跑大矩阵 sgemm：可占用 ~25~30 GB/s
* 整机 56 核，总带宽 ~200~250 GB/s

则并行加速比最高：

```
Speedup_max ≈ 250 / 30 ≈ 8.3
```

→ **远低于 56 核的理想加速。**

---

##### 图示：带宽受限导致加速比饱和

```
Speedup
↑
|                ________  (带宽上限，例如8x)
|              _/
|            _/
|          _/
|        _/
|      _/
|   __/
|__/
+------------------------------→ N
      增加核心数 (56)
```

---

#### 6. 任务粒度（Task Granularity）

- 任务粒度指每个并行任务的计算量大小。

```
/*
 * 粒度选择：并行性能的关键
 * 
 * ┌─────────────────────────────────────────────────────────┐
 * │                粒度权衡                                 │
 * └─────────────────────────────────────────────────────────┘
 * 
 * 细粒度（Fine-grained）：
 *   每个任务计算很少
 *   例：每个线程计算 1 行
 * 
 *   优点：
 *   ✓ 负载均衡好
 *   ✓ 适应不同硬件
 * 
 *   缺点：
 *   ✗ 任务创建开销大
 *   ✗ 线程切换频繁
 *   ✗ 缓存局部性差
 * 
 * 粗粒度（Coarse-grained）：
 *   每个任务计算很多
 *   例：每个线程计算 100 行
 * 
 *   优点：
 *   ✓ 任务创建开销小
 *   ✓ 缓存局部性好
 * 
 *   缺点：
 *   ✗ 负载不均衡
 *   ✗ 硬件适应性差
 */
```

##### 6.1 粒度选择

- 超细粒度 (每个任务 1 行)：

```c
void matmul_ultra_fine(Tensor *A, Tensor *B, Tensor *C) {
    for (size_t i = 0; i < M; i++) {
        thread_pool_submit(pool, compute_row, (void*)i);
    }
    // 任务数：1000
    // 每任务计算量：1000*1000 = 1M 次浮点运算
    // 任务创建开销：~1ms * 1000 = 1s（可能超过计算时间！）
}
```

- 中等粒度（每个任务 100 行）：

```c
void matmul_medium(Tensor *A, Tensor *B, Tensor *C) {
    for (size_t i = 0; i < M; i += 100) {
        thread_pool_submit(pool, compute_row_range, (void*)i);
    }
    // 任务数：10
    // 每任务计算量：100M 次浮点运算
    // 任务创建开销：~1ms * 10 = 10ms（合理）
}
```

- 粗粒度（每个任务 500 行）：

```c
void matmul_coarse(Tensor *A, Tensor *B, Tensor *C) {
    for (size_t i = 0; i < M; i += 500) {
        thread_pool_submit(pool, compute_row_range, (void*)i);
    }
    // 任务数：2
    // 每任务计算量：500M 次浮点运算
    // 任务创建开销：~1ms * 2 = 2ms（最小）
    // 但如果只有 4 个线程，浪费了 2 个！
}
```

- 动态粒度选择：根据线程数和矩阵大小动态调整任务粒度，达到最佳平衡。

```c
size_t optimal_granularity(size_t M, int num_threads) {
    // 每个线程至少分配 64 行（经验值）
    size_t rows_per_thread = MAX(64, M / (num_threads * 4));
    
    // 向上取整到缓存行大小的倍数（64 字节 = 16 个 float）
    rows_per_thread = (rows_per_thread + 15) & ~15;
    
    return rows_per_thread;
}
```
#### 伪共享（False Sharing）

- 伪共享是多核系统中最容易被忽视、但影响最致命的性能问题之一。
    - 它源于 **硬件缓存行（Cache Line）为最小一致性单位** 的事实。

- 伪共享会导致：
* 多核 CPU 间**缓存行来回争夺**
* 内存一致性协议消耗大量带宽
* 实际性能下降 **10×～100×**
* 难以通过代码逻辑发现（因为逻辑上没有共享）

---

##### 1. CPU 缓存一致性基础（MESI 协议）

- 缓存一致性协议决定了伪共享的性能特征。这里快速回顾现代 CPU 的 MESI 状态机：

| MESI 状态       | 含义       | 可写？ | 其他 CPU 是否可读？ |
| ------------- | -------- | --- | ------------ |
| **M**odified  | 本核独占且已修改 | ✔   | ✘            |
| **E**xclusive | 独占但未修改   | ✔   | ✘            |
| **S**hared    | 多核共享，只读  | ✘   | ✔            |
| **I**nvalid   | 无效       | ✘   | ✘            |

- 关键点：

> **只要某个核心要写入数据，该缓存行必须为 Modified (M) 状态。**

- 所以多核同时写入同一缓存行时，会触发大量的：

* Invalidate（使其他核心缓存行无效）
* Writeback（写回主存）
* Reload（其他核心重新加载缓存行）

- 这就是伪共享的本质。

---

##### 2. 为什么 False Sharing 如此致命？

虽然线程写入的是“不同变量”，但变量 **在物理上落在同一缓存行**。

例如：缓存行大小 64B，而两个 int 只有 8B，这 8B 必定落在同一 cache line 中。

当 CPU0 写入 cache line 的前 4B 时：

```
[CPU0 的 L1 Cache]
缓存行状态从 S/E → M
```

- 当 CPU1 写入同一条缓存行另一部分：

* MESI：CPU1 必须使 CPU0 的缓存行无效
* CPU0 必须将 M 状态的缓存行写回内存
* CPU1 必须从内存重新读取整个 64B 缓存行
* CPU1 改写该行，使其变为 M

- 如果两个线程每次循环都写入该行 → 上述操作会发生 **数百万次**

导致：

✔ CPU <-> CPU 之间“争抢缓存行”
✔ L1/L2 Cache 一致性交互爆炸
✔ 内存带宽白白被浪费
✔ 性能拖慢一个数量级以上

---

##### 3. 示例：看似无冲突，但实际冲突极大

```c
int counters[4];  // 4个线程分别写 counters[0..3]

void thread_func(int id) {
    for (int i = 0; i < 1000000; i++) {
        counters[id]++;
    }
}
```

虽然 counters[i] 是独立的变量：

* 没有锁
* 没有共享逻辑
* 没有数据依赖
* 在算法层面完全可并行

但其内存布局如下：

```
|------------------- 64-byte Cache Line --------------------|
[c0][c1][c2][c3] [padding....................................]
 4B   4B   4B   4B
```

4 个线程不断写入 → 触发百万次 MESI 修改/失效/写回

---

##### 4. CPU 级别的真实交互过程

假设 CPU0 写 counters[0]，CPU1 写 counters[1]：

- 步骤 1：CPU0 写 counters[0]
    * 加载整个缓存行到 L1
    * 修改 counters[0]
    * 设置缓存行为 **M (Modified)**

- 步骤 2：CPU1 写 counters[1]

CPU1 尝试写入同一行，需要进入 Modified，因此：

1. CPU1 发送 **Invalidate** 请求
2. CPU0 必须将缓存行写回内存（Writeback）
3. CPU0 将该行标记为 Invalid
4. CPU1 从内存重新读取整条缓存行（64B）
5. CPU1 修改 counters[1]，缓存行为 M

- 步骤 3：CPU0 再次写 counters[0]

过程反向重复 —— 痛苦无限循环

---

##### 5. 性能灾难示意图

```
正常并行（无伪共享）
吞吐量 ↑
         |             _____________
         |           /
         |         /
         |       /
         |_____/________________________________
              核心数量 →

伪共享（缓存争夺）
吞吐量 ↑
         |     __
         |   /
         |  /
         | /
         |/____________________________________
              核心数量 →
```

---

##### 6. 如何避免伪共享？

- 方案 1：Padding（填充）

```c
struct {
    int counter;
    char padding[60];  // 64 - 4
} counters_aligned[4];
```

使每个 counter 独占一条 64-byte cache line。

---

- 方案 2：对齐（Alignment）

```c
struct {
    int counter;
} __attribute__((aligned(64))) counters_attr[4];
```

等价于 padding，但由编译器确保对齐。

---

- 方案 3：动态对齐分配

```c
int *counters_alloc = aligned_alloc(64, 4 * 64);
```

适合需要手动控制内存布局的高性能库（BLAS、深度学习框架）。

---

- 方案 4：使用每线程私有数据结构

例如在 OpenMP 中：

```c
#pragma omp parallel
{
    static int local_counter[64];  // 每线程独占
    local_counter[omp_get_thread_num()]++;
}
```

---

##### 7. 伪共享在矩阵乘法中的典型灾难场景

矩阵 C 的写入通常按行进行。如果矩阵未 64B 对齐：

```
C[0][63]  C[1][0..1]   可能共享同一缓存行！
```

当线程 0 写入 C[0][*] 的最后几个元素，同时线程 1 写入 C[1][0..*] → 立刻触发伪共享。

**为什么常发生**

矩阵行大小：

```
N = 1000
float = 4B
行大小 = 4000B = 62.5 cache lines
```

如果 malloc 出来的起始地址不是 64 字节对齐：

* 行之间的边界会“错位”
* 行首尾会混进同一条 cache line
* 多线程对行写入就会产生伪共享

---

##### 8. 避免矩阵乘法伪共享的策略

- 1. 强制矩阵对齐

```c
float *C = aligned_alloc(64, M * N * sizeof(float));
```

确保行首对齐 → 行不会 overlap。

---

- 2. Block/Tiling（分块矩阵乘法）

让每个线程写入独立的矩阵块：

```
| Block0 | Block1 | Block2 |
| Block3 | Block4 | Block5 |
```

块大小应使“线程负责的区域由多个完整缓存行组成”。

---

- 3. NUMA-aware 设计（更高级）

如果是多 socket 机器：

* 让每个 NUMA 节点的线程写入其本地内存
* 避免跨 NUMA 访问导致内存抖动

（在并行矩阵乘法中非常关键）

---

##### 9. 如何检测伪共享？

常用工具：

| 工具                         | 平台            | 功能                   |
| -------------------------- | ------------- | -------------------- |
| `perf c2c`                 | Linux         | 检测 cache-to-cache 数据 |
| Intel VTune                | Windows/Linux | 内存瓶颈/缓存一致性分析         |
| Linux perf “L3 cache miss” | Linux         | 伪共享会导致 L3 miss 爆炸    |
| Valgrind Cachegrind        | Linux         | 缓存模拟                 |

伪共享的典型信号：

* L1/L2 cache miss 增高
* cache-to-cache 传输激增
* LLC（last-level cache）miss 飙升
* IPC（Instructions per cycle）下降到 0.2 以下


### 缓存优化技术

#### 1. 缓存层次结构

```
现代 CPU 缓存架构（以 Intel x86 为例）：

┌────────────────────────────────────────────┐
│              CPU 核心 0                    │
│  ┌──────────┐  ┌──────────┐               │
│  │ L1 数据   │  │ L1 指令  │               │
│  │ 32-64 KB │  │ 32 KB   │               │
│  │ 4 周期   │  │ 4 周期   │               │
│  └─────┬────┘  └────┬─────┘               │
│        └────────────┘                      │
│              │                             │
│         ┌────▼─────┐                       │
│         │   L2     │                       │
│         │ 256-512KB│                       │
│         │ 12 周期  │                       │
│         └────┬─────┘                       │
└──────────────┼─────────────────────────────┘
               │
        ┌──────▼──────┐
        │   L3 (共享)  │
        │  8-32 MB    │
        │  40 周期    │
        └──────┬──────┘
               │
        ┌──────▼──────┐
        │  主内存      │
        │  GB 级      │
        │  200 周期   │
        └─────────────┘

```

- 关键参数：
    - L1 缓存行大小：64 字节
    - L1 关联度：8 路组相联
    - L2 关联度：4-8 路
    - L3 关联度：12-16 路

- 性能对比：
    - L1 命中：~1 ns
    - L2 命中：~3 ns
    - L3 命中：~10 ns
    - 主内存：~60 ns
→ 相差 60 倍！