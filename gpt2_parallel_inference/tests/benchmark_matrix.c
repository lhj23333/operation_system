#include "matrix_parallel.h"
#include "tensor.h"
#include "common.h"
#include <sys/time.h>
#include <math.h>

static double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/**
 * 计算 GFLOPS
 * 
 * 矩阵乘法浮点运算数：2*M*N*K
 * GFLOPS = (2*M*N*K / 1e9) / (time_in_seconds)
 */
static double calculate_gflops(size_t M, size_t N, size_t K, double time_ms) {
    double flops = 2.0 * M * N * K;
    double gflops = (flops / 1e9) / (time_ms / 1000.0);
    return gflops;
}

/**
 * 验证结果正确性
 */
static bool verify_result(const Tensor *C1, const Tensor *C2, float epsilon) {
    if (C1->size != C2->size) return false;
    
    for (size_t i = 0; i < C1->size; i++) {
        float diff = fabsf(C1->data[i] - C2->data[i]);
        if (diff > epsilon) {
            ERROR_PRINT("Mismatch at index %zu: %.6f vs %.6f (diff=%.6f)",
                       i, C1->data[i], C2->data[i], diff);
            return false;
        }
    }
    
    return true;
}

/**
 * 性能基准测试
 */
matmul_benchmark_t matmul_benchmark(size_t M, size_t K, size_t N, int num_threads) {
    matmul_benchmark_t result = {0};
    
    INFO_PRINT("╔════════════════════════════════════════╗");
    INFO_PRINT("║  Matrix Multiplication Benchmark       ║");
    INFO_PRINT("╠════════════════════════════════════════╣");
    INFO_PRINT("║  Size: [%zu x %zu] @ [%zu x %zu]%-3s║", M, K, K, N, "");
    INFO_PRINT("║  Threads: %d%-28s║", num_threads, "");
    INFO_PRINT("╚════════════════════════════════════════╝");
    
    // 创建张量
    size_t shape_A[] = {M, K};
    size_t shape_B[] = {K, N};
    size_t shape_C[] = {M, N};
    
    Tensor *A = tensor_create(2, shape_A);
    Tensor *B = tensor_create(2, shape_B);
    Tensor *C_serial = tensor_create(2, shape_C);
    Tensor *C_parallel = tensor_create(2, shape_C);
    
    ASSERT(A && B && C_serial && C_parallel, "Tensor creation failed");
    
    // 初始化随机数据
    INFO_PRINT("Initializing matrices with random data...");
    tensor_fill_random(A, -1.0f, 1.0f);
    tensor_fill_random(B, -1.0f, 1.0f);
    
    // 初始化矩阵库
    matrix_config_t config = {
        . num_threads = num_threads,
        .block_size = 32,
        .use_blocking = true,
        .use_simd = false
    };
    matrix_init(&config);
    
    // === 测试 1：串行版本（基线）===
    INFO_PRINT("\n[1/4] Running serial matmul (ijk)...");
    double start = get_time_ms();
    matmul_serial(A, B, C_serial);
    result.serial_time = get_time_ms() - start;
    result.gflops = calculate_gflops(M, N, K, result.serial_time);
    
    INFO_PRINT("  Time: %.2f ms, Performance: %.2f GFLOPS",
               result.serial_time, result.gflops);
    
    // === 测试 2：串行优化版本（ikj）===
    INFO_PRINT("\n[2/4] Running serial matmul (ikj).. .");
    Tensor *C_ikj = tensor_create(2, shape_C);
    start = get_time_ms();
    matmul_serial_ikj(A, B, C_ikj);
    double time_ikj = get_time_ms() - start;
    double gflops_ikj = calculate_gflops(M, N, K, time_ikj);
    
    INFO_PRINT("  Time: %.2f ms, Performance: %.2f GFLOPS",
               time_ikj, gflops_ikj);
    INFO_PRINT("  Speedup vs ijk: %.2fx", result.serial_time / time_ikj);
    
    // 验证正确性
    ASSERT(verify_result(C_serial, C_ikj, 1e-3), "ikj result mismatch");
    tensor_free(C_ikj);
    
    // === 测试 3：串行分块版本 ===
    INFO_PRINT("\n[3/4] Running serial matmul (blocked).. .");
    Tensor *C_blocked = tensor_create(2, shape_C);
    start = get_time_ms();
    matmul_serial_blocked(A, B, C_blocked);
    double time_blocked = get_time_ms() - start;
    double gflops_blocked = calculate_gflops(M, N, K, time_blocked);
    
    INFO_PRINT("  Time: %.2f ms, Performance: %.2f GFLOPS",
               time_blocked, gflops_blocked);
    INFO_PRINT("  Speedup vs ijk: %.2fx", result.serial_time / time_blocked);
    
    ASSERT(verify_result(C_serial, C_blocked, 1e-3), "blocked result mismatch");
    tensor_free(C_blocked);
    
    // === 测试 4：并行版本 ===
    INFO_PRINT("\n[4/4] Running parallel matmul.. .");
    start = get_time_ms();
    matmul_parallel_blocked(A, B, C_parallel);
    result.parallel_time = get_time_ms() - start;
    
    // 验证正确性
    ASSERT(verify_result(C_serial, C_parallel, 1e-3), "Parallel result mismatch");
    
    // 计算性能指标
    result.speedup = result.serial_time / result.parallel_time;
    result.efficiency = result.speedup / num_threads;
    double parallel_gflops = calculate_gflops(M, N, K, result.parallel_time);
    
    INFO_PRINT("  Time: %.2f ms, Performance: %.2f GFLOPS",
               result.parallel_time, parallel_gflops);
    INFO_PRINT("  Speedup: %.2fx", result.speedup);
    INFO_PRINT("  Efficiency: %.2f%%", result.efficiency * 100);
    
    // === 总结 ===
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║          Performance Summary                   ║\n");
    printf("╠════════════════════════════════════════════════╣\n");
    printf("║ Serial (ijk):       %8.2f ms  %6.2f GFLOPS ║\n",
           result.serial_time, result.gflops);
    printf("║ Serial (ikj):       %8.2f ms  %6.2f GFLOPS ║\n",
           time_ikj, gflops_ikj);
    printf("║ Serial (blocked):   %8.2f ms  %6.2f GFLOPS ║\n",
           time_blocked, gflops_blocked);
    printf("║ Parallel (blocked): %8.2f ms  %6.2f GFLOPS ║\n",
           result.parallel_time, parallel_gflops);
    printf("╠════════════════════════════════════════════════╣\n");
    printf("║ Speedup (ikj):      %.2fx%-21s║\n",
           result.serial_time / time_ikj, "");
    printf("║ Speedup (blocked):  %.2fx%-21s║\n",
           result.serial_time / time_blocked, "");
    printf("║ Speedup (parallel): %.2fx%-21s║\n",
           result.speedup, "");
    printf("║ Parallel efficiency: %.1f%%%-20s║\n",
           result.efficiency * 100, "");
    printf("╚════════════════════════════════════════════════╝\n");
    
    // 清理
    tensor_free(A);
    tensor_free(B);
    tensor_free(C_serial);
    tensor_free(C_parallel);
    matrix_cleanup();
    
    return result;
}

