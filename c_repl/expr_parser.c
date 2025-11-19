#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include "expr_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "func_manager.h"

// 词法分析 Lexer
typedef enum {
    TOKEN_NUMBER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MUL,
    TOKEN_DIV,
    TOKEN_MOD,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_EOF,
    TOKEN_ERROR
} token_type;

typedef struct {
    token_type type;
    double value;
} Token;

typedef struct {
    const char *input;
    size_t pos;
    Token current_token;
} Lexer;

static Lexer* lexer_create(const char *input) {
    Lexer *lex = malloc(sizeof(Lexer));
    lex->input = input;
    lex->pos = 0;
    return lex;
}

static void lexer_destroy(Lexer *lex) {
    free(lex);
}

static void lexer_next_token(Lexer *lex) {
    while (lex->pos < strlen(lex->input) && isspace(lex->input[lex->pos])) lex->pos ++;
    if(lex->pos >= strlen(lex->input)) {
        lex->current_token.type = TOKEN_EOF;
        return;
    }

    char c = lex->input[lex->pos];
    
    // 数字
    if(isdigit(c) || (c == '.' && lex->pos + 1 < strlen(lex->input) && isdigit(lex->input[lex->pos + 1]))) {
        char num_str[64] = {0};
        size_t i = 0;
        while (lex->pos < strlen(lex->input) && (isdigit(lex->input[lex->pos]))) {
            num_str[i ++] = lex->input[lex->pos ++];
        }
        lex->current_token.type = TOKEN_NUMBER;
        lex->current_token.value = atof(num_str);
        return ;
    }

    // 运算符
    lex->pos ++;
    switch (c) {
        case '+': lex->current_token.type = TOKEN_PLUS; break;
        case '-': lex->current_token.type = TOKEN_MINUS; break;
        case '*': lex->current_token.type = TOKEN_MUL; break;
        case '/': lex->current_token.type = TOKEN_DIV; break;
        case '%': lex->current_token.type = TOKEN_MOD; break;
        case '(': lex->current_token.type = TOKEN_LPAREN; break;
        case ')': lex->current_token.type = TOKEN_RPAREN; break;
        default: lex->current_token.type = TOKEN_ERROR; break;
    }
}

// 语法分析 Parser
typedef struct {
    Lexer *lexer;
} Parser;

static double parse_expression(Parser *parser);
static double parse_term(Parser *parser);
static double parse_factor(Parser *parser);

static Parser* parser_create(const char *input) {
    Parser *par = malloc(sizeof(Parser));
    par->lexer = lexer_create(input);
    lexer_next_token(par->lexer);
    return par;
}

static void parser_destory(Parser *par) {
    lexer_destroy(par->lexer);
    free(par);
}

// 表达式: 项 + 项 - 项 ...
static double parse_expression(Parser *par) {
    double result = parse_term(par);

    while (par->lexer->current_token.type == TOKEN_PLUS ||
           par->lexer->current_token.type == TOKEN_MINUS) {
        token_type op = par->lexer->current_token.type;
        lexer_next_token(par->lexer);
        double right = parse_term(par);

        if(op == TOKEN_PLUS) result += right;
        else result -= right;
    }
    return result;
}

// 项: 因子 * 因子 / 因子 % 因子 ...
static double parse_term(Parser *par) {
    double result = parse_factor(par);

    while(par->lexer->current_token.type == TOKEN_MUL ||
          par->lexer->current_token.type == TOKEN_DIV ||
          par->lexer->current_token.type == TOKEN_MOD) {
        token_type op = par->lexer->current_token.type;
        lexer_next_token(par->lexer);
        double right = parse_factor(par);

        if (op == TOKEN_MUL) {
            result *= right;
        } else if (op == TOKEN_DIV) {
            if(right == 0) return NAN;
            result /= right;
        } else if (op == TOKEN_MOD) {
            if(right == 0) return NAN;
            result = (int)result % (int)right;
        }
    }
    return result;
}

// 因子: 数字 | (表达式) | -因子 | +因子
static double parse_factor(Parser *par) {
    if(par->lexer->current_token.type == TOKEN_NUMBER) {
        double value = par->lexer->current_token.value;
        lexer_next_token(par->lexer);
        return value;
    }

    if(par->lexer->current_token.type == TOKEN_LPAREN) {
        lexer_next_token(par->lexer);
        double value = parse_expression(par);
        if(par->lexer->current_token.type != TOKEN_RPAREN) {
            return NAN;     // 缺少右括号
        }
        lexer_next_token(par->lexer);
        return value;
    }

    if(par->lexer->current_token.type == TOKEN_PLUS) {
        lexer_next_token(par->lexer);
        return parse_factor(par);
    }
    if(par->lexer->current_token.type == TOKEN_MINUS) {
        lexer_next_token(par->lexer);
        return parse_factor(par);
    }
    return NAN;     // 无效因子
}

int is_simple_arithmetic_expression(const char *expr) {
    for (const char *p = expr; *p != '\0'; ++p) {
        if(isspace((unsigned char)*p)) continue;
        if(isdigit((unsigned char)*p)) continue;
        if(*p == '+' || *p == '-' || *p == '*' || *p == '/' ||
           *p == '%' || *p == '(' || *p == ')' || *p == '.') continue;
        return 0; 
    }
    return 1;
}

