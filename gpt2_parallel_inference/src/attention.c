#include "gpt2.h"
#include "matrix_parallel.h"
#include <math.h>
#include <sys/time.h>

/* ========== tool function ========== */

static double _get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/* ========== softmax function ========== */
void softmax_2d(Tensor *x) {
    ASSERT(x != NULL, "Tensor is NULL");
    ASSERT(x->ndim == 2, "Tensor must be 2D");
    
    size_t M = x->shape[0];
    size_t N = x->shape[1];
    
    DEBUG_PRINT("Computing softmax for tensor [%zu, %zu]", M, N);
    
    for (size_t i = 0; i < M; i++) {
        float *row = &x->data[i * N];
        
        // 找最大值（数值稳定性）
        float max_val = row[0];
        for (size_t j = 1; j < N; j++) {
            if (row[j] > max_val) {
                max_val = row[j];
            }
        }
        
        // 计算 exp 并求和
        float sum = 0.0f;
        for (size_t j = 0; j < N; j++) {
            row[j] = expf(row[j] - max_val);
            sum += row[j];
        }
        
        // 归一化
        if (sum < 1e-10f) {
            WARN_PRINT("Softmax sum near zero, using uniform");
            float uniform = 1.0f / N;
            for (size_t j = 0; j < N; j++) {
                row[j] = uniform;
            }
        } else {
            for (size_t j = 0; j < N; j++) {
                row[j] /= sum;
            }
        }
    }
    
    DEBUG_PRINT("Softmax completed");
}

/* ========== Layer Normalization ========== */

void layer_norm(Tensor *x, const Tensor *gamma, const Tensor *beta, float eps) {
    ASSERT(x != NULL && gamma != NULL && beta != NULL, "NULL tensor");
    ASSERT(x->ndim == 2, "Input must be 2D");
    ASSERT(gamma->ndim == 1 && beta->ndim == 1, "gamma/beta must be 1D");
    
    size_t seq_len = x->shape[0];
    size_t hidden_dim = x->shape[1];
    
    ASSERT(gamma->shape[0] == hidden_dim, "gamma size mismatch");
    ASSERT(beta->shape[0] == hidden_dim, "beta size mismatch");
    
    DEBUG_PRINT("Layer norm: seq_len=%zu, hidden_dim=%zu", seq_len, hidden_dim);
    
    for (size_t i = 0; i < seq_len; i++) {
        float *data = x->data + i * hidden_dim;
        
        // 计算均值
        float mean = 0.0f;
        for (size_t j = 0; j < hidden_dim; j++) {
            mean += data[j];
        }
        mean /= hidden_dim;
        
        // 计算方差
        float var = 0.0f;
        for (size_t j = 0; j < hidden_dim; j++) {
            float diff = data[j] - mean;
            var += diff * diff;
        }
        var /= hidden_dim;
        
        // 归一化
        float inv_std = 1.0f / sqrtf(var + eps);
        for (size_t j = 0; j < hidden_dim; j++) {
            data[j] = gamma->data[j] * (data[j] - mean) * inv_std + beta->data[j];
        }
    }
    
    DEBUG_PRINT("Layer norm completed");
}

/* ========== GELU 激活函数 ========== */

void gelu(Tensor *x) {
    ASSERT(x != NULL, "Tensor is NULL");
    
    const float sqrt_2_over_pi = 0.7978845608f;
    const float coef = 0.044715f;
    
    DEBUG_PRINT("Computing GELU for %zu elements", x->size);
    
    for (size_t i = 0; i < x->size; i++) {
        float val = x->data[i];
        float cube = val * val * val;
        float inner = sqrt_2_over_pi * (val + coef * cube);
        x->data[i] = 0.5f * val * (1.0f + tanhf(inner));
    }
    
    DEBUG_PRINT("GELU completed");
}

/* ========== 残差连接 ========== */

void residual_add(Tensor *x, const Tensor *residual) {
    ASSERT(x != NULL && residual != NULL, "NULL tensor");
    ASSERT(x->size == residual->size, "Size mismatch");
    
    for (size_t i = 0; i < x->size; i++) {
        x->data[i] += residual->data[i];
    }
}

/* ========== 单头 Attention ========== */

