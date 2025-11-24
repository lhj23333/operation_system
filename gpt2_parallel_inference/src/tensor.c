#include "tensor.h"
#include <math.h>
#include <time.h>

/* ============= 内部辅助函数 ============= */

/**
 * @name _tensor_compute_size
 * @brief 计算总元素数量
 * 
 * @param ndim 维度数量
 * @param shape 维度数组
 * 
 * @return size_t 总元素数量
 */
static size_t _tensor_compute_size(size_t ndim, const size_t *shape) {
    if (ndim == 0 || shape == NULL) return 0;

    size_t total = 1;
    for (size_t i = 0; i < ndim; i ++) {
        if (shape[i] == 0) {
            WARN_PRINT("_tensor_compute_size: Shape dimension %zu is 0", i);
            return 0;
        }
        
        if (total > SIZE_MAX / shape[i]) {
            ERROR_PRINT("_tensor_compute_size: Size overflow at dimension %zu", i);
            return 0;
        }
    
        total *= shape[i];
    }
    return total;
}


/**
 * @name _tensor_compute_default_strides
 * @brief 计算默认行优先存储步长
 * 
 * @param t             - 张量指针  
 */
static void _tensor_compute_default_strides(Tensor *t) {
    if (t == NULL) {
        ERROR_PRINT("_tensor_compute_default_strides: Invalid argument");
        return ;
    }

    // 仅在未分配时分配内存
    if (t->stride == NULL) {
        t->stride = (size_t *)malloc(t->ndim * sizeof(size_t));
        if (t->stride == NULL) {
            ERROR_PRINT("_tensor_compute_default_strides: Failed to allocate memory for tensor strides");
            return ;
        }
    }

    size_t stride = 1;
    for (int i = (int)t->ndim - 1; i >= 0; i --) {
        t->stride[i] = stride;
        stride *= t->shape[i];
    }

    DEBUG_PRINT("_tensor_compute_default_strides: Computed strides: [");
    for (size_t i = 0; i < t->ndim; i ++) {
        DEBUG_PRINT("%zu%s", t->stride[i], (i < t->ndim - 1) ? "," : "");
    }
    DEBUG_PRINT("]");

    DEBUG_PRINT("_tensor_compute_default_strides: Strides computed successfully");
}


/**
 * @name _tensor_validate_slice_range
 * @brief 验证切片范围的有效性
 * 
 * @param t - 张量指针
 * @param start - 起始索引
 * @param end - 结束索引
 * 
 * @return 有效返回 true，无效返回 false
 */
static bool _tensor_validate_slice_range(const Tensor *t, const size_t *start, const size_t *end) {
    if (t == NULL || start == NULL || end == NULL) {
        ERROR_PRINT("_tensor_validate_slice_range: Invalid arguments");
        return false;
    }

    if (t->ndim == 0) {
        ERROR_PRINT("_tensor_validate_slice_range: Cannot slice 0-D tensor");
        return false;
    }

    for (size_t i = 0; i < t->ndim; i ++) {
        if (start[i] >= end[i]) {
            ERROR_PRINT("_tensor_validate_slice_range: Invalid range - start[%zu]:%zu >= end[%zu]:%zu",
                i, start[i], i, end[i]);
            return false;
        }

        if(end[i] > t->shape[i]) {
            ERROR_PRINT("_tensor_validate_slice_range: Range out of bounds - end[%zu]:%zu > shape[%zu]:%zu",
                i, end[i], i, t->shape[i]);
            return false;
        }
    }
    
    return true;
}

/**
 * @name _tensor_slice_view
 * @brief 创建张量视图（共享数据）
 * 
 * @param t             - 待切片张量指针
 * @param start         - 各维度起始索引
 * @param end           - 各维度结束索引（不包含）
 * 
 * @return 
 *      successful - 返回新张量指针（共享数据）
 *      failed - 返回 NULL
 */
