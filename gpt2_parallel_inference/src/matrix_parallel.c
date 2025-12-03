#include "matrix_parallel.h"
#include <sys/time.h>

// 并行化阈值：小于此值使用串行版本
#define PARALLEL_THRESHOLD (64 * 64 * 64)   // M * K * N 的乘积阈值
#define MIN_ROWS_PER_TASK 4                 // 每个任务最少处理的行数

static matrix_config_t g_matrix_cfg;
static thread_pool_t *g_thread_pool = NULL;

int matrix_init(const matrix_config_t *cfg) {
    if (cfg == NULL) {
        ERROR_PRINT("Configuration is NULL");
        return -1;
    }

    INFO_PRINT("Initializing matrix library (threads: %d, block_size: %zu)",
               cfg->num_threads, cfg->block_size);
    g_matrix_cfg = *cfg;

    // 初始化线程池
    thread_pool_cfg_t pool_cfg = {
        .num_threads = cfg->num_threads,
        .queue_size = 1024,
        .stack_size = 0,
        .daemon_threads = false
    };
    g_thread_pool = thread_pool_create(&pool_cfg);
    if (g_thread_pool == NULL) {
        ERROR_PRINT("Failed to create thread pool");
        return -1;
    }

    INFO_PRINT("Matrix library initialized successfully");
    return 0;
}


void matrix_cleanup(void) {
    if (g_thread_pool == NULL) return;

    INFO_PRINT("Cleaning up matrix library");

    thread_pool_destroy(g_thread_pool);
    g_thread_pool = NULL;

    INFO_PRINT("Matrix library cleaned up successfully");
}


void matmul_serial(const Tensor *A, const Tensor *B, Tensor *C) {
    ASSERT(A != NULL && B != NULL && C != NULL, "NULL tensor");
    ASSERT(A->ndim == 2 && B->ndim == 2 && C->ndim == 2, "Must be 2D");

    size_t M = A->shape[0];
    size_t K = A->shape[1];
    size_t N = B->shape[1];

    ASSERT(K == B->shape[0], "Dimension mismatch");
    ASSERT(M == C->shape[0] && N == C->shape[1], "Output size mismatch");

    DEBUG_PRINT("Serial matmul: [%zu x %zu] @ [%zu x %zu]", M, K, K, N);

    memset(C->data, 0, C->size * sizeof(float));

    // ijk 循环
    for (size_t i = 0; i < M; i ++) {
        for (size_t j = 0; j < N; j ++) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; k ++) {
                sum += A->data[i*K + k]*B->data[k*N + j];
            }
            C->data[i*N + j] = sum;
        }
    }
}

void matmul_serial_ikj(const Tensor *A, const Tensor *B, Tensor *C) {
    ASSERT(A != NULL && B != NULL && C != NULL, "NULL tensor");
    ASSERT(A->ndim == 2 && B->ndim == 2 && C->ndim == 2, "Must be 2D");

    size_t M = A->shape[0];
    size_t K = A->shape[1];
    size_t N = B->shape[1];

    ASSERT(K == B->shape[0], "Dimension mismatch");
    ASSERT(M == C->shape[0] && N == C->shape[1], "Output size mismatch");

    DEBUG_PRINT("Serial matmul (ikj): [%zu x %zu] @ [%zu x %zu]", M, K, K, N);

    memset(C->data, 0, C->size * sizeof(float));

    // ikj 循环：
    for (size_t i = 0; i < M; i ++) {
        for (size_t k = 0; k < K; k ++) {
            float a_ik = A->data[i*K + k];
            for (size_t j = 0; j < N; j ++) {
                C->data[i*N + j] += a_ik*B->data[k*N + j];
            }
        }
    }
}


