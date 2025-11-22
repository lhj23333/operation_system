#ifndef __TENSOR_H__
#define __TENSOR_H__

#include <stddef.h>
#include <stdint.h>

/**
 * @name 张量结构体 - 存储多维数组数据
 * 
 * @brief 
 * 核心设计理念：
 * 1. 使用一维数组存储数据（内存连续，缓存友好）
 * 2. 用 shape 和 ndim 描述逻辑维度
 * 3. 支持后续的内存对齐优化
 */
typedef struct {
    float *data;        // 数据指针（堆分配）
size_t *shape;      // 维度数组，如[batch, seq_len, hidden_dm]
    size_t ndim;        // 维度数量
    size_t size;        // 总元素数量
} Tensor;

/**
 * @name tensor_create
 * @brief 张量创建
 * 
 * @param 
 *      ndim - 维度数量
 * @param
 *      shape - 维度数组
 * 
 * @return
 *      successful - 返回创建张量指针
 *      failed - 返回 NULL
 */
Tensor* tensor_create(size_t ndim, const size_t *shape);


/**
 * @name tensor_free
 * @brief 张量销毁
 * 
 * @param
 *      t - 待销毁张量指针
 */
void tensor_free(Tensor *t);


/**
 * @name tensor_offset
 * @brief 多维索引转一维偏移
 * 
 * @param
 *     t - 张量指针
 * @param
 *     indices - 多维索引数组
 * 
 * @return
 *     返回一维偏移（行优先 Row-Major 存储）
 */
size_t tensor_offset(const Tensor *t, const size_t *indices);


/**
 * @name tensor_get
 * @brief 获取张量元素值
 * 
 * @param
 *      t - 张量指针
 * @param
 *      indices - 多维索引数组
 * 
 * @return
 *     返回对应元素值
 */
float tensor_get(const Tensor *t, const size_t *indices);


/**
 * @name tensor_set
 * @brief 设置张量元素值
 * 
 * @param
 *      t - 张量指针
 * @param
 *      indices - 多维索引数组
 * @param
 *      value - 待设置值
 */
void tensor_set(Tensor *t, const size_t *indices, float value);


/**
 * @name tensor_print_info
 * @brief 打印张量信息（维度、形状等）
 * 
 * @param
 *      t - 张量指针
 */
void tensor_print_info(const Tensor *t);


#endif /* __TENSOR_H__ */