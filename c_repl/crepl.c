#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>

#include "expr_parser.h"
#include "func_manager.h"

// color define
#define COLOR_RESET "\x1b[0m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_RED "\x1b[31m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_CYAN "\x1b[36m"

// Constant definition
#define LIBS_DIR "./libs"
#define MAX_INPUT_LEN 4096

// Global function manager
FunctionManager *g_func_manager = NULL;

typedef enum {
    INPUT_EXPRESSION,
    INPUT_FUNCTION,
    INPUT_COMMAND,
    INPUT_INVALID
} InputType;

// tool function
void trim_string(char *str) {
    int start = 0;
    while (isspace((unsigned char)str[start])) start ++;

    int end = strlen(str) - 1;
    while (end >= 0 && isspace((unsigned char)str[end])) end --;
    if (start > 0) {
        memmove(str, str + start, end - start + 2);
    }
    str[end - start + 1] = '\0';
}

int is_whitespace(const char *str) {
    for (int i = 0; str[i] != '\0'; i ++) {
        if(!isspace((unsigned char)str[i])) {
            return 0;
        }
    }
    return 1;
}

// Clean function
void cleanup_libs(void) {
    DIR *dir = opendir(LIBS_DIR);
    if(dir == NULL) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if(entry->d_type == DT_REG) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", LIBS_DIR, entry->d_name);
            if(unlink(filepath) == -1) {
                fprintf(stderr, "%s[WARN]%s Failed to delete: %s\n",
                    COLOR_YELLOW, COLOR_RESET, filepath);
            }
        }
    }
    closedir(dir);

    if(rmdir(LIBS_DIR) == -1) {
        fprintf(stderr, "%s[WARN]%s Failed to remove libs directory\n",
            COLOR_YELLOW, COLOR_RESET);
    }
}

void cleanup_handler() {
    printf("%s[INFO]%s Cleaning up...\n", COLOR_YELLOW, COLOR_RESET);

    if(g_func_manager) {
        func_manager_cleanup(g_func_manager);
    }
    cleanup_libs();
    printf("%s[INFO]%s Cleanup complete\n", COLOR_YELLOW, COLOR_RESET);
}


// Process function
void show_help(void) {
    printf("\n");
    printf("%s╔═══════════════════════════════════════════════════════════════════╗%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║                    Available Commands                             ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s╠═══════════════════════════════════════════════════════════════════╣%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║                                                                   ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║  %s[Expression evaluation]%s Enter a C expression such as: 2 + 3 * 4  ║%s\n",
           COLOR_CYAN, COLOR_GREEN, COLOR_CYAN, COLOR_RESET);
    printf("%s║    support: +, -, *, /, %%, (), int or float                       ║%s\n",
           COLOR_CYAN, COLOR_RESET);
    printf("%s║                                                                   ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║  %s[Function define]%s Enter a C function such as:                    ║%s\n",
           COLOR_CYAN, COLOR_GREEN, COLOR_CYAN, COLOR_RESET);
    printf("%s║    int add(int a, int b) { return a + b; }                        ║%s\n",
           COLOR_CYAN, COLOR_RESET);
    printf("%s║                                                                   ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║  %shelp%s   - show help                                               ║%s\n",
           COLOR_CYAN, COLOR_YELLOW, COLOR_CYAN, COLOR_RESET);
    printf("%s║  %slist%s   - list defined func                                       ║%s\n",
           COLOR_CYAN, COLOR_YELLOW, COLOR_CYAN, COLOR_RESET);
    printf("%s║  %sclear%s  - clear screen                                            ║%s\n",
           COLOR_CYAN, COLOR_YELLOW, COLOR_CYAN, COLOR_RESET);
    printf("%s║  %sexit%s   - exit REPL (or press Ctrl+D)                             ║%s\n",
           COLOR_CYAN, COLOR_YELLOW, COLOR_CYAN, COLOR_RESET);
    printf("%s║                                                                   ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s╚═══════════════════════════════════════════════════════════════════╝%s\n\n", 
           COLOR_CYAN, COLOR_RESET);
}

void handle_command(const char *cmd) {
    char temp[MAX_INPUT_LEN];
    strncpy(temp, cmd, MAX_INPUT_LEN - 1);
    temp[MAX_INPUT_LEN - 1] = '\0';
    trim_string(temp);

    for(int i = 0; temp[i]; i ++) {
        temp[i] =  tolower((unsigned char)temp[i]);
    }

    if(strcmp(temp, "exit") == 0 || strcmp(temp, "quit") == 0) {
        printf("%s[INFO]%s Exiting REPL....\n", COLOR_YELLOW, COLOR_RESET);
        exit(0);
    } else if(strcmp(temp, "help") == 0) {
        show_help();
    } else if(strcmp(temp, "list") == 0 || strcmp(temp, "funcs") == 0) {
        func_manager_list(g_func_manager);
    } else if(strcmp(temp, "clear") == 0) {
        system("clear");
    } else {
        fprintf(stderr, "%s[ERROR]%s Unknown command: %s\n", COLOR_RED, COLOR_RESET, cmd);
        printf("        Type 'help' for available commands\n");
    } 
}