static Tensor* _tensor_slice_view(const Tensor *t, const size_t *start, const size_t *end) {
    // 参数验证
    if (!_tensor_validate_slice_range(t, start, end)) {
        ERROR_PRINT("_tensor_slice_view: Invalid slice range");
        return NULL;
    }

    // 创建视图向量
    Tensor *view_t = (Tensor *)malloc(sizeof(Tensor));
    if (view_t == NULL) {
        ERROR_PRINT("_tensor_slice_view: Failed to allocate memory for tensor slice view");
        return NULL;
    }

    // 初始化元数据
    view_t->ndim = t->ndim;
    view_t->data = t->data;     // 共享数据指针
    view_t->owns_data = false;  // 不拥有数据所有权

    // 分配并设置新 shape
    view_t->shape = (size_t *)malloc(t->ndim * sizeof(size_t));
    if (view_t->shape == NULL) {
        ERROR_PRINT("_tensor_slice_view: Failed to allocate memory for slice view shape");
        free(view_t);
        return NULL;
    }
    size_t new_size = 1;
    for (size_t i = 0; i < t->ndim; i ++) {
        size_t dim_size = end[i] - start[i];
        view_t->shape[i] = dim_size;
        new_size *= dim_size;

        DEBUG_PRINT("_tensor_slice_view: View shape[%zu] = %zu (from [%zu, %zu))",
            i, dim_size, start[i], end[i]);
    }
    view_t->size = new_size;

    // 继承原stride 
    view_t->stride = (size_t *)malloc(t->ndim * sizeof(size_t));
    if (view_t->stride == NULL) {
        ERROR_PRINT("_tensor_slice_view: Failed to allocate memory for tensor slice view");
        free(view_t->shape);
        free(view_t);
        return NULL;
    }
    memcpy(view_t->stride, t->stride, t->ndim * sizeof(size_t));

    DEBUG_PRINT("_tensor_slice_view: Created tensor slice view successfully at %p", (void*)view_t);

    // 计算偏移量
    view_t->offset = t->offset;
    for (size_t i = 0; i < t->ndim; i ++) {
        view_t->offset += start[i] *t->stride[i];
    }
    DEBUG_PRINT("_tensor_slice_view: View offset: %zu (original offset:%zu)",
        view_t->offset, t->offset);

    INFO_PRINT("_tensor_slice_view: Tensor slice view created successfully, size: %zu", view_t->size);


    return view_t;
}


/**
 * @name tensor_slice_copy
 * @brief 创建张量副本（数据复制）
 * 
 * @param t             - 待切片张量指针
 * @param start         - 各维度起始索引
 * @param end           - 各维度结束索引（不包含）
 * 
 * @return 
 *      successful - 返回新张量指针（数据复制）
 *      failed - 返回 NULL
 */
static Tensor* _tensor_slice_copy(const Tensor *t, const size_t *start, const size_t *end) {
    // 参数验证
    if (!_tensor_validate_slice_range(t, start, end)) {
        ERROR_PRINT("_tensor_slice_copy: Invalid slice range");
        return NULL;
    }

    // 创建视图 (中间表示)
    Tensor *view_t = _tensor_slice_view(t, start, end);
    if (view_t == NULL) {
        ERROR_PRINT("_tensor_slice_copy: Failed to create slice view for copy");
        return NULL;
    }

    // 创建新张量 (数据复制)
    Tensor *copy_t = (Tensor *)malloc(sizeof(Tensor));
    if (copy_t == NULL) {
        ERROR_PRINT("_tensor_slice_copy: Failed to allocate memory for tensor slice copy");
        tensor_free(view_t);
        return NULL;
    }

    copy_t->ndim = view_t->ndim;
    copy_t->size = view_t->size;
    copy_t->offset = 0;
    copy_t->owns_data = true;

    // 分配并复制 shape
    copy_t->shape = (size_t *)malloc(view_t->ndim * sizeof(size_t));
    if (copy_t->shape == NULL) {
        ERROR_PRINT("_tensor_slice_copy: Failed to allocate memory for slice copy shape");
        tensor_free(view_t);
        free(copy_t);
        return NULL;
    }
    memcpy(copy_t->shape, view_t->shape, view_t->ndim * sizeof(size_t));

    // 初始化并计算 stride
    copy_t->stride = (size_t *)malloc(view_t->ndim * sizeof(size_t));
    _tensor_compute_default_strides(copy_t);
    if (copy_t->stride == NULL) {
        ERROR_PRINT("_tensor_slice_copy: Failed to compute stride for slice copy");
        tensor_free(view_t);
        free(copy_t->shape);
        free(copy_t);
        return NULL;
    } 

    // 分配新数据
    copy_t->data = (float *)malloc(copy_t->size * sizeof(float));
    if (copy_t->data == NULL) {
        ERROR_PRINT("_tensor_slice_copy: Failed to allocate memory for slice copy data");
        tensor_free(view_t);
        free(copy_t->shape);
        free(copy_t->stride);
        free(copy_t);
        return NULL;
    }
    
    // 拷贝数据
    INFO_PRINT("_tensor_slice_copy: Copying %zu elements....", copy_t->size);

    // 生成所有多维索引并逐个拷贝
    size_t *indices = (size_t *)malloc(copy_t->ndim * sizeof(size_t));
    if (indices == NULL) {
        ERROR_PRINT("_tensor_slice_copy: Failed to allocate memory for indices array");
        tensor_free(view_t);
        tensor_free(copy_t);
        return NULL;
    }
    memset(indices, 0, copy_t->ndim * sizeof(size_t));

    for (size_t elem = 0; elem < copy_t->size; elem ++) {
        // 获取视图中值并设置到新拷贝中
        float val = tensor_get(view_t, indices);
        tensor_set(copy_t, indices, val);

        // 递增多维索引
        for (int dim = (int)copy_t->ndim - 1; dim >= 0; dim --) {
            indices[dim] ++;
            if (indices[dim] < copy_t->shape[dim]) {
                break;
            }
            indices[dim] = 0; // 进位
        }
    }

    free(indices);
    tensor_free(view_t);

    INFO_PRINT("_tensor_slice_copy: Tensor slice copy created successfully at %p", (void*)copy_t);
    return copy_t;
}

