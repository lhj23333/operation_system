#ifndef __TASK_QUEUE_H__
#define __TASK_QUEUE_H__

#include "common.h"
#include <pthread.h>

/**
 * @brief 任务结构体
 * 
 * 设计：
 * - 支持任意类型的任务数据，通过 void* 指针传递
 * - 任务处理函数通过函数指针传递，支持多种任务处理逻辑
 */
typedef struct task {
    void (*function)(void *arg);        // 任务处理函数指针
    void *arg;                          // 任务数据指针
    void (*cleanup)(void *arg);         // 任务清理函数指针（可选）
    struct task *next;                  // 链表指针
} Task;

/**
 * @brief 任务队列结构体
 * 
 * 设计：
 * - 使用链表实现任务队列，支持动态添加和移除任务
 * - 通过互斥锁和条件变量实现线程安全的任务添加和获取
 * - 支持任务队列的销毁，释放所有资源
 * - not-full 等条件变量（背压机制）
 */
typedef struct {
    /* 队列数据 */
    Task *front;
    Task *back;

    /* 队列状态 */
    size_t count;                       // 当前任务数量
    size_t max_count;                   // 最大任务数量（0 表示无限制）
    size_t total_processed;             // 总处理任务数量
    size_t active_tasks;                // 正在执行的任务数量

    /* 同步机制 */
    pthread_mutex_t mutex;              // 互斥锁
    pthread_cond_t cond_not_empty;      // 非空条件变量
    pthread_cond_t cond_not_full;       // 非满条件变量
    pthread_cond_t cond_empty;          // 空条件变量
    pthread_cond_t cond_all_done;       // 所有任务完成条件变量

    /* 统计信息 */
    struct {
        size_t total_enqueued;          // 总入队任务数量
        size_t total_dequeued;          // 总出队任务数量
        double avg_wait_time;           // 平均等待时间
    } stats;
} task_queue_t;

/* ========== 任务 API ========== */

/**
 * @brief 提交任务到队列
 * @param queue 队列指针
 * @param func 任务函数
 * @param arg 任务参数
 * @param cleanup 清理函数（可选）
 * @return 0成功，-1失败
 */
int task_queue_submit(task_queue_t *queue, 
                      void (*func)(void*), 
                      void *arg,
                      void (*cleanup)(void*));


/* ========== 队列 API ========== */

/**
 * @name task_queue_create
 * @brief 创建并初始化任务队列
 * 
 * @param max_count 最大任务数量（0 表示无限制）
 * 
 * @return 
 *      successful: 指向新创建任务队列的指针
 *      failed: NULL
 */
task_queue_t* task_queue_create(size_t max_count);


/**
 * @name task_queue_destroy
 * @brief 销毁任务队列，释放所有资源
 * 
 * @param queue 指向任务队列的指针
 * 
 * @return void
 */
void task_queue_destroy(task_queue_t *queue);


/**
 * @name task_queue_push
 * @brief 入队（生产者）
 * 
 * @param queue 指向任务队列的指针
 * @param task  指向要入队的任务的指针
 * 
 * @return
 *      successful: 0
 *      failed: -1
 */
int task_queue_push(task_queue_t *queue, Task *task);


/**
 * @name task_queue_pop
 * @brief 出队（消费者）
 * 
 * @param queue     指向任务队列的指针
 * @param shutdown  指示是否在关闭时退出
 * 
 * @return
 *      successful: 指向出队任务的指针 
 *            NULL：队列关闭且无任务可用
 */
Task* task_queue_pop(task_queue_t *queue, volatile bool *shutdown);


/**
 * @name task_queue_wait_empty
 * @brief 等待队列为空 - 用于优雅关闭：等到所有任务处理完成
 * 
 * @param queue 指向任务队列的指针
 * 
 * @return void
 */
void task_queue_wait_empty(task_queue_t *queue);


/**
 * @name task_queue_get_count
 * @brief 获取当前队列大小
 * 
 * @param queue 指向任务队列的指针
 * 
 * @return 当前队列大小
 */
size_t task_queue_get_count(task_queue_t *queue);


/**
 * @name task_queue_print_stats
 * @brief 打印队列统计信息
 * 
 * @param queue 指向任务队列的指针
 * 
 * @return void
 */
void task_queue_print_stats(task_queue_t *queue);


/**
 * @brief 从队列中获取并执行一个任务（高层API）
 * 
 * 此函数封装了任务的获取、执行和销毁的完整生命周期。
 * thread_pool 应该使用此函数而不是直接操作 Task 结构体。
 * 
 * @param queue 队列指针
 * @param should_shutdown 关闭标志指针（由调用者保证线程安全访问）
 * 
 * @return 
 *      0: 成功执行一个任务
 *      1: 收到关闭信号且队列为空，应该退出
 *     -1: 错误
 */
int task_queue_pop_and_execute(task_queue_t *queue, bool *should_shutdown);

#endif /* __TASK_QUEUE_H__ */
