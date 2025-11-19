#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "func_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <regex.h>
#include <ctype.h>

#define LIBS_DIR "./libs"

#define COLOR_CYAN "\x1b[36m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_RED "\x1b[31m"
#define COLOR_RESET "\x1b[0m"

static int extract_function_name(const char *func_source, char *func_name, size_t name_size) {
    // 使用正则表达式
    regex_t regex;

    const char *pattern = "([a-zA-z_][a-zA-Z0-9]*)\\s*\\(";
    if(regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return -1;
    }

    const char *ptr = func_source;

    // 找到函数后 "(" 前一位置
    while(*ptr != '\0' && *ptr != '(') ptr ++;
    if(*ptr != '(') {
        regfree(&regex);
        return -1;
    }
    ptr --;

    // 向后查找函数名结尾
    while(ptr > func_source && isspace(*ptr)) ptr --;
    const char *name_end = ptr;

    // 向后查找函数名的开始
    while(ptr > func_source && (isalnum(*ptr) || *ptr == '_')) ptr --;
    ptr ++;
    
    int len = name_end - ptr + 1;
    if(len > 0 && len < (int)name_size) {
        strncpy(func_name, ptr, len);
        func_name[len] = '\0';
        regfree(&regex);
        return 0;
    }

    regfree(&regex);
    return -1;
}

static int compile_function_to_lib(FunctionManager *fmgr, int func_id) {
    FunctionDef *func = &fmgr->functions[func_id];

    // 生成动态库临时文件
    char lib_path[256];
    snprintf(lib_path, sizeof(lib_path), "%s/libfunc_%d.so", LIBS_DIR, func_id);

    char temp_source_file[256];
    snprintf(temp_source_file, sizeof(temp_source_file), "/tmp/func_XXXXXX.c");
    int fd = mkstemps(temp_source_file, 2);  // 2 = length of ".c" suffix
    if(fd == -1) {
        fprintf(stderr, "%s[ERROR]%s Failed to create temp file\n",
            COLOR_RED, COLOR_RESET);
        return -1;
    }
    FILE *fp = fdopen(fd, "w");
    if(!fp) {
        fprintf(stderr, "%s[ERROR]%s Failed to open temp file\n",
            COLOR_RED, COLOR_RESET);
        close(fd);
        unlink(temp_source_file);
        return -1;
    }
    fprintf(fp, "%s\n", func->source_code);
    fclose(fp);

    // 编译为共享库
    char compile_cmd[768];
    snprintf(compile_cmd, sizeof(compile_cmd),
        "gcc -shared -fPIC %s -o %s 2>/dev/null",
        temp_source_file, lib_path);
    int ret = system(compile_cmd);

    unlink(temp_source_file);
    if(ret != 0) {
        fprintf(stderr, "%s[ERROR]%s Compilation failed\n",
            COLOR_RED, COLOR_RESET);
        return -1;
    }
    
    // 加载动态库
    func->handle = dlopen(lib_path, RTLD_LAZY);
    if(!func->handle) {
        fprintf(stderr, "%s[ERROR]%s Failed to load library: %s%s\n", 
            COLOR_RED, COLOR_RESET, COLOR_GREEN, lib_path);
        return -1;
    }

    printf("%s[INFO]%s Function compiled: %s%s\n",
        COLOR_YELLOW, COLOR_RESET, COLOR_GREEN, lib_path);

    return 0;
}

int func_manager_add(FunctionManager *fmgr, const char *func_source) {
    if(fmgr->count >= MAX_FUNCTIONS) {
        fprintf(stderr, "%s[ERROR]%s Too many functions\n", 
            COLOR_RED, COLOR_RESET);
        return -1;
    }

    FunctionDef *func = &fmgr->functions[fmgr->count];
    func->func_id = fmgr->count;

    // 提取函数名
    if(extract_function_name(func_source, func->name, MAX_FUNC_NAME) != 0) {
        fprintf(stderr, "%s[ERROR]%s Failed to extract function name\n", 
            COLOR_RED, COLOR_RESET);
        return -1;
    }

    // 保存源代码
    func->source_code = malloc(strlen(func_source) + 1);
    if(!func->source_code) {
        fprintf(stderr, "%s[ERROR]%s Memory allocation failed\n",
            COLOR_RED, COLOR_RESET);
        return -1;
    }
    strcpy(func->source_code, func_source);

    strncpy(func->signature, func_source, sizeof(func->signature) - 1);
    func->signature[sizeof(func->signature) - 1] = '\0';

    // 编译源码
    if(compile_function_to_lib(fmgr, func->func_id) != 0) {
        fprintf(stderr, "%s[ERROR]%s Failed to compile function\n", 
            COLOR_RED, COLOR_RESET);
        
        free(func->source_code);
        func->source_code = NULL;
        memset(func, 0, sizeof(FunctionDef));
        
        return -1;
    }

    fmgr->count ++;
    printf("%s[INFO]%s Added function: %s\n", 
        COLOR_YELLOW, COLOR_RESET, func->name);

    return func->func_id;
}

void func_manager_list(FunctionManager *fmgr) {
    if(fmgr->count == 0) {
        printf("%s[INFO]%s No functions defined yet\n", COLOR_YELLOW, COLOR_RESET);
        return;
    }

    printf("\n%s╔════════════════════════════════════════════════════════════╗%s\n",
           COLOR_CYAN, COLOR_RESET);
    printf("%s║                   Defined Functions (%d)                    ║%s\n",
           COLOR_CYAN, fmgr->count, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════╣%s\n",
           COLOR_CYAN, COLOR_RESET);

    for(int i = 0; i < fmgr->count; i ++) {
        printf("%s║  %s[%d]%s %-50s    %s║%s\n",
               COLOR_CYAN, COLOR_GREEN, i, COLOR_RESET, 
               fmgr->functions[i].name, COLOR_CYAN, COLOR_RESET);
    }

    printf("%s╚════════════════════════════════════════════════════════════╝%s\n\n",
           COLOR_CYAN, COLOR_RESET);
}

FunctionManager* func_manager_init(void) {
    FunctionManager *fmgr = malloc(sizeof(FunctionManager));
    if (!fmgr) {
        fprintf(stderr, "%s[ERROR]%s Failed to allocate memory for FunctionManager\n",
            COLOR_RED, COLOR_RESET);
        return NULL;
    }
    fmgr->count = 0;
    memset(fmgr->functions, 0, sizeof(fmgr->functions));
    return fmgr;
}

FunctionDef* func_manager_get(FunctionManager *fmgr, const char *func_name) {
    for(int i = 0; i < fmgr->count; i ++) {
        if(strcmp(fmgr->functions[i].name, func_name) == 0) {
            return &fmgr->functions[i];
        }
    }
    return NULL;
}

void emit_function_prototypes(FunctionManager *fmgr, FILE *out) {
    for(int i = 0; i < fmgr->count; i ++) {
        const char *def = fmgr->functions[i].source_code;
        const char *brace = strchr(def, '{');
        if(!brace) continue;
        const char *p = brace - 1;
        while(p > def && isspace((unsigned char)*p)) p --;
        size_t len = p - def + 1;
        if (len > 0) {
            fprintf(out, "%.*s;\n", (int)len, def);
        }
    }
}

void func_manager_cleanup(FunctionManager *fmgr) {
    for(int i = 0; i < fmgr->count; i ++) {
        if(fmgr->functions[i].handle) {
            dlclose(fmgr->functions[i].handle);
        }
        if(fmgr->functions[i].source_code) {
            free(fmgr->functions[i].source_code);
        }
    }
    free(fmgr);
}