/* ============= 对外接口函数 ============= */

Tensor* tensor_create(size_t ndim, const size_t *shape) {
    DEBUG_PRINT("tensor_create: Creating tensor with ndim: %zu", ndim);
    
    // 参数检测
    if (ndim == 0 || shape == NULL) {
        ERROR_PRINT("tensor_create: Invalid arguments");
        return NULL;
    }

    // 分配 Tensor 结构体 
    Tensor *t = (Tensor *)malloc(sizeof(Tensor));
    if (t == NULL) {
        ERROR_PRINT("tensor_create: Failed to allocate memory for Tensor");
        return NULL;
    }

    t->ndim = ndim;
    t->data = NULL;
    t->stride = NULL;
    t->offset = 0;
    t->owns_data = true;

    // 分配并拷贝 shape 数组
    t->shape = (size_t *)malloc(ndim * sizeof(size_t));
    if (t->shape == NULL) {
        ERROR_PRINT("tensor_create: Failed to allocate memory for shape array");
        free(t);
        return NULL;
    }

    #ifdef DEBUG
        DEBUG_PRINT("Shape: [");
        for (size_t i = 0; i < ndim; i ++) {
            t->shape[i] = shape[i];
            DEBUG_PRINT("%zu%s", shape[i], (i < ndim - 1) ? "," : "");
        }
        DEBUG_PRINT("]\n");
    #else
        for(size_t i = 0; i < ndim; i ++) {
            t->shape[i] = shape[i];
        }
    #endif

    // 计算总元素数量
    t->size = _tensor_compute_size(ndim, shape);
    if (t->size == 0) {
        ERROR_PRINT("tensor_create: Invalid tensor size computed");
        free(t->shape);
        free(t);
        return NULL;
    }

    DEBUG_PRINT("tensor_create: Total elements: %zu (%.2f MB)",
        t->size, (t->size * sizeof(float)) / (1024.0 * 1024.0));

    // 分配数据内存
    t->data = (float *)calloc(t->size, sizeof(float));
    if (t->data == NULL) {
        ERROR_PRINT("tensor_create: Failed to allocate memory for tensor data array (%zu bytes)",
            t->size * sizeof(float));
        free(t->shape);
        free(t);
        return NULL;
    }

    // 计算默认步长
    _tensor_compute_default_strides(t);
    if (t->stride == NULL) {
        ERROR_PRINT("tensor_create: Failed to compute tensor strides");
        free(t->data);
        free(t->shape);
        free(t);
        return NULL;
    }

    DEBUG_PRINT("tensor_create: Tensor created successfully at %p", (void*)t);
    return t;
}

