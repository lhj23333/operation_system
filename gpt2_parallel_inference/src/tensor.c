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
            WARN_PRINT("Shape dimension %zu is 0", i);
            return 0;
        }
        
        if (total > SIZE_MAX / shape[i]) {
            ERROR_PRINT("Size overflow detected");
            return 0;
        }
    
        total *= shape[i];
    }
    return total;
}

/* ============= 对外接口函数 ============= */

Tensor* tensor_create(size_t nidm, const size_t *shape) {
    DEBUG_PRINT("Creating tensor with ndim: %zu", nidm);
    
    // 参数检测
    if (nidm == 0 || shape == NULL) {
        ERROR_PRINT("Invalid arguments: ndim: %zu, shape: %p", nidm, (void*)shape);
        return NULL;
    }

    // 分配 Tensor 结构体 
    Tensor *t = (Tensor *)malloc(sizeof(Tensor));
    if (t == NULL) {
        ERROR_PRINT("Failed to allocate memory for Tensor");
        return NULL;
    }

    t->ndim = nidm;
    t->data = NULL;

    // 分配并拷贝 shape 数组
    t->shape = (size_t *)malloc(nidm * sizeof(size_t));
    if (t->shape == NULL) {
        ERROR_PRINT("Failed to allocate memory for shape array");
        free(t);
        return NULL;
    }

    DEBUG_PRINT("Shape: [");
    for (size_t i = 0; i < nidm; i ++) {
        t->shape[i] = shape[i];
        DEBUG_PRINT("%zu%s", shape[i], (i < nidm - 1) ? "," : "");
    }
    DEBUG_PRINT("]");

    // 计算总元素数量
    t->size = _tensor_compute_size(nidm, shape);
    if (t->size == 0) {
        ERROR_PRINT("Invalid tensor size computed");
        free(t->shape);
        free(t);
        return NULL;
    }

    DEBUG_PRINT("Total elements: %zu (%.2f MB)",
        t->size, (t->size * sizeof(float)) / (1024.0 * 1024.0));

    // 分配数据内存
    t->data = (float *)calloc(t->size, sizeof(float));
    if (t->data == NULL) {
        ERROR_PRINT("Failed to allocate memory for tensor data array (%zu bytes)",
            t->size * sizeof(float));
        free(t->shape);
        free(t);
        return NULL;
    }

    DEBUG_PRINT("Tensor created successfully at %p", (void*)t);
    return t;
}

Tensor *tensor_create_with_value(size_t nidm, const size_t *shape, float value) {
    DEBUG_PRINT("Creating tensor with value: %.2f", value);

    Tensor *t = tensor_create(nidm, shape);
    if (t == NULL) return NULL;

    for (size_t i = 0; i < t->size; i ++) {
        t->data[i] = value;
    }

    return t;
}

Tensor *tensor_from_data(size_t nidm, const size_t *shape, const float *data) {
    DEBUG_PRINT("Creating tensor from data %p", (void*)data);

    Tensor *t = tensor_create(nidm, shape);
    if(t == NULL) return NULL;

    memcpy(t->data, data, t->size * sizeof(float));
    if (t->data == NULL) {
        ERROR_PRINT("Failed to copy data into tensor");
        free(t->shape);
        free(t);
        return NULL;
    }

    return 0;
}

void tensor_free(Tensor *t) {
    if (t == NULL) {
        WARN_PRINT("Attempted to free a NULL tensor");
        return;
    }

    DEBUG_PRINT("Freeing tensor (size:%zu, memory: %.2f MB) at %p",
        t->size, (t->size * sizeof(float)) / (1024.0 * 1024.0), (void*)t);
    
    if (t->data != NULL) {
        free(t->data);
        t->data = NULL;
    }

    if (t->shape != NULL) {
        free(t->shape);
        t->shape = NULL;
    }

    free(t);
    DEBUG_PRINT("Tensor freed successfully");
}


/* ========== 索引操作 ========== */

size_t tensor_offset(const Tensor *t, const size_t *indices) {
    if (t == NULL || indices == NULL) {
        ERROR_PRINT("Invalid arguments to tensor_offset, tensor or indices is NULL");
        return 0;
    }

    size_t offset = 0;
    size_t stride = 1;

    /**
     * 演算示例：(shape=[2,3,4], indices=[1,2,3]);
     * 
     * i=2: offset = 0 + 3 * 1 = 3;     stride = 1 * 4 = 4;
     * i=1: offset = 3 + 2 * 4 = 11;    stride = 4 * 3 = 12;
     * i=0: offset = 11 + 1 * 12 = 23;  stride = 12 * 2 = 24;
     */
    for (int i = (int)t->ndim - 1; i >= 0; i --) {
        #ifdef DEBUG
            if (indices[i] >= t->shape[i]) {
                ERROR_PRINT("Index out of bounds: index[%d]:%zu >= shape[%d]:%zu",
                    i, indices[i], i, t->shape[i]);
                return 0;
            }
        #endif

        offset += indices[i] * stride;
        stride *= t->shape[i];
    }
    return offset;
}