ExprResult parse_and_eval(const char *expr) {
    ExprResult result = {0};

    // 创建解析器
    Parser *par = parser_create(expr);

    // 解析表达式
    double value = parse_expression(par);

    if(par->lexer->current_token.type != TOKEN_EOF) {
        result.is_valid = 0;
        strncpy(result.error_msg, "Unexpected tokens after expression", sizeof(result.error_msg) - 1);
        strncpy(result.type, "error", sizeof(result.type) - 1);
    } else if(isnan(value)) {
        result.is_valid = 0;
        strncpy(result.error_msg, "Invalid expression or division by zero", sizeof(result.error_msg) - 1);
        strncpy(result.type, "error", sizeof(result.type) - 1);
    } else {
        result.is_valid = 1;
        result.value = value;

        if(value == floor(value)) {
            strncpy(result.type, "int", sizeof(result.type) - 1);
        } else {
            strncpy(result.type, "double", sizeof(result.type) - 1);
        }
    }

    parser_destory(par);
    return result;
}

int compile_and_execute(const char *expr, FunctionManager *fmgr, char *output, size_t output_size) {
    if(!output || output_size == 0) return -1;
    output[0] = '\0';

    if(strchr(expr, ';') || strchr(expr, '{') || strchr(expr, '}') ||
       strstr(expr, "#include") || strstr(expr, "#define")) {
        snprintf(output, output_size, "Rejected: expression contains forbidden tokens(only single expression allowed)");
        return -1;
    }

    // 使用 mkstemp 创建安全的临时文件
    char src_template[] = "/tmp/temp_expr_XXXXXX.c";
    int fd = mkstemps(src_template, 2);  // 2 = length of ".c" suffix
    if(fd == -1) {
        snprintf(output, output_size, "Failed to create temp file");
        return -1;
    }
    FILE *temp_file = fdopen(fd, "w");
    if(!temp_file) {
        snprintf(output, output_size, "Failed to open temp file");
        close(fd);
        unlink(src_template);
        return -1;
    }
    

    fprintf(temp_file, "#include <stdio.h>\n");
    fprintf(temp_file, "#include <math.h>\n");
    if(fmgr && fmgr->count > 0) {
        emit_function_prototypes(fmgr, temp_file);
        fprintf(temp_file, "\n");
    }
    fprintf(temp_file, "int main() {\n");
    fprintf(temp_file, "    double _val = (%s);\n", expr);
    fprintf(temp_file, "    if (_val == floor(_val)) {\n");
    fprintf(temp_file, "        printf(\"%%d\\n\\n\", (int)_val);\n");
    fprintf(temp_file, "    } else {\n");
    fprintf(temp_file, "        printf(\"%%.6f\\n\\n\", _val);\n");
    fprintf(temp_file, "    }\n");
    fprintf(temp_file, "    return 0;\n");
    fprintf(temp_file, "}\n");
    fclose(temp_file);

    // 构建编译文件路径 bin_path
    char bin_path[256];
    strncpy(bin_path, src_template, sizeof(bin_path) - 1);
    bin_path[sizeof(bin_path) - 1] = '\0';
    char *dot = strrchr(bin_path, '.');
    if(dot) *dot = '\0';    // 去除 .c
    
    // 构造链接 libs 中的 .so 参数
    char libs_args[1024] = {0};
    if(fmgr && fmgr->count > 0) {
        DIR *dir = opendir("./libs");
        if(dir) {
            struct dirent *ent;
            size_t off = 0;
            while((ent = readdir(dir)) != NULL) {
                if(ent->d_type == DT_REG) {
                    const char *name = ent->d_name;
                    size_t name_len = strlen(name);
                    if(name_len > 3 && strstr(name, ".so")) {
                        char path[256];
                        snprintf(path, sizeof(path), " libs/%s", name);
                        size_t path_len = strlen(path);
                        if(off + path_len < sizeof(libs_args) - 1) {
                            strcpy(libs_args + off, path);
                            off += path_len;
                        }
                    }
                }
            }
            closedir(dir);
        }
    }

    // 执行编译
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
        "gcc -Wall -Wextra -std=c99 -O0 %s -o %s -lm -Wl,-rpath,'$ORIGIN/libs'%s 2>&1",
        src_template, bin_path, libs_args);

    FILE *pipe = popen(cmd, "r");
    if(!pipe) {
        snprintf(output, output_size, "Failed to run gcc");
        unlink(src_template);
        return -1;
    }

    // 获取编译输出(如有错误)
    char compile_log[1024] = {0};
    size_t clog_len = 0;
    char line[256];
    while (fgets(line, sizeof(line), pipe)) {
        size_t l = strlen(line);
        if(clog_len + l < sizeof(compile_log) - 1) {
            strcpy(compile_log + clog_len, line);
            clog_len += l;
        }
    }
    int gcc_status = pclose(pipe);
    if(gcc_status != 0) {
        snprintf(output, output_size, "Compile failed:\n%s", compile_log);
        unlink(src_template);
        return -1;
    }

    // 运行可执行文件
    FILE *run_pipe = popen(bin_path, "r");
    if (!run_pipe) {
        snprintf(output, output_size, "Failed to run binary");
        unlink(src_template);
        unlink(bin_path);
        return -1;
    }
    char run_out[512] = {0};
    size_t runlog_len = 0;
    while(fgets(line, sizeof(line), run_pipe)) {
        size_t l = strlen(line);
        if(runlog_len + l < sizeof(run_out) - 1) {
            strcpy(run_out + runlog_len, line);
            runlog_len += l;
        }   
    }
    pclose(run_pipe);

    snprintf(output, output_size, "%s", run_out);

    unlink(src_template);
    unlink(bin_path);
    return 0;
}