Tensor *tensor_create_with_value(size_t ndim, const size_t *shape, float value) {
    DEBUG_PRINT("tensor_create_with_value: Creating tensor with value: %.2f", value);

    Tensor *t = tensor_create(ndim, shape);
    if (t == NULL) return NULL;

    for (size_t i = 0; i < t->size; i ++) {
        t->data[i] = value;
    }

    return t;
}

Tensor *tensor_from_data(size_t ndim, const size_t *shape, const float *data) {
    DEBUG_PRINT("tensor_from_data: Creating tensor from data %p", (void*)data);

    if (data == NULL) {
        ERROR_PRINT("tensor_from_data: Input data is NULL");
        return NULL;
    }

    Tensor *t = tensor_create(ndim, shape);
    if(t == NULL) return NULL;

    INFO_PRINT("tensor_from_data: Copying %zu elements (%.2f MB)",
        t->size, (t->size * sizeof(float)) / (1024.0 * 1024.0));
    memcpy(t->data, data, t->size * sizeof(float));

    return t;
}

void tensor_free(Tensor *t) {
    if (t == NULL) {
        ERROR_PRINT("tensor_free: Attempted to free a NULL tensor");
        return;
    }

    DEBUG_PRINT("tensor_free: Freeing tensor (size:%zu, memory: %.2f MB) at %p",
        t->size, (t->size * sizeof(float)) / (1024.0 * 1024.0), (void*)t);
    
    // 仅在拥有数据所有权时才释放数据
    if (t->owns_data && t->data != NULL) {
        free(t->data);
        t->data = NULL;
    }

    // 释放 shape 数据
    if (t->shape != NULL) {
        free(t->shape);
        t->shape = NULL;
    }

    // 释放 stride 数据
    if (t->stride != NULL) {
        free(t->stride);
        t->stride = NULL;
    }

    free(t);
    DEBUG_PRINT("tensor_free: Tensor freed successfully");
}


/* ========== 索引操作 ========== */

/*
 * 三维张量 T[2][3][4] 的内存映射
 * 
 * 维度含义：[深度][行][列]
 * 
 * 逻辑视图：
 *   层0:  | 0  1  2  3 |      层1:  | 12 13 14 15 |
 *         | 4  5  6  7 |            | 16 17 18 19 |
 *         | 8  9 10 11 |            | 20 21 22 23 |
 * 
 * 物理内存（一维）：
 *   [0][1][2][3][4][5]...[22][23]
 *    └─────────┘ └────────┘
 *       层0行0      层0行1
 * 
 * 索引公式（行优先）：
 *   offset = i * (rows * cols) + j * cols + k
 *          = i * 12 + j * 4 + k
 * 
 * 例：(shape=[2,3,4], indices=[1,2,3]);
 * T[1][2][3] → offset = 1*12 + 2*4 + 3 = 23 ✓
 * 
 * i=2: offset = 0 + 3 * 1 = 3;     stride = 1 * 4 = 4;
 * i=1: offset = 3 + 2 * 4 = 11;    stride = 4 * 3 = 12;
 * i=0: offset = 11 + 1 * 12 = 23;  stride = 12 * 2 = 24;
 * 
 */
size_t tensor_offset_with_stride(const Tensor *t, const size_t *indices) {
    if (t == NULL || indices == NULL) {
        ERROR_PRINT("tensor_offset_with_stride: Invalid arguments");
        return 0;
    }

    size_t offset = t->offset;

    for (size_t i = 0; i < t->ndim; i ++) {
        if (indices[i] >= t->shape[i]) {
            ERROR_PRINT("tensor_offset_with_stride: Index out of bounds: index[%zu]:%zu >= shape[%zu]:%zu",
                i, indices[i], i, t->shape[i]);
            return 0;
        }
        offset += indices[i] * t->stride[i];
    }
    return offset;
}

float tensor_get(const Tensor *t, const size_t *indices) {
    if (t == NULL || indices == NULL) {
        ERROR_PRINT("tensor_get: Invalid arguments");
        return 0.0f;
    }

    size_t offset = tensor_offset_with_stride(t, indices);
    return t->data[offset];
}

void tensor_set(Tensor *t, const size_t *indices, float value) {
    if (t == NULL || indices == NULL) {
        ERROR_PRINT("tensor_set: Invalid arguments");
        return;
    }

    size_t offset = tensor_offset_with_stride(t, indices);
    t->data[offset] = value;
}


