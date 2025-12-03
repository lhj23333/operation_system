#include "gpt2.h"
#include "matrix_parallel.h"
#include <sys/time.h>
#include <math.h>

static double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void test_softmax() {
    INFO_PRINT("=== Test: Softmax ===");
    
    size_t shape[] = {2, 3};
    Tensor *x = tensor_create(2, shape);
    
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    memcpy(x->data, data, 6 * sizeof(float));
    
    softmax_2d(x);
    
    // 验证和为 1
    for (size_t i = 0; i < 2; i++) {
        float sum = 0.0f;
        for (size_t j = 0; j < 3; j++) {
            sum += x->data[i * 3 + j];
        }
        ASSERT(fabsf(sum - 1.0f) < 1e-5f, "Softmax sum must be 1.0");
    }
    
    tensor_free(x);
    INFO_PRINT("✓ PASSED\n");
}

void test_single_head_attention() {
    INFO_PRINT("=== Test: Single-Head Attention ===");
    
    size_t seq_len = 4;
    size_t d_k = 8;
    size_t shape[] = {seq_len, d_k};
    
    Tensor *Q = tensor_create(2, shape);
    Tensor *K = tensor_create(2, shape);
    Tensor *V = tensor_create(2, shape);
    Tensor *output = tensor_create(2, shape);
    
    tensor_fill_random(Q, -1.0f, 1.0f);
    tensor_fill_random(K, -1.0f, 1.0f);
    tensor_fill_random(V, -1.0f, 1.0f);
    
    attention_single_head(Q, K, V, NULL, output);
    
    ASSERT(output->shape[0] == seq_len, "Output seq_len mismatch");
    ASSERT(output->shape[1] == d_k, "Output d_k mismatch");
    
    tensor_free(Q);
    tensor_free(K);
    tensor_free(V);
    tensor_free(output);
    
    INFO_PRINT("✓ PASSED\n");
}

void test_multi_head_attention_small() {
    INFO_PRINT("=== Test: Multi-Head Attention (Small) ===");
    
    size_t seq_len = 8;
    size_t d_model = 64;
    size_t num_heads = 4;
    
    // 创建输入
    size_t input_shape[] = {seq_len, d_model};
    Tensor *X = tensor_create(2, input_shape);
    tensor_fill_random(X, -1.0f, 1.0f);
    
    // 创建权重
    attention_weights_t weights;
    size_t weight_shape[] = {d_model, d_model};
    size_t bias_shape[] = {d_model};
    
    weights.W_Q = tensor_create(2, weight_shape);
    weights.W_K = tensor_create(2, weight_shape);
    weights.W_V = tensor_create(2, weight_shape);
    weights.W_O = tensor_create(2, weight_shape);
    weights.b_Q = tensor_create(1, bias_shape);
    weights.b_K = tensor_create(1, bias_shape);
    weights.b_V = tensor_create(1, bias_shape);
    weights.b_O = tensor_create(1, bias_shape);
    
    tensor_fill_random(weights.W_Q, -0.1f, 0.1f);
    tensor_fill_random(weights.W_K, -0.1f, 0.1f);
    tensor_fill_random(weights.W_V, -0.1f, 0.1f);
    tensor_fill_random(weights.W_O, -0.1f, 0.1f);
    
    memset(weights.b_Q->data, 0, d_model * sizeof(float));
    memset(weights.b_K->data, 0, d_model * sizeof(float));
    memset(weights.b_V->data, 0, d_model * sizeof(float));
    memset(weights.b_O->data, 0, d_model * sizeof(float));
    
    // 创建输出
    Tensor *output_serial = tensor_create(2, input_shape);
    Tensor *output_parallel = tensor_create(2, input_shape);
    
    // 初始化矩阵库
    matrix_config_t config = {
        .num_threads = 4,
        .block_size = 32,
        .use_blocking = true,
        .use_simd = false
    };
    matrix_init(&config);
    
    // 串行计算
    INFO_PRINT("Computing serial multi-head attention...");
    double start = get_time_ms();
    attention_multi_head_serial(X, &weights, num_heads, NULL, output_serial);
    double serial_time = get_time_ms() - start;
    INFO_PRINT("Serial time: %.2f ms", serial_time);
    
    // 并行计算
    INFO_PRINT("Computing parallel multi-head attention...");
    start = get_time_ms();
    attention_multi_head_parallel(X, &weights, num_heads, NULL, output_parallel);
    double parallel_time = get_time_ms() - start;
    INFO_PRINT("Parallel time: %.2f ms", parallel_time);
    INFO_PRINT("Speedup: %.2fx", serial_time / parallel_time);
    
    // 验证结果
    float max_diff = 0.0f;
    for (size_t i = 0; i < output_serial->size; i++) {
        float diff = fabsf(output_serial->data[i] - output_parallel->data[i]);
        if (diff > max_diff) max_diff = diff;
    }
    INFO_PRINT("Max difference: %.6e", max_diff);
    ASSERT(max_diff < 1e-3f, "Results mismatch");
    
    // 清理
    tensor_free(X);
    tensor_free(output_serial);
    tensor_free(output_parallel);
    tensor_free(weights.W_Q);
    tensor_free(weights.W_K);
    tensor_free(weights.W_V);
    tensor_free(weights.W_O);
    tensor_free(weights.b_Q);
    tensor_free(weights.b_K);
    tensor_free(weights.b_V);
    tensor_free(weights.b_O);
    matrix_cleanup();
    
    INFO_PRINT("✓ PASSED\n");
}