void attention_single_head(const Tensor *Q, 
                           const Tensor *K, 
                           const Tensor *V,
                           const Tensor *mask,
                           Tensor *output) {
    
    ASSERT(Q != NULL && K != NULL && V != NULL && output != NULL, "NULL tensor");
    ASSERT(Q->ndim == 2 && K->ndim == 2 && V->ndim == 2, "Must be 2D");
    
    size_t seq_len = Q->shape[0];
    size_t d_k = Q->shape[1];
    size_t d_v = V->shape[1];
    
    ASSERT(K->shape[0] == seq_len && K->shape[1] == d_k, "K shape mismatch");
    ASSERT(V->shape[0] == seq_len, "V seq_len mismatch");
    ASSERT(output->shape[0] == seq_len && output->shape[1] == d_v, "Output shape mismatch");
    
    DEBUG_PRINT("Single-head attention: seq_len=%zu, d_k=%zu, d_v=%zu", seq_len, d_k, d_v);
    
    double start = _get_time_ms();
    
    // 计算 Scores = Q @ K^T
    size_t scores_shape[] = {seq_len, seq_len};
    Tensor *scores = tensor_create(2, scores_shape);
    
    for (size_t i = 0; i < seq_len; i++) {
        for (size_t j = 0; j < seq_len; j++) {
            float sum = 0.0f;
            for (size_t k = 0; k < d_k; k++) {
                sum += Q->data[i * d_k + k] * K->data[j * d_k + k];
            }
            scores->data[i * seq_len + j] = sum;
        }
    }
    
    // 缩放
    float scale = 1.0f / sqrtf((float)d_k);
    for (size_t i = 0; i < scores->size; i++) {
        scores->data[i] *= scale;
    }
    
    // 应用掩码
    if (mask != NULL) {
        ASSERT(mask->shape[0] == seq_len && mask->shape[1] == seq_len, "Mask shape mismatch");
        for (size_t i = 0; i < scores->size; i++) {
            scores->data[i] += mask->data[i];
        }
    }
    
    // Softmax
    softmax_2d(scores);
    
    // 加权 Value
    memset(output->data, 0, output->size * sizeof(float));
    for (size_t i = 0; i < seq_len; i++) {
        for (size_t j = 0; j < d_v; j++) {
            float sum = 0.0f;
            for (size_t k = 0; k < seq_len; k++) {
                sum += scores->data[i * seq_len + k] * V->data[k * d_v + j];
            }
            output->data[i * d_v + j] = sum;
        }
    }
    
    tensor_free(scores);
    
    DEBUG_PRINT("Single-head attention completed in %.2f ms", _get_time_ms() - start);
}

/* ========== Multi-Head Attention 辅助函数 ========== */

static Tensor** split_heads(const Tensor *X, size_t num_heads) {
    ASSERT(X != NULL && X->ndim == 2, "Invalid input");
    
    size_t seq_len = X->shape[0];
    size_t d_model = X->shape[1];
    size_t d_k = d_model / num_heads;
    
    ASSERT(d_model % num_heads == 0, "d_model must be divisible by num_heads");
    
    Tensor **heads = (Tensor **)malloc(num_heads * sizeof(Tensor *));
    ASSERT(heads != NULL, "Failed to allocate heads");
    
    for (size_t h = 0; h < num_heads; h++) {
        size_t shape[] = {seq_len, d_k};
        heads[h] = tensor_create(2, shape);
        
        for (size_t i = 0; i < seq_len; i++) {
            for (size_t j = 0; j < d_k; j++) {
                size_t src_idx = i * d_model + h * d_k + j;
                size_t dst_idx = i * d_k + j;
                heads[h]->data[dst_idx] = X->data[src_idx];
            }
        }
    }
    
    return heads;
}

static void merge_heads(Tensor **heads, size_t num_heads, Tensor *output) {
    ASSERT(heads != NULL && output != NULL && output->ndim == 2, "Invalid args");
    
    size_t seq_len = heads[0]->shape[0];
    size_t d_v = heads[0]->shape[1];
    size_t d_model = num_heads * d_v;
    
    ASSERT(output->shape[0] == seq_len && output->shape[1] == d_model, "Output shape mismatch");
    
    for (size_t h = 0; h < num_heads; h++) {
        for (size_t i = 0; i < seq_len; i++) {
            for (size_t j = 0; j < d_v; j++) {
                size_t src_idx = i * d_v + j;
                size_t dst_idx = i * d_model + h * d_v + j;
                output->data[dst_idx] = heads[h]->data[src_idx];
            }
        }
    }
}

