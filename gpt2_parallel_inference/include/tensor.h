#ifndef __TENSOR_H__
#define __TENSOR_H__

#include <common.h>

/**
 * @name 张量结构体 - 存储多维数组数据
 * 
 * @brief 
 * 核心设计理念：
 * 1. 数据与元数据分离
 * 2. 内存连续优先
 * 3. 支持任意维度
 * 
 * 内存布局：
 *   结构体本身（栈或堆）
 *     ├─ data  → 堆上的浮点数组（主要内存）
 *     ├─ shape → 堆上的维度数组
 *     ├─ ndim  → 标量
 *     └─ size  → 标量（缓存优化）
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
 * @param ndim - 维度数量
 * @param shape - 各维度大小数组
 * 
 * @return
 *      successful - 返回创建张量指针
 *      failed - 返回 NULL
 * 
 * 示例：
 *  size_t shape[3] = {2, 3, 4};
 *  Tensor *t = tensor_create(3, shape); // 创建形状为 (2, 3, 4) 的张量
 */
Tensor* tensor_create(size_t ndim, const size_t *shape);


/**
 * @name tensor_create_with_vlaue
 * @brief 张量创建并初始化
 * 
 * @param ndim - 维度数量
 * @param shape - 各维度大小数组
 * @param value - 初始化值
 * 
 * @return
 *      successful - 返回创建张量指针
 *      failed - 返回 NULL
 */
Tensor* tensor_create_with_value(size_t ndim, const size_t *shape, float value);

/**
 * @name tensor_from_data
 * @brief 从已有数据创建张量
 * 
 * @param ndim - 维度数量
 * @param shape - 各维度大小数组
 * @param data - 已有数据指针（浮点数组）
 * 
 * @return
 *      successful - 返回创建张量指针
 *      failed - 返回 NULL
 */
Tensor* tensor_from_data(size_t ndim, const size_t *shape, const float *data);


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
 * @param t - 张量指针
 * @param indices - 多维索引数组
 * 
 * @return
 *     返回一维偏移量（
 * 
 * 算法：行优先（Row-Major）计算
 *    offset = Σ (indices[i] * Π shape[j])，其中 j > i
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


/* ========== 形状操作 ========== */

/**
 * @name tensor_shape_equal
 * @brief 比较两个张量形状是否相同
 * 
 * @param a - 张量指针 a
 * @param b - 张量指针 b
 * 
 * @return
 *      相同返回 true，不同返回 false
 */
bool tensor_shape_equal(const Tensor *a, const Tensor *b);


/**
 * @name tensor_clone
 * @brief 克隆张量（深拷贝）
 * 
 * @param t - 待克隆张量指针
 * 
 * @return
 *     返回新张量指针（深拷贝） 
 */
Tensor* tensor_clone(const Tensor *t);

/**
 * @name tensor_reshape
 * @brief 张量重塑形状
 * 
 * @param t - 待重塑张量指针
 * @param nidm - 新维度数量
 * @param new_shape - 新形状数组
 * 
 * @return
 *      successful - 返回新张量指针（共享数据）
 *      failed - 返回 NULL
 * 
 * 注意：新形状的总元素数量必须与原张量相同
 */
Tensor* tensor_reshape(const Tensor *t, size_t nidm, const size_t *new_shape);


/* ========== 工具函数 ========== */

/**
 * @name tensor_print_info
 * @brief 打印张量信息（维度、形状等）
 * 
 * @param t - 张量指针
 */
void tensor_print_info(const Tensor *t);


/**
 * @name tensor_print_data
 * @brief 打印张量数据（仅限小张量）
 * 
 * @param t - 张量指针
 */
void tensor_print_data(const Tensor *t);


/**
 * @name tensor_fill_random
 * @brief 用随机值填充张量 (用于测试)
 * 
 * @param t - 张量指针
 * @param min - 最小值
 * @param max - 最大值
 */
void tensor_fill_random(Tensor *t, float min, float max);


/**
 * @name TensorStats
 * @brief 张量统计信息结构体
 * 
 * 包含最小值、最大值、均值和方差
 */
typedef struct {
    float min;
    float max;
    float mean;
    float variance;
} tensor_stats;

/**
 * @name tensor_compute_stats
 * @brief 计算张量统计信息
 * 
 * @param t - 张量指针
 * 
 * @return
 *      返回张量统计信息结构体
 */
tensor_stats tensor_compute_stats(const Tensor *t);

#endif /* __TENSOR_H__ */