void test_multi_head_attention_large() {
    INFO_PRINT("=== Test: Multi-Head Attention (Large - GPT2 Scale) ===");
    
    // GPT-2 小模型规模: seq_len=128, d_model=768, num_heads=12
    size_t seq_len = 128;
    size_t d_model = 768;
    size_t num_heads = 12;
    
    INFO_PRINT("Config: seq_len=%zu, d_model=%zu, num_heads=%zu", seq_len, d_model, num_heads);
    INFO_PRINT("Matrix size: [%zu x %zu] @ [%zu x %zu]", seq_len, d_model, d_model, d_model);
    INFO_PRINT("Work size: %zu (threshold: %d)", seq_len * d_model * d_model, 64*64*64);
    
    // 创建输入
    size_t input_shape[] = {seq_len, d_model};
    Tensor *X = tensor_create(2, input_shape);
    tensor_fill_random(X, -1.0f, 1.0f);
    
    // 创建权重
    attention_weights_t weights;
    size_t weight_shape[] = {d_model, d_model};
    size_t bias_shape[] = {d_model};
    
    weights.W_Q = tensor_create(2, weight_shape);
    weights.W_K = tensor_create(2, weight_shape);
    weights.W_V = tensor_create(2, weight_shape);
    weights.W_O = tensor_create(2, weight_shape);
    weights.b_Q = tensor_create(1, bias_shape);
    weights.b_K = tensor_create(1, bias_shape);
    weights.b_V = tensor_create(1, bias_shape);
    weights.b_O = tensor_create(1, bias_shape);
    
    // Xavier 初始化
    float scale = sqrtf(2.0f / (d_model + d_model));
    tensor_fill_random(weights.W_Q, -scale, scale);
    tensor_fill_random(weights.W_K, -scale, scale);
    tensor_fill_random(weights.W_V, -scale, scale);
    tensor_fill_random(weights.W_O, -scale, scale);
    
    memset(weights.b_Q->data, 0, d_model * sizeof(float));
    memset(weights.b_K->data, 0, d_model * sizeof(float));
    memset(weights.b_V->data, 0, d_model * sizeof(float));
    memset(weights.b_O->data, 0, d_model * sizeof(float));
    
    // 创建输出
    Tensor *output_serial = tensor_create(2, input_shape);
    Tensor *output_parallel = tensor_create(2, input_shape);
    
    // 初始化矩阵库 - 使用更多线程
    matrix_config_t config = {
        .num_threads = 4,
        .block_size = 64,  // 更大的块大小
        .use_blocking = true,
        .use_simd = false
    };
    matrix_init(&config);
    
    // ===== 串行计算 =====
    INFO_PRINT("Computing serial multi-head attention...");
    double start = get_time_ms();
    attention_multi_head_serial(X, &weights, num_heads, NULL, output_serial);
    double serial_time = get_time_ms() - start;
    INFO_PRINT("Serial time: %.2f ms", serial_time);
    
    // ===== 并行计算 =====
    INFO_PRINT("Computing parallel multi-head attention...");
    start = get_time_ms();
    attention_multi_head_parallel(X, &weights, num_heads, NULL, output_parallel);
    double parallel_time = get_time_ms() - start;
    INFO_PRINT("Parallel time: %.2f ms", parallel_time);
    
    // ===== 性能报告 =====
    double speedup = serial_time / parallel_time;
    INFO_PRINT("╔════════════════════════════════════════╗");
    INFO_PRINT("║         Performance Report             ║");
    INFO_PRINT("╠════════════════════════════════════════╣");
    INFO_PRINT("║ Serial time:   %10.2f ms           ║", serial_time);
    INFO_PRINT("║ Parallel time: %10.2f ms           ║", parallel_time);
    INFO_PRINT("║ Speedup:       %10.2fx             ║", speedup);
    INFO_PRINT("║ Efficiency:    %10.1f%%            ║", (speedup / config.num_threads) * 100);
    INFO_PRINT("╚════════════════════════════════════════╝");
    
    // 验证结果
    float max_diff = 0.0f;
    float sum_diff = 0.0f;
    for (size_t i = 0; i < output_serial->size; i++) {
        float diff = fabsf(output_serial->data[i] - output_parallel->data[i]);
        sum_diff += diff;
        if (diff > max_diff) max_diff = diff;
    }
    float avg_diff = sum_diff / output_serial->size;
    INFO_PRINT("Max difference: %.6e", max_diff);
    INFO_PRINT("Avg difference: %.6e", avg_diff);
    ASSERT(max_diff < 1e-2f, "Results mismatch");  // 大矩阵允许稍大误差
    
    // 清理
    tensor_free(X);
    tensor_free(output_serial);
    tensor_free(output_parallel);
    tensor_free(weights.W_Q);
    tensor_free(weights.W_K);
    tensor_free(weights.W_V);
    tensor_free(weights.W_O);
    tensor_free(weights.b_Q);
    tensor_free(weights.b_K);
    tensor_free(weights.b_V);
    tensor_free(weights.b_O);
    matrix_cleanup();
    
    INFO_PRINT("✓ PASSED\n");
}