void matmul_serial_blocked(const Tensor *A, const Tensor *B, Tensor *C) {
    ASSERT(A != NULL && B != NULL && C != NULL, "NULL tensor");
    ASSERT(A->ndim == 2 && B->ndim == 2 && C->ndim == 2, "Must be 2D");

    size_t M = A->shape[0];
    size_t K = A->shape[1];
    size_t N = B->shape[1];
    size_t block_size = g_matrix_cfg.block_size;

    ASSERT(K == B->shape[0], "Dimension mismatch");
    ASSERT(M == C->shape[0] && N == C->shape[1], "Output size mismatch");

    DEBUG_PRINT("Serial blocked matmul: [%zu x %zu] @ [%zu x %zu], block_size=%zu",
                M, K, K, N, block_size);
    
    memset(C->data, 0, C->size * sizeof(float));

    for (size_t ii = 0; ii < M; ii += block_size) {
        for (size_t kk = 0; kk < K; kk += block_size) {
            for (size_t jj = 0; jj < N; jj += block_size) {
                // 计算块边界
                size_t i_end = MIN(ii + block_size, M);
                size_t k_end = MIN(kk + block_size, K);
                size_t j_end = MIN(jj + block_size, N);

                for (size_t i = ii; i < i_end; i ++) {
                    for (size_t k = kk; k < k_end; k ++) {
                        float a_ik = A->data[i*K + k];
                        for (size_t j = jj; j < j_end; j ++) {
                            C->data[i*N + j] += a_ik*B->data[k*N + j];
                        }
                    }
                }
            }
        }
    }
}

/**
 * 行分块任务参数
 */
typedef struct {
    const Tensor *A;
    const Tensor *B;
    Tensor *C;
    size_t row_start;
    size_t row_end;
    int thread_id;
} matmul_row_task_t;