float tensor_get(const Tensor *t, const size_t *indices) {
    if (t == NULL || indices == NULL) {
        ERROR_PRINT("Invalid arguments to tensor_get, tensor or indices is NULL");
        return 0.0f;
    }

    size_t offset = tensor_offset(t, indices);
    return t->data[offset];
}

void tensor_set(Tensor *t, const size_t *indices, float value) {
    if (t == NULL || indices == NULL) {
        ERROR_PRINT("Invalid arguments to tensor_set, tensor or indices is NULL");
        return;
    }

    size_t offset = tensor_offset(t, indices);
    t->data[offset] = value;
}


/* ========== 形状操作 ========== */

bool tensor_shape_equal(const Tensor *a, const Tensor *b) {
    if (a == NULL || b == NULL) {
        ERROR_PRINT("Invalid arguments to tensor_shape_equal, a or b is NULL");
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
        ERROR_PRINT("Invalid argument to tensor_clone, tensor is NULL");
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
        ERROR_PRINT("Invalid arguments to tensor_reshape, tensor or new_shape is NULL");
        return NULL;
    }

    size_t new_size = _tensor_compute_size(new_ndim, new_shape);
    if (new_size != t->size) {
        ERROR_PRINT("Reshape size mismatch: old size %zu, new size %zu", t->size, new_size);
        return NULL;
    }

    // 创建 new tensor(共享数据)
    Tensor *new_t = (Tensor *)malloc(sizeof(Tensor));
    if (new_t == NULL) {
        ERROR_PRINT("Failed to allocate memory for reshaped tensor");
        return NULL;
    }

    new_t->ndim = new_ndim;
    new_t->size = new_size;
    new_t->data = t->data; // 共享数据指针

    // 分配并拷贝新 shape 数组
    new_t->shape = (size_t *)malloc(new_ndim * sizeof(size_t));
    if (new_t->shape == NULL) {
        ERROR_PRINT("Failed to allocate memory for reshaped tensor shape");
        free(new_t);
        return NULL;
    }
    memcpy(new_t->shape, new_shape, new_ndim * sizeof(size_t));

    return new_t;
}   


/* ========== 调试辅助函数 ========== */

void tensor_print_info(const Tensor *t) {
    if (t == NULL) {
        WARN_PRINT("Tensor: NULL\n");
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
    printf("]%-*s║\n", (int)(28 - (t->ndim * 4)), "");

    printf("║ Total Elements: %zu%-15s║\n", t->size, "");
    printf("║ Memory: %.2f MB%-19s║\n", 
           (t->size * sizeof(float)) / (1024.0 * 1024.0), "");
    printf("║ Data pointer: %p%-10s║\n", (void*)t->data, "");
    printf("╚════════════════════════════════════╝\n");
}

void tensor_print_data(const Tensor *t) {
    if (t == NULL) {
        WARN_PRINT("Tensor: NULL\n");
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

        for (int i = 0; i < rows; i ++) {
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
        for (int i = 0; i < t->size; i ++) {
            printf(" %7.4f", t->data[i]);
            if ((i + 1) % 10 == 0 && (i + 1) < t->size) {
                printf("\n");
            }
        }
        printf(" ]\n");
    }
}

void tensor_fill_random(Tensor *t, float min, float max) {
    if (t == NULL) {
        ERROR_PRINT("Invalid argument to tensor_fill_random, tensor is NULL");
        return;
    }

    static bool seeded = false;
    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }

    float range = max - min;
    for (size_t i = 0; i < t->size; i ++) {
        t->data[i] = min + range * ((float)rand() / RAND_MAX);
    }
}

tensor_stats tensor_compute_stats(const Tensor *t) {
    tensor_stats stats = {0};
    if (t == NULL) {
        ERROR_PRINT("Invalid argument to tensor_compute_stats, tensor is NULL");
        return stats;
    }

    stats.min = t->data[0];
    stats.max = t->data[0];
    stats.mean = 0.0f;

    // 计算均值、最小值、最大值
    for (size_t i = 0; i < t->size; i ++) {
        float val = t->data[i];
        if (val < stats.min) stats.min = val;
        if (val > stats.max) stats.max = val;
        stats.mean += val;
    }
    stats.mean /= t->size;

    // 计算方差
    stats.variance = 0.0f;
    for (size_t i = 0; i < t->size; i ++) {
        float diff = t->data[i] - stats.mean;
        stats.variance += diff * diff;
    }
    stats.variance /= t->size;

    return stats;
}