#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

#include <stddef.h>
#include "func_manager.h"

typedef struct {
    int is_valid;   // 是否有效
    double value;   // 求值结果
    char type[32];  // 数据类型（"int"、"double"、"error")
    char error_msg[256];    // 错误信息
} ExprResult;

// 检验是否为纯算术表达式与复杂函数
int is_simple_arithmetic_expression(const char *expr);

// 纯算术表达式解析求值
ExprResult parse_and_eval(const char *expr);

// 编译并执行复杂表达式
int compile_and_execute(const char *expr, FunctionManager *fmgr, char *output, size_t output_size);

#endif