static void matmul_row_task(void* arg) {
    matmul_row_task_t *task = (matmul_row_task_t *)arg;

    size_t K = task->A->shape[1];
    size_t N = task->B->shape[1];

    DEBUG_PRINT("Thread %d: Computing rows [%zu, %zu)",
                task->thread_id, task->row_start, task->row_end);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (size_t i = task->row_start; i < task->row_end; i ++) {
        for (size_t k = 0; k < K; k ++) {
            float a_ik = task->A->data[i*K + k];
            for (size_t j = 0; j < N; j ++) {
                task->C->data[i*N + j] += a_ik*task->B->data[k*N + j];
            }
        }
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                     (end.tv_usec - start.tv_usec) / 1000.0;

    DEBUG_PRINT("Thread %d: Completed in %.2f ms", task->thread_id, elapsed);

    free(task);
}

void matmul_parallel_row(const Tensor *A, const Tensor *B, Tensor *C) {
    ASSERT(A != NULL && B != NULL && C != NULL, "NULL tensor");
    ASSERT(A->ndim == 2 && B->ndim == 2 && C->ndim == 2, "Must be 2D");

    size_t M = A->shape[0];
    size_t K = A->shape[1];
    size_t N = B->shape[1];

    ASSERT(K == B->shape[0], "Dimension mismatch");
    ASSERT(M == C->shape[0] && N == C->shape[1], "Output size mismatch");

    // // 小矩阵直接使用串行版本
    // size_t work_size = M * K * N;
    // if (work_size < PARALLEL_THRESHOLD || M < (size_t)g_matrix_cfg.num_threads) {
    //     DEBUG_PRINT("Matrix too small (%zu), using serial version", work_size);
    //     matmul_serial_ikj(A, B, C);
    //     return;
    // }

    INFO_PRINT("Parallel row-wise matmul: [%zu x %zu] @ [%zu x %zu] (threads: %d)",
                M, K, K, N, g_matrix_cfg.num_threads);

    memset(C->data, 0, C->size * sizeof(float));

    int num_threads = g_matrix_cfg.num_threads;

    // 计算每个线程处理的行数（粒度优化）
    size_t rows_per_thread = (M + num_threads - 1) / num_threads;
    rows_per_thread = MAX(MIN_ROWS_PER_TASK, rows_per_thread);

    DEBUG_PRINT("Rows per thread: %zu", rows_per_thread);

    // 提交任务到线程池
    int tasks_submitted = 0;
    for (size_t start = 0; start < M; start += rows_per_thread) {
        size_t end = MIN(start + rows_per_thread, M);
        
        matmul_row_task_t *task = (matmul_row_task_t *)malloc(sizeof(matmul_row_task_t));
        ASSERT(task != NULL, "Failed to allocate task");

        task->A = A;
        task->B = B;
        task->C = C;
        task->row_start = start;
        task->row_end = end;
        task->thread_id = tasks_submitted;

        int ret = thread_pool_submit(g_thread_pool, matmul_row_task, (void *)task, NULL);
        ASSERT(ret == 0, "Failed to submit task to thread pool");

        tasks_submitted ++;
    }

    DEBUG_PRINT("Submitted %d tasks to thread pool", tasks_submitted);

    // 等待所有任务完成
    thread_pool_wait_all(g_thread_pool);

    INFO_PRINT("Parallel matmul completed");
}

static void matmul_blocked_task(void* arg) {
    matmul_row_task_t *task = (matmul_row_task_t *)arg;

    size_t K = task->A->shape[1];
    size_t N = task->B->shape[1];
    size_t block_size = g_matrix_cfg.block_size;
    
    DEBUG_PRINT("Thread %d: Computing rows [%zu, %zu) with block_size %zu",
                task->thread_id, task->row_start, task->row_end, block_size);

    // 分块计算
    for (size_t ii = task->row_start; ii < task->row_end; ii += block_size) {
        for (size_t kk = 0; kk < K; kk += block_size) {
            for (size_t jj = 0; jj < N; jj += block_size) {
                // 计算块边界
                size_t i_end = MIN(ii + block_size, task->row_end);
                size_t k_end = MIN(kk + block_size, K);
                size_t j_end = MIN(jj + block_size, N);

                for (size_t i = ii; i < i_end; i ++) {
                    for (size_t k = kk; k < k_end; k ++) {
                        float a_ik = task->A->data[i*K + k];  // 修复: i*k -> i*K
                        for (size_t j = jj; j < j_end; j ++) {
                            task->C->data[i*N + j] += a_ik*task->B->data[k*N + j];
                        }
                    }
                }
            }
        }
    }

    free(task);
}

void matmul_parallel_blocked(const Tensor *A, const Tensor *B, Tensor *C) {
    ASSERT(A != NULL && B != NULL && C != NULL, "NULL tensor");
    ASSERT(A->ndim == 2 && B->ndim == 2 && C->ndim == 2, "Must be 2D");

    size_t M = A->shape[0];
    size_t K = A->shape[1];
    size_t N = B->shape[1];

    ASSERT(K == B->shape[0], "Dimension mismatch");
    ASSERT(M == C->shape[0] && N == C->shape[1], "Output size mismatch");

    // // 小矩阵直接使用串行版本
    // size_t work_size = M * K * N;
    // if (work_size < PARALLEL_THRESHOLD || M < (size_t)g_matrix_cfg.num_threads) {
    //     DEBUG_PRINT("Matrix too small (%zu), using serial blocked version", work_size);
    //     matmul_serial_blocked(A, B, C);
    //     return;
    // }

    INFO_PRINT("Parallel blocked matmul: [%zu x %zu] @ [%zu x %zu] (threads: %d, block_size: %zu)",
                M, K, K, N, g_matrix_cfg.num_threads, g_matrix_cfg.block_size);

    memset(C->data, 0, C->size * sizeof(float));

    int num_threads = g_matrix_cfg.num_threads;

    // 计算每个线程处理的行数（粒度优化）
    size_t rows_per_thread = (M + num_threads - 1) / num_threads;
    rows_per_thread = MAX(MIN_ROWS_PER_TASK, rows_per_thread);

    DEBUG_PRINT("Rows per thread: %zu", rows_per_thread);

    // 提交任务到线程池
    int tasks_submitted = 0;
    for (size_t start = 0; start < M; start += rows_per_thread) {
        size_t end = MIN(start + rows_per_thread, M);
        
        matmul_row_task_t *task = (matmul_row_task_t *)malloc(sizeof(matmul_row_task_t));
        ASSERT(task != NULL, "Failed to allocate task");

        task->A = A;
        task->B = B;
        task->C = C;
        task->row_start = start;
        task->row_end = end;
        task->thread_id = tasks_submitted;

        int ret = thread_pool_submit(g_thread_pool, matmul_blocked_task, (void *)task, NULL);
        ASSERT(ret == 0, "Failed to submit task to thread pool");

        tasks_submitted ++;
    }

    DEBUG_PRINT("Submitted %d tasks to thread pool", tasks_submitted);

    // 等待所有任务完成
    thread_pool_wait_all(g_thread_pool);

    INFO_PRINT("Parallel blocked matmul completed");
}

thread_pool_t* matrix_get_thread_pool(void) {
    return g_thread_pool;
}