/* ========== 形状操作 ========== */

bool tensor_shape_equal(const Tensor *a, const Tensor *b) {
    if (a == NULL || b == NULL) {
        ERROR_PRINT("tensor_shape_equal: Invalid arguments");
        return false;
    }
    if (a->ndim != b->ndim) return false;

    for (size_t i = 0; i < a->ndim; i ++) {
        if (a->shape[i] != b->shape[i]) return false;
    }
    return true;
}

Tensor* tensor_clone(const Tensor *t) {
    if (t == NULL) {
        ERROR_PRINT("tensor_clone: Invalid argument");
        return NULL;
    }

    // 创建 new tensor
    Tensor *new_t = tensor_create(t->ndim,t->shape);
    if (new_t == NULL) {
        ERROR_PRINT("Failed to clone tensor");
        return NULL;
    }

    // 深拷贝数据
    memcpy(new_t->data, t->data, t->size * sizeof(float));

    return new_t;
}

Tensor* tensor_reshape(const Tensor *t, size_t new_ndim, const size_t *new_shape) {
    if (t == NULL || new_shape == NULL) {
        ERROR_PRINT("tensor_reshape: Invalid arguments");
        return NULL;
    }

    size_t new_size = _tensor_compute_size(new_ndim, new_shape);
    if (new_size != t->size) {
        ERROR_PRINT("tensor_reshape: Reshape size mismatch: old size %zu, new size %zu", t->size, new_size);
        return NULL;
    }

    // 创建 new tensor(共享数据)
    Tensor *new_t = (Tensor *)malloc(sizeof(Tensor));
    if (new_t == NULL) {
        ERROR_PRINT("tensor_reshape: Failed to allocate memory for reshaped tensor");
        return NULL;
    }

    new_t->ndim = new_ndim;
    new_t->size = new_size;
    new_t->data = t->data;      // 共享数据指针
    new_t->owns_data = false;   // 不拥有数据所有权
    new_t->offset = 0;

    // 分配并拷贝新 shape 数组
    new_t->shape = (size_t *)malloc(new_ndim * sizeof(size_t));
    if (new_t->shape == NULL) {
        ERROR_PRINT("tensor_reshape: Failed to allocate memory for reshaped tensor shape");
        free(new_t);
        return NULL;
    }
    memcpy(new_t->shape, new_shape, new_ndim * sizeof(size_t));

    // 分配并计算新 stride（修复）
    new_t->stride = NULL;
    _tensor_compute_default_strides(new_t);
    if (new_t->stride == NULL) {
        ERROR_PRINT("tensor_reshape: Failed to compute stride for reshaped tensor");
        free(new_t->shape);
        free(new_t);
        return NULL;
    }

    return new_t;
}   

Tensor *tensor_transpose(const Tensor *t) {
    if (t == NULL) {
        ERROR_PRINT("tensor_transpose: Invalid argument");
        return NULL;
    }

    if (t->ndim != 2) {
        ERROR_PRINT("tensor_transpose: Tensor transpose only supports 2D tensors, got ndim: %zu", t->ndim);
        return NULL;
    }

    size_t new_shape[2] = {t->shape[1], t->shape[0]};
    Tensor *transposed_t = tensor_create(2, new_shape);
    if (transposed_t == NULL) {
        ERROR_PRINT("tensor_transpose: Failed to create transposed tensor");
        return NULL;
    }
    // tensor_create 已经初始化了 stride，无需额外操作

    for (size_t i = 0; i < t->shape[0]; i ++) {
        for (size_t j = 0; j < t->shape[1]; j ++) {
            size_t src_offset = i * t->shape[1] + j;
            size_t dst_offser = j * transposed_t->shape[1] + i;
            transposed_t->data[dst_offser] = t->data[src_offset];
        }
    }

    return transposed_t;
}

Tensor *tensor_slice(const Tensor *t, const size_t *start, const size_t *end, bool force_copy) {
    if (t == NULL) {
        ERROR_PRINT("tensor_slice: Invalid argument");
        return NULL;
    }

    if (!force_copy) {
        // 优先使用视图（高效）
        INFO_PRINT("tensor_slice: Using slice view mode");
        return _tensor_slice_view(t, start, end);
    } else {
        INFO_PRINT("tensor_slice: Using slice copy mode");
        return _tensor_slice_copy(t, start, end);
    }

}

