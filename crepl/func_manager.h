#ifndef FUNC_MANAGER_H
#define FUNC_MANAGER_H

#define MAX_FUNCTIONS 100
#define MAX_FUNC_NAME 64

#include <stdint.h>
#include <stdio.h>

typedef struct {
    char name[MAX_FUNC_NAME];   // 函数名
    char signature[512];        // 函数签名
    char *source_code;          // 源代码
    void *handle;               // deopen 句柄
    int func_id;                // 函数 ID
} FunctionDef;  

typedef struct {
    FunctionDef functions[MAX_FUNCTIONS];
    int count;
} FunctionManager;

// 添加函数
int func_manager_add(FunctionManager *fmgr, const char *func_source);

// 列出当前函数表
void func_manager_list(FunctionManager *fmgr);

// 获取函数句柄
FunctionDef* func_manager_get(FunctionManager *fmgr, const char *func_name);

// 初始化
FunctionManager* func_manager_init(void);

// 函数清除 
void func_manager_cleanup(FunctionManager *fmgr);

// 输出所有函数“原型”到文件（用于表达式编译时前置声明）
void emit_function_prototypes(FunctionManager *fmgr, FILE *out);

#endif