/* ========== Multi-Head Attention 串行 ========== */

void attention_multi_head_serial(const Tensor *X,
                                 const attention_weights_t *weights,
                                 size_t num_heads,
                                 const Tensor *mask,
                                 Tensor *output) {
    
    ASSERT(X != NULL && weights != NULL && output != NULL, "NULL args");
    ASSERT(X->ndim == 2, "Input must be 2D");
    
    size_t seq_len = X->shape[0];
    size_t d_model = X->shape[1];
    
    INFO_PRINT("Multi-head attention (serial): heads=%zu, seq_len=%zu, d_model=%zu",
               num_heads, seq_len, d_model);
    
    double total_start = _get_time_ms();
    
    // 线性投影
    size_t qkv_shape[] = {seq_len, d_model};
    Tensor *Q_full = tensor_create(2, qkv_shape);
    Tensor *K_full = tensor_create(2, qkv_shape);
    Tensor *V_full = tensor_create(2, qkv_shape);
    
    matmul_serial_ikj(X, weights->W_Q, Q_full);
    matmul_serial_ikj(X, weights->W_K, K_full);
    matmul_serial_ikj(X, weights->W_V, V_full);
    
    // 添加偏置
    for (size_t i = 0; i < seq_len; i++) {
        for (size_t j = 0; j < d_model; j++) {
            Q_full->data[i * d_model + j] += weights->b_Q->data[j];
            K_full->data[i * d_model + j] += weights->b_K->data[j];
            V_full->data[i * d_model + j] += weights->b_V->data[j];
        }
    }
    
    // 分割头
    Tensor **Q_heads = split_heads(Q_full, num_heads);
    Tensor **K_heads = split_heads(K_full, num_heads);
    Tensor **V_heads = split_heads(V_full, num_heads);
    
    tensor_free(Q_full);
    tensor_free(K_full);
    tensor_free(V_full);
    
    // 逐头计算
    size_t d_k = d_model / num_heads;
    size_t head_shape[] = {seq_len, d_k};
    Tensor **head_outputs = (Tensor **)malloc(num_heads * sizeof(Tensor *));
    
    for (size_t h = 0; h < num_heads; h++) {
        head_outputs[h] = tensor_create(2, head_shape);
        attention_single_head(Q_heads[h], K_heads[h], V_heads[h], 
                             mask, head_outputs[h]);
        tensor_free(Q_heads[h]);
        tensor_free(K_heads[h]);
        tensor_free(V_heads[h]);
    }
    
    free(Q_heads);
    free(K_heads);
    free(V_heads);
    
    // 合并头
    Tensor *concat = tensor_create(2, qkv_shape);
    merge_heads(head_outputs, num_heads, concat);
    
    for (size_t h = 0; h < num_heads; h++) {
        tensor_free(head_outputs[h]);
    }
    free(head_outputs);
    
    // 最终线性变换
    matmul_serial_ikj(concat, weights->W_O, output);
    for (size_t i = 0; i < seq_len * d_model; i++) {
        output->data[i] += weights->b_O->data[i % d_model];
    }
    
    tensor_free(concat);
    
    INFO_PRINT("Multi-head attention (serial) completed in %.2f ms", _get_time_ms() - total_start);
}

/* ========== Multi-Head Attention 并行 ========== */

typedef struct {
    const Tensor *Q_head;
    const Tensor *K_head;
    const Tensor *V_head;
    const Tensor *mask;
    Tensor *output_head;
    int head_id;
} attention_head_task_t;

static void attention_head_task(void *arg) {
    attention_head_task_t *task = (attention_head_task_t *)arg;
    
    DEBUG_PRINT("Computing attention for head %d", task->head_id);
    
    attention_single_head(task->Q_head, task->K_head, task->V_head,
                         task->mask, task->output_head);
    
    free(task);
}

