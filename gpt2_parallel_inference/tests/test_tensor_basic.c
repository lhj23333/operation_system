#include "tensor.h"
#include "common.h"
#include <sys/time.h>

/**
 * 测试 1：创建和销毁 Tensor
 */
void test_tensor_create_destroy() {
    INFO_PRINT("=== Test: Create and Destroy ===");

    size_t shape[] = {2, 3, 4};
    Tensor *t = tensor_create(3, shape);

    // 验证元数据
    ASSERT(t != NULL, "Tensor creation failed");
    ASSERT(t->ndim == 3, "Tensor ndim incorrect");
    ASSERT(t.size == 24, "Tensor size incorrect");
    ASSERT(t->data != NULL, "Tensor data allocation failed");
    ASSERT(t->shape != NULL, "Tensor shape allocation failed");

    // 验证 shape 拷贝
    for (size_t i = 0; i < 3; i++) {
        ASSERT(t->shape[i] == shape[i], "Shape mismatch");
    }

    // 验证初始化为 0
    for (size_t i = 0; i < t->size; i ++) {
        ASSERT(t->data[i] == 0.0f, "Tensor data not initialized to zero");
    }

    // 打印信息
    tensor_print_info(t);
    
    tensor_free(t);
    INFO_PRINT("=== Test Passed ===\n");
}


/**
 * 测试 2：索引计算
 */
void test_indexing() {
    INFO_PRINT("=== Test: Indexing ===");

    size_t shape[] = {2, 3, 4};
    Tensor *t = tensor_create(3, shape);

    // 手动设置数据
    for (size_t i = 0; i < t->size; i ++) {
        t->data[i] == (float)i;
    }

    // 测试索引计算
    struct {
        size_t indices[3];
        size_t expected_offset;
        float expected_value;
    } tests[] = {
        {{0, 0, 0}, 0, 0.0f},
        {{0, 1, 2}, 6, 6.0f},
        {{1, 0, 0}, 12, 12.0f},
        {{1, 2, 3}, 23, 23.0f},
    };

    for (size_t i = 0; i < ARRAY_SIZE(tests); i ++) {
        size_t offset = tensor_offset(t, tests[i].indices);
        float value = tensor_get(t, tests[i].indices);

        DEBUG_PRINT("Indices:[%zu, %zu, %zu] => Offset: %zu, Value: %.1f",
            tests[i].indices[0], tests[i].indices[1], tests[i].indices[2],
            offset, value);

        ASSERT(offset == tests[i].expected_offset, "Offset mismatch");
        ASSERT(value == tests[i].expected_value, "Value mismatch");
    }
    tensor_free(t);

    INFO_PRINT("=== Test Passed ===\n");
}


/**
 * 测试 3：内存布局验证
 */
void test_memory_layout() {
    INFO_PRINT("=== Test: Memory Layout ===");

    size_t shape[] = {2, 3};
    Tensor *t = tensor_create(2, shape);

    // 设置数据
    float expected_value[] = {1, 2, 3, 4, 5, 6};
    for (size_t i = 0; i < 6; i ++) {
        t->data[i] = expected_value[i];
    }

    // 验证内存布局
    size_t idx[][2] = {
        {0, 0}, {0, 1}, {0, 2},
        {1, 0}, {1, 1}, {1, 2}
    };

    for (size_t i = 0; i < 6; i ++) {
        float value = tensor_get(t, idx[i]);
        DEBUG_PRINT("A[%zu][%zu]: %.1f (expected: %.1f)",
            idx[i][0], idx[i][1], value, expected_value[i]);
        ASSERT(value == expected_value[i], "Memory layout mismatch");
    }
    tensor_print_data(t);

    tensor_free(t);
    INFO_PRINT("=== Test Passed ===\n");
}

/**
 * 测试 4：边界情况
 */
void test_edge_cases() {
    INFO_PRINT("=== Test: Edge Cases ===");
    
    // 测试 1：单元素张量
    size_t shape1[] = {1};
    Tensor *t1 = tensor_create(1, shape1);
    ASSERT(t1 != NULL, "Failed to create 1-element tensor");
    ASSERT(t1->size == 1, "Wrong size for 1-element tensor");
    tensor_free(t1);
    
    // 测试 2：大维度张量
    size_t shape2[] = {2, 2, 2, 2, 2};  // 5D
    Tensor *t2 = tensor_create(5, shape2);
    ASSERT(t2 != NULL, "Failed to create 5D tensor");
    ASSERT(t2->size == 32, "Wrong size for 5D tensor");
    tensor_free(t2);
    
    // 测试 3：NULL 安全性
    tensor_free(NULL);  // 应该不崩溃
    
    INFO_PRINT("✓ PASSED\n");
}


/**
 * 测试 5：性能测试
 */
void test_performance() {
    INFO_PRINT("=== Test: Performance ===");

    // 创建较大的张量
    size_t shape[] = {100, 100, 10}; // 100,000 elements
    Tensor *t = tensor_create(3, shape);
    ASSERT(t != NULL, "Failed to create large tensor");

    struct timeval start, end;

    // test 1: 顺序写入
    gettimeofday(&start, NULL);
    for (size_t i = 0; i < t->size; i ++) {
        t->data[i] = (float)i;
    }
    gettimeofday(&end, NULL);
    double write_time = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_usec - start.tv_usec) / 1000.0;

    // test 2: 顺序读取
    gettimeofday(&start, NULL);
    double sum = 0.0;
    for (size_t i = 0; i < t->size; i ++) {
        sum += t->data[i];
    }
    gettimeofday(&end, NULL);
    double read_time = (end.tv_sec - start.tv_sec) * 1000.0 +
                       (end.tv_usec - start.tv_usec) / 1000.0;

    INFO_PRINT("Sequenttial writeL %.2f ms (%.2f GB/s)",
        write_time, (t->size * sizeof(float)) / (write_time / 1000.0) / 1e9);
    INFO_PRINT("Sequential read: %.2f ms (%.2f GB/s)",
        read_time, (t->size * sizeof(float)) / (read_time / 1000.0) / 1e9);

    tensor_free(t);
    INFO_PRINT("=== Test Passed ===\n");
}

int main() {
    INFO_PRINT("╔════════════════════════════════════════╗");
    INFO_PRINT("║   Tensor Basic Tests                  ║");
    INFO_PRINT("╚════════════════════════════════════════╝\n");
    
    test_create_and_destroy();
    test_indexing();
    test_memory_layout();
    test_edge_cases();
    test_performance();
    
    INFO_PRINT("\n╔════════════════════════════════════════╗");
    INFO_PRINT("║   All Tests PASSED! 🎉                ║");
    INFO_PRINT("╚════════════════════════════════════════╝");
    
    return 0;
}