void benchmark_matmul() {
    INFO_PRINT("=== Benchmark: Matrix Multiplication ===");
    
    size_t sizes[] = {64, 128, 256, 512};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    matrix_config_t config = {
        .num_threads = 4,
        .block_size = 64,
        .use_blocking = true,
        .use_simd = false
    };
    matrix_init(&config);
    
    INFO_PRINT("╔══════════╦════════════╦════════════╦══════════╗");
    INFO_PRINT("║   Size   ║  Serial    ║  Parallel  ║ Speedup  ║");
    INFO_PRINT("╠══════════╬════════════╬════════════╬══════════╣");
    
    for (int s = 0; s < num_sizes; s++) {
        size_t N = sizes[s];
        size_t shape_a[] = {N, N};
        size_t shape_b[] = {N, N};
        size_t shape_c[] = {N, N};
        
        Tensor *A = tensor_create(2, shape_a);
        Tensor *B = tensor_create(2, shape_b);
        Tensor *C_serial = tensor_create(2, shape_c);
        Tensor *C_parallel = tensor_create(2, shape_c);
        
        tensor_fill_random(A, -1.0f, 1.0f);
        tensor_fill_random(B, -1.0f, 1.0f);
        
        // 串行
        double start = get_time_ms();
        matmul_serial_blocked(A, B, C_serial);
        double serial_time = get_time_ms() - start;
        
        // 并行
        start = get_time_ms();
        matmul_parallel_blocked(A, B, C_parallel);
        double parallel_time = get_time_ms() - start;
        
        double speedup = serial_time / parallel_time;
        
        INFO_PRINT("║ %4zux%-4zu ║ %8.2f ms ║ %8.2f ms ║  %5.2fx  ║",
                   N, N, serial_time, parallel_time, speedup);
        
        // 验证正确性
        float max_diff = 0.0f;
        for (size_t i = 0; i < C_serial->size; i++) {
            float diff = fabsf(C_serial->data[i] - C_parallel->data[i]);
            if (diff > max_diff) max_diff = diff;
        }
        ASSERT(max_diff < 1e-3f, "Matmul results mismatch");
        
        tensor_free(A);
        tensor_free(B);
        tensor_free(C_serial);
        tensor_free(C_parallel);
    }
    
    INFO_PRINT("╚══════════╩════════════╩════════════╩══════════╝");
    
    matrix_cleanup();
    INFO_PRINT("✓ PASSED\n");
}

int main() {
    INFO_PRINT("╔════════════════════════════════════════╗");
    INFO_PRINT("║   Attention Mechanism Tests            ║");
    INFO_PRINT("╚════════════════════════════════════════╝\n");
    
    test_softmax();
    test_single_head_attention();
    test_multi_head_attention_small();
    
    INFO_PRINT("\n");
    benchmark_matmul();
    
    INFO_PRINT("\n");
    test_multi_head_attention_large();
    
    INFO_PRINT("╔════════════════════════════════════════╗");
    INFO_PRINT("║   All Tests PASSED!                    ║");
    INFO_PRINT("╚════════════════════════════════════════╝");
    
    return 0;
}