void attention_multi_head_parallel(const Tensor *X,
                                   const attention_weights_t *weights,
                                   size_t num_heads,
                                   const Tensor *mask,
                                   Tensor *output) {
    
    ASSERT(X != NULL && weights != NULL && output != NULL, "NULL args");
    
    size_t seq_len = X->shape[0];
    size_t d_model = X->shape[1];
    
    INFO_PRINT("Multi-head attention (parallel): heads=%zu, seq_len=%zu", num_heads, seq_len);
    
    double total_start = _get_time_ms();
    
    // 获取全局线程池
    thread_pool_t *pool = matrix_get_thread_pool();
    ASSERT(pool != NULL, "Matrix thread pool not initialized. Call matrix_init() first.");
    
    // 线性投影（使用并行矩阵乘法）
    size_t qkv_shape[] = {seq_len, d_model};
    Tensor *Q_full = tensor_create(2, qkv_shape);
    Tensor *K_full = tensor_create(2, qkv_shape);
    Tensor *V_full = tensor_create(2, qkv_shape);
    
    matmul_parallel_blocked(X, weights->W_Q, Q_full);
    matmul_parallel_blocked(X, weights->W_K, K_full);
    matmul_parallel_blocked(X, weights->W_V, V_full);
    
    // 添加偏置
    for (size_t i = 0; i < seq_len * d_model; i++) {
        Q_full->data[i] += weights->b_Q->data[i % d_model];
        K_full->data[i] += weights->b_K->data[i % d_model];
        V_full->data[i] += weights->b_V->data[i % d_model];
    }
    
    // 分割头
    Tensor **Q_heads = split_heads(Q_full, num_heads);
    Tensor **K_heads = split_heads(K_full, num_heads);
    Tensor **V_heads = split_heads(V_full, num_heads);
    
    tensor_free(Q_full);
    tensor_free(K_full);
    tensor_free(V_full);
    
    size_t d_k = d_model / num_heads;
    size_t head_shape[] = {seq_len, d_k};
    Tensor **head_outputs = (Tensor **)malloc(num_heads * sizeof(Tensor *));
    
    // 提交所有 attention head 任务到全局线程池
    for (size_t h = 0; h < num_heads; h++) {
        head_outputs[h] = tensor_create(2, head_shape);
        
        attention_head_task_t *task = (attention_head_task_t *)malloc(sizeof(attention_head_task_t));
        task->Q_head = Q_heads[h];
        task->K_head = K_heads[h];
        task->V_head = V_heads[h];
        task->mask = mask;
        task->output_head = head_outputs[h];
        task->head_id = h;
        
        thread_pool_submit(pool, attention_head_task, task, NULL);
    }
    
    // 等待所有 attention head 计算完成
    thread_pool_wait_all(pool);
    
    // 安全地清理头数据
    for (size_t h = 0; h < num_heads; h++) {
        tensor_free(Q_heads[h]);
        tensor_free(K_heads[h]);
        tensor_free(V_heads[h]);
    }
    free(Q_heads);
    free(K_heads);
    free(V_heads);
    
    // 合并头
    Tensor *concat = tensor_create(2, qkv_shape);
    merge_heads(head_outputs, num_heads, concat);
    
    for (size_t h = 0; h < num_heads; h++) {
        tensor_free(head_outputs[h]);
    }
    free(head_outputs);
    
    // 最终线性变换
    matmul_parallel_blocked(concat, weights->W_O, output);
    for (size_t i = 0; i < seq_len * d_model; i++) {
        output->data[i] += weights->b_O->data[i % d_model];
    }
    
    tensor_free(concat);
    
    INFO_PRINT("Multi-head attention (parallel) completed in %.2f ms", _get_time_ms() - total_start);
}

/* ========== 因果掩码 ========== */

Tensor* create_causal_mask(size_t seq_len) {
    size_t shape[] = {seq_len, seq_len};
    Tensor *mask = tensor_create(2, shape);
    
    for (size_t i = 0; i < seq_len; i++) {
        for (size_t j = 0; j < seq_len; j++) {
            mask->data[i * seq_len + j] = (j > i) ? -INFINITY : 0.0f;
        }
    }
    
    return mask;
}