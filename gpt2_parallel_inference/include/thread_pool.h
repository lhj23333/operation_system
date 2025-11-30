#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include "common.h"
#include "task_queue.h"
#include <pthread.h>

/**
 * 线程池结构体
 */
typedef struct {
    int num_threads;        // 工作线程数量
    int queue_size;         // 任务队列大小
    int stack_size;         // 线程栈大小（可选）
    bool daemon_threads;    // 是否为守护线程
} thread_pool_cfg_t;

/**
 * 线程信息（用于监控）
 */
typedef struct {
    pthread_t tid;                  // 线程ID
    int id;                         // 线程编号
    size_t tasks_completed;         // 已完成任务数
    bool is_active;                 // 线程活跃状态
    bool should_exit;               // 线程退出标志位（用于动态调整线程数）
} thread_info_t;

/**
 * 线程池状态枚举
 */
typedef enum {
    POOL_CREATED = 0,   // 已创建
    POOL_RUNNING = 1,   // 正在运行
    POOL_STOPPING = 2, // 正在停止
    POOL_STOPPED = 3    // 已停止
} thread_pool_state_t;

/**
 * 线程池结构体
 * 
 * 核心设计：
 * - 多层同步机制
 * - 任务队列与线程分离
 * - 线程状态监控
 */
typedef struct {
    /* 线程管理 */
    pthread_t *threads;             // 线程数组
    thread_info_t *thread_infos;    // 线程信息数组
    int num_threads;                // 线程数量

    /* 任务队列 */
    task_queue_t *task_queue;       // 任务队列指针

    /* 状态管理 */
    thread_pool_state_t state;      // 线程池状态
    pthread_mutex_t state_lock;     // 状态互斥锁
    pthread_cond_t state_changed;   // 状态条件变量

    // /* 统计信息 */
    // struct {
    //     size_t total_tasks;         // 总任务数
    //     size_t failed_tasks;        // 失败任务数
    //     double total_time;          // 总时间
    // } stats;

    /* 线程关闭标志 */
    volatile bool shutdown;
} thread_pool_t;

/* ============= 线程池 API ============= */

/**
 * @name thread_pool_create
 * @brief 创建线程池
 * 
 * @param cfg 线程池配置参数
 * 
 * @return 
 *      successful: 线程池指针
 *      failed: NULL
 */
thread_pool_t* thread_pool_create(const thread_pool_cfg_t *cfg);


/**
 * @name thread_pool_destroy
 * @brief 销毁线程池
 * 
 * @param pool 线程池指针
 * 
 * @return void
 */
void thread_pool_destroy(thread_pool_t *pool);


/**
 * @name thread_pool_submit
 * @brief 提交任务
 * 
 * @param pool 线程池指针
 * @param func 任务函数指针
 * @param arg 任务参数指针
 * @param cleanup 任务清理函数指针（可选）
 * 
 * @return
 *      successful: 0
 *      failed: -1
 */

int thread_pool_submit(thread_pool_t *pool, void (*func)(void *), void *arg, void (*cleanup)(void *));


/**
 * @name thread_pool_wait_all
 * @brief 等待所有任务完成
 * 
 * @param pool 线程池指针
 * 
 * @return void
 */
void thread_pool_wait_all(thread_pool_t *pool);


/**
 * @name thread_pool_shutdown
 * @brief 关闭线程池
 * 
 * @param pool 线程池指针
 * 
 * @return void
 */
void thread_pool_shutdown(thread_pool_t *pool);


/**
 * @name thread_pool_print_info
 * @brief 打印线程池信息
 * 
 * @param pool 线程池指针
 * 
 * @return void
 */
void thread_pool_print_info(const thread_pool_t *pool);


/**
 * @name thread_pool_resize
 * @brief 动态调整线程池大小
 * 
 * @param pool 线程池指针
 * @param new_size 新线程数量
 * 
 * @return
 *      successful: 0
 *      failed: -1
 */
int thread_pool_resize(thread_pool_t *pool, int new_size);

#endif /* __THREAD_POOL_H__ */