#ifndef __GPT2_H__
#define __GPT2_H__

#include "common.h"
#include "matrix_parallel.h"
#include "thread_pool.h"

/**
 * GPT-2 config (small 版本)
 */
typedef struct {
    size_t vocab_size;          // 词汇表大小(50257)
    size_t max_seq_len;         // 最大序列长度(1024)
    size_t d_model;             // 模型维度(768)
    size_t num_heads;           // 注意力头数(12)
    size_t num_layers;          // Transformer 层数(12)
    size_t d_ff;                // FNN 维度(3072 = 4 * d_model)
    float dropout;              // Dropout 率
} gpt2_config_t;

/**
 * Attention 层权重矩阵
 */
typedef struct {
    Tensor *W_Q;    // 查询权重矩阵 [d_model, d_model]
    Tensor *W_K;    // 键权重矩阵 [d_model, d_model]
    Tensor *W_V;    // 值权重矩阵 [d_model, d_model]
    Tensor *W_O;    // 输出权重矩阵 [d_model, d_model]

    Tensor *b_Q;    // 查询偏置向量 [d_model]
    Tensor *b_K;    // 键偏置向量 [d_model]
    Tensor *b_V;    // 值偏置向量 [d_model]
    Tensor *b_O;    // 输出偏置向量 [d_model]
} attention_weights_t;


/**
 * FFN 层权重矩阵
 */
typedef struct {
    Tensor *W1;     // 第一层权重矩阵 [d_model, d_ff]
    Tensor *b1;     // 第一层偏置向量 [d_ff]
    Tensor *W2;     // 第二层权重矩阵 [d_ff, d_model]
    Tensor *b2;     // 第二层偏置向量 [d_model]
} ffn_weights_t;


/**
 * Transformer block 参数
 */
typedef struct {
    attention_weights_t attn;  // 注意力层权重
    ffn_weights_t ffn;         // ffn 权重

    Tensor *ln1_gamma;         // 第一个 LayerNorm 的缩放参数 [d_model]
    Tensor *ln1_beta;          // 第一个 LayerNorm 的偏移参数 [d_model]
    Tensor *ln2_gamma;         // 第二个 LayerNorm 的缩放参数 [d_model]
    Tensor *ln2_beta;          // 第二个 LayerNorm 的偏移参数 [d_model]
} transformer_block_t;


/**
 * GPT-2 模型结构体
 */
typedef struct {
    gpt2_config_t cfg;

    // Embedding 层
    Tensor *token_embedding;        // 词嵌入矩阵 [vocab_size, d_model]
    Tensor *position_embedding;     // 位置嵌入矩阵 [max_seq_len, d_model]

    // transformer blocks
    transformer_block_t *blocks;    // transformer blocks 数组 [num_layers]

    // 最终 LayerNorm 参数
    Tensor *final_ln_gamma;
    Tensor *final_ln_beta;

    // 输出层 (与 token_embedding 共享权重)
    Tensor *lm_head;                // 语言模型头权重矩阵 [d_model, vocab_size]
} gpt2_model_t;


/* ========== 模型 API ========== */

/**
 * @name gpt2_create
 * @brief 创建并初始化 GPT-2 模型
 * 
 * @param cfg GPT-2 配置参数
 * 
 * @return gpt2_model_t* 指向初始化好的 GPT-2 模型结构体的指针
 */
gpt2_model_t* gpt2_create(const gpt2_config_t *cfg);


/**
 * @name gpt2_free
 * @brief 释放 GPT-2 模型
 * 
 * @param model 指向 GPT-2 模型结构体的指针
 * 
 * @return void
 */
void gpt2_free(gpt2_model_t *model);


/**
 * @name gpt2_load_weights
 * @brief 从文件加载 GPT-2 模型权重
 * 
 * @param model 指向 GPT-2 模型结构体的指针
 * @param checkpoint_path 权重文件路径
 * 
 * @return int
 *      0 - 成功
 *     -1 - 失败
 */
int gpt2_load_weights(gpt2_model_t *model, const char *checkpoint_path);


/**
 * @name gpt2_forward
 * @brief 前向传播
 * 
 * @param model 指向 GPT-2 模型结构体的指针
 * @param input_ids 输入 token ID 张量 [batch_size, seq_len]
 * @param output    输出 logits 张量 [batch_size, seq_len, vocab_size]
 * 
 * @return void
 */
void gpt2_forward(gpt2_model_t *model, const Tensor *input_ids, Tensor *output);


/* ========== Attention API ========== */

/**
 * @name attention_single_head
 * @brief 单头注意力机制（串行实现）
 * 
 * @param Q         查询张量 [batch_size, seq_len, d_k]
 * @param K         键张量 [batch_size, seq_len, d_k]
 * @param V         值张量 [batch_size, seq_len, d_k]
 * @param mask      掩码张量 [batch_size, seq_len, seq_len]，可选
 * @param output    输出张量 [batch_size, seq_len, d_k]
 * 
 * @return void
 */
void attention_single_head(const Tensor *Q, 
                           const Tensor *K, 
                           const Tensor *V,
                           const Tensor *mask,
                           Tensor *output);


/**
 * @name attention_multi_head_serial
 * @brief Multi-Head Attention（串行版本）
 * 
 * @param X         输入向量 [seq_len, d_model]
 * @param weights   权重参数
 * @param num_heads 头数
 * @param mask      掩码张量 [seq_len, seq_len]，可选
 * @param output    输出向量 [seq_len, d_model]
 * 
 * @return void
 */
void attention_multi_head_serial(const Tensor *X,
                                 const attention_weights_t *weights,
                                 size_t num_heads,
                                 const Tensor *mask,
                                 Tensor *output);

/**
 * @name attention_multi_head_parallel
 * @brief Multi-Head Attention（并行版本）
 * 
 * @param X         输入向量 [seq_len, d_model]
 * @param weights   权重参数
 * @param num_heads 头数
 * @param mask      掩码张量 [seq_len, seq_len]，可选
 * @param output    输出向量 [seq_len, d_model]
 * 
 * @return void
 */
void attention_multi_head_parallel(const Tensor *X,
                                   const attention_weights_t *weights,
                                   size_t num_heads,
                                   const Tensor *mask,
                                   Tensor *output);

/**
 * @name create_causal_mask
 * @brief 创建因果掩码（用于自回归生成）
 * 
 * @param seq_len 序列长度
 * 
 * @return 掩码张量 [seq_len, seq_len]
 */
Tensor* create_causal_mask(size_t seq_len);

/* ========== 工具函数 ========== */

/**
 * @name softmax
 * @brief 计算输入张量的 Softmax
 * 
 * @param x 输入输出张量
 * 
 * @return void
 */
void softmax_2d(Tensor *x);

/**
 * @name layer_norm
 * @brief Layer Normalization
 * 
 * @param x     输入输出张量
 * @param gamma 缩放参数
 * @param beta  偏移参数
 * @param eps   防止除零的小常数
 * 
 * @return void
 */
void layer_norm(Tensor *x, const Tensor *gamma, const Tensor *beta, float eps);

/**
 * @name gelu
 * @brief  GELU 激活函数
 * 
 * @param x 输入输出张量
 * 
 * @return void
 */
void gelu(Tensor *x);

/**
 * @name residual_add
 * @brief  残差连接
 * 
 * @param x        输入输出张量
 * @param residual 残差张量
 * 
 * @return void
 */
void residual_add(Tensor *x, const Tensor *residual);

#endif /* __GPT2_H__ */