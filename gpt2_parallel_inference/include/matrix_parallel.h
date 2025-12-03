#ifndef __MATRIX_PARALLEL_H__
#define __MATRIX_PARALLEL_H__

#include "common.h"
#include "tensor.h"
#include "thread_pool.h"

/**
 * 矩阵运算配置结构体
 */
typedef struct {
    int num_threads;        // 使用的线程数
    size_t block_size;      // 每个线程处理的块大小
    bool use_blocking;      // 分块处理标志
    bool use_simd;          // 是否使用 SIMD 优化
} matrix_config_t;

/**
 * @name matrix_init
 * @brief 初始化矩阵运算库
 * 
 * @param config 矩阵运算配置结构体指针
 * 
 * @return
 *       success - 0
 *       failure - -1
 */
int matrix_init(const matrix_config_t* config);


/**
 * @name matrix_cleanup
 * @brief 矩阵运算库反初始化
 * 
 * @return void
 */
void matrix_cleanup(void);


/**
 * @name matmul_serial
 * @brief 矩阵串行乘法
 * 
 * @param A 输入矩阵 A
 * @param B 输入矩阵 B
 * @param C 输出矩阵 C
 * 
 * @return void
 */
void matmul_serial(const Tensor* A, const Tensor* B, Tensor* C);


/**
 * @name matmul_serial_ikj
 * @brief 矩阵串行乘法 (IKJ 优化)
 * 
 * @param A 输入矩阵 A
 * @param B 输入矩阵 B
 * @param C 输出矩阵 C
 * 
 * @return void
 */
void matmul_serial_ikj(const Tensor* A, const Tensor* B, Tensor* C);


/**
 * @name matmul_serial_blocked
 * @brief 矩阵串行乘法 (分块优化)
 * 
 * @param A 输入矩阵 A
 * @param B 输入矩阵 B
 * @param C 输出矩阵 C
 * 
 * @return void
 */
void matmul_serial_blocked(const Tensor* A, const Tensor* B, Tensor* C);


/**
 * @name matmul_parallel_row
 * @brief 矩阵并行乘法 (行分块)
 * 
 * @param A 输入矩阵 A
 * @param B 输入矩阵 B
 * @param C 输出矩阵 C
 * 
 * @return void
 */
void matmul_parallel_row(const Tensor* A, const Tensor* B, Tensor* C);


/**
 * @name matmul_parallel_blocked
 * @brief 矩阵并行乘法 (行分块 + 缓存优化)
 * 
 * @param A 输入矩阵 A
 * @param B 输入矩阵 B
 * @param C 输出矩阵 C
 * 
 * @return void
 */
void matmul_parallel_blocked(const Tensor* A, const Tensor* B, Tensor* C);


/**
 * 性能测试
 */
typedef struct {
    double serial_time;
    double parallel_time;
    double gflops;
    double speedup;
    double efficiency;
} matmul_benchmark_t;

/**
 * @name matmul_benchmark
 * @brief 矩阵乘法性能测试
 * 
 * @param M 矩阵 A 的行数
 * @param K 矩阵 A 的列数 / 矩阵 B 的行数
 * @param N 矩阵 B 的列数
 * @param num_threads 使用的线程数
 */
matmul_benchmark_t matmul_benchmark(size_t M, size_t K, size_t N, int num_threads);


/**
 * @name matrix_get_thread_pool
 * @brief 获取全局矩阵计算线程池
 * 
 * @return thread_pool_t* 线程池指针，未初始化时返回 NULL
 */
thread_pool_t* matrix_get_thread_pool(void);

#endif /* __MATRIX_PARALLEL_H__ */