/**
 * 多尺寸基准测试
 */
void run_size_sweep() {
    INFO_PRINT("\n╔════════════════════════════════════════╗");
    INFO_PRINT("║      Matrix Size Sweep Test            ║");
    INFO_PRINT("╚════════════════════════════════════════╝\n");
    
    size_t sizes[] = {128, 256, 512, 768, 1024};
    int num_threads = 4;
    
    printf("Size\tSerial(ms)\tParallel(ms)\tSpeedup\tGFLOPS\n");
    printf("────\t──────────\t────────────\t───────\t──────\n");
    
    for (size_t i = 0; i < ARRAY_SIZE(sizes); i++) {
        size_t N = sizes[i];
        
        matmul_benchmark_t result = matmul_benchmark(N, N, N, num_threads);
        
        double gflops = calculate_gflops(N, N, N, result.parallel_time);
        
        printf("%zu\t%.2f\t\t%.2f\t\t%.2fx\t%.2f\n",
               N, result.serial_time, result.parallel_time,
               result.speedup, gflops);
    }
}

/**
 * 线程数扩展性测试
 */
void run_thread_scaling() {
    INFO_PRINT("\n╔════════════════════════════════════════╗");
    INFO_PRINT("║     Thread Scaling Test                ║");
    INFO_PRINT("╚════════════════════════════════════════╝\n");
    
    size_t N = 1024;
    int thread_counts[] = {1, 2, 4, 8};
    
    printf("Threads\tTime(ms)\tSpeedup\tEfficiency\n");
    printf("───────\t────────\t───────\t──────────\n");
    
    double baseline_time = 0;
    
    for (size_t i = 0; i < ARRAY_SIZE(thread_counts); i++) {
        int num_threads = thread_counts[i];
        
        matmul_benchmark_t result = matmul_benchmark(N, N, N, num_threads);
        
        if (i == 0) {
            baseline_time = result.parallel_time;
        }
        
        double speedup = baseline_time / result.parallel_time;
        double efficiency = speedup / num_threads;
        
        printf("%d\t%.2f\t\t%.2fx\t%.1f%%\n",
               num_threads, result.parallel_time,
               speedup, efficiency * 100);
    }
}

int main(int argc, char **argv) {
    INFO_PRINT("╔════════════════════════════════════════╗");
    INFO_PRINT("║   Matrix Multiplication Benchmark      ║");
    INFO_PRINT("╚════════════════════════════════════════╝\n");
    
    if (argc > 1 && strcmp(argv[1], "--sweep") == 0) {
        run_size_sweep();
    } else if (argc > 1 && strcmp(argv[1], "--scaling") == 0) {
        run_thread_scaling();
    } else {
        // 默认：单个测试
        size_t N = 1024;
        int num_threads = 4;
        
        if (argc > 2) {
            N = atoi(argv[1]);
            num_threads = atoi(argv[2]);
        }
        
        matmul_benchmark(N, N, N, num_threads);
    }
    
    return 0;
}