void execute_expression(const char *expr) {
    printf("\n");

    if(is_simple_arithmetic_expression(expr)) {
        ExprResult result = parse_and_eval(expr);
        if(result.is_valid) {
            if(strcmp(result.type, "int") == 0) {
                printf("%s=> %d%s \n\n", 
                    COLOR_GREEN, (int)result.value, COLOR_RESET);
            } else {
                printf("%s=> %f%s \n\n", 
                    COLOR_GREEN, result.value, COLOR_RESET);
            } 
        } else {
            printf("%s[ERROR]%s %s\n\n", 
                COLOR_RED, COLOR_RESET, result.error_msg);
        }
        return ;
    }

    char buf[512];
    int rc = compile_and_execute(expr, g_func_manager, buf, sizeof(buf));
    if(rc == 0) {
        trim_string(buf);
        if(buf[0] != '\0') {
            printf("%s=> %s%s \n\n", 
                COLOR_GREEN, buf, COLOR_RESET);
        } else {
            printf("%s=> (no output)%s\n", 
                COLOR_GREEN, COLOR_RESET);
        }
    } else {
        printf("%s[ERROR]%s %s\n", 
            COLOR_RED, COLOR_RESET, buf);
    }
}

void define_function(const char *func_def) {
    printf("\n");

    int func_id = func_manager_add(g_func_manager, func_def);

    if(func_id >= 0) {
        printf("%s[SUCCESS]%s Function added successfully (ID: %d)\n\n", 
            COLOR_YELLOW, COLOR_RESET, func_id);
    } else {
        printf("%s[ERROR]%s Failed to define function\n\n", 
            COLOR_RED, COLOR_RESET);
    }
}

// Input function
InputType classify_input(const char *input) {
    char temp[MAX_INPUT_LEN];
    strncpy(temp, input, MAX_INPUT_LEN - 1);
    temp[MAX_INPUT_LEN - 1] = '\0';
    trim_string(temp);

    if(strlen(temp) == 0) {
        return INPUT_INVALID;
    }

    if(isalpha(temp[0]) ) {
        int has_paren = strchr(temp, '(') != NULL;
        int has_brace = strchr(temp, '{') != NULL;
        
        if(has_brace && has_paren) {
            return INPUT_FUNCTION;
        }
        if(has_paren) {
            return INPUT_EXPRESSION;
        }
        
        return INPUT_COMMAND;
    }

    if(strchr(temp, '{') != NULL && strchr(temp, '}') != NULL) {
        return INPUT_FUNCTION;
    }

    return INPUT_EXPRESSION;
}

void handle_input(const char *input) {
    InputType type = classify_input(input);

    switch(type) {
        case INPUT_COMMAND:
            handle_command(input);
            break;
        case INPUT_EXPRESSION:
            execute_expression(input);
            break;
        case INPUT_FUNCTION:
            define_function(input);
            break;
        case INPUT_INVALID:
            printf("%s[WARN]%s Invaild input\n", 
                COLOR_YELLOW, COLOR_RESET);
            break;
    }
}

// Init function
void init_repl() {
    struct stat sb;
    if(stat(LIBS_DIR, &sb) == -1) {
        if(mkdir(LIBS_DIR, 0755) == -1) {
            fprintf(stderr, "%s[ERROR]%s Failed to create libs directory\n",
                    COLOR_RED, COLOR_RESET);
            exit(1);
        }
    }
    g_func_manager = func_manager_init();
    atexit(cleanup_handler);
    printf("%s[INFO]%s C REPL Initialized sucessfully\n",
            COLOR_YELLOW, COLOR_RESET);
}

int main() {
    printf("\n");
    printf("%s╔════════════════════════════════════════════════════════════╗%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║           C REPL - Read-Eval-Print-Loop v1.0               ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║                Type 'help' for commands                    ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s╚════════════════════════════════════════════════════════════╝%s\n\n", 
           COLOR_CYAN, COLOR_RESET);

    init_repl();

    char *input = NULL;
    while(1) {
        input = readline("> ");
        if(input == NULL) {
            printf("\n");
            break;
        }

        if(is_whitespace(input)) {
            free(input);
            continue;
        }

        add_history(input);
        handle_input(input);
        free(input);
    }
    
    return 0;
}