/* ========== 调试辅助函数 ========== */

void tensor_print_info(const Tensor *t) {
    if (t == NULL) {
        WARN_PRINT("tensor_print_info: invalid argument");
        return;
    }

    printf("╔════════════════════════════════════╗\n");
    printf("║       Tensor Information          ║\n");
    printf("╠════════════════════════════════════╣\n");
    printf("║ Dimensions: %zu%-20s║\n", t->ndim, "");

    printf("║ Shape: [");
    for (size_t i = 0; i < t->ndim; i ++) {
        printf("%zu%s", t->shape[i], (i < t->ndim - 1) ? "," : "");
    }
    int padding = 28 - (int)(t->ndim * 4);
    if (padding < 0) padding = 0;
    printf("]%-*s║\n", padding, "");

    printf("║ Total Elements: %zu%-15s║\n", t->size, "");
    printf("║ Memory: %.2f MB%-19s║\n", 
           (t->size * sizeof(float)) / (1024.0 * 1024.0), "");
    printf("║ Data pointer: %p%-10s║\n", (void*)t->data, "");
    printf("║ Owns data: %s%-23s║\n", t->owns_data ? "true" : "false", "");
    printf("╚════════════════════════════════════╝\n");
}

void tensor_print_data(const Tensor *t) {
    if (t == NULL) {
        WARN_PRINT("tensor_print_data: invalid argument");
        return;
    }

    // 只打印小张量
    if (t->size > 100) {
        printf("Tensor too large to print data (size: %zu)\n", t->size);
        printf("Showing first 10 elements: ");
        for (size_t i = 0; i < 10; i ++) {
            printf("%.4f", t->data[i]);
        }
        printf(" ...\n");
        return ;
    }

    printf("Tensor Data (size: %zu): [", t->size);

    // 2D 特殊处理
    if (t->ndim == 2) {
        size_t rows = t->shape[0];
        size_t cols = t->shape[1];

        for (size_t i = 0; i < rows; i ++) {
            printf("  [");
            for (size_t j = 0; j < cols; j ++) {
                size_t offset = i * cols + j;
                printf(" %7.4f", t->data[offset]);
            }
            printf(" ]\n");
        }
    } else {
        // 一维打印
        printf("  [");
        for (size_t i = 0; i < t->size; i ++) {
            printf(" %.4f", t->data[i]);
            if ((i + 1) % 10 == 0 && (i + 1) < t->size) {
                printf("\n   ");
            }
        }
        printf(" ]\n");
    }
}

void tensor_fill_random(Tensor *t, float min, float max) {
    if (t == NULL) {
        ERROR_PRINT("tensor_fill_random: Invalid argument");
        return;
    }

    // 使用静态变量确保种子只初始化一次
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)t);
        seeded = true;
    }

    float range = max - min;
    for (size_t i = 0; i < t->size; i++) {
        t->data[i] = min + range * ((float)rand() / RAND_MAX);
    }
}

tensor_stats tensor_compute_stats(const Tensor *t) {
    tensor_stats stats = {0};
    if (t == NULL) {
        ERROR_PRINT("tensor_compute_stats: Invalid argument");
        return stats;
    }

    stats.min = t->data[0];
    stats.max = t->data[0];
    stats.mean = 0.0f;

    // 计算均值、最小值、最大值
    double sum = 0.0;
    double c = 0.0;     // kahan 算法补偿项
    for (size_t i = 0; i < t->size; i ++) {
        float val = t->data[i];
        if (val < stats.min) stats.min = val;
        if (val > stats.max) stats.max = val;

        // Kahan 求和算法
        double y = val - c;
        double temp = sum + y;
        c = (temp - sum) - y;
        sum = temp;
    }
    stats.mean = (float)(sum / t->size);

    // 计算方差
    double variance_sum = 0.0;
    c = 0.0;            // 重置补偿项
    for (size_t i = 0; i < t->size; i ++) {
        double diff = t->data[i] - stats.mean;
        double y = diff * diff - c;
        double temp = variance_sum + y;
        c = (temp - variance_sum) - y;
        variance_sum = temp;
    }
    stats.variance = (float)(variance_sum / t->size);

    return stats;
}