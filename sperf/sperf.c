#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>

#define MAX_SYSCALLS 512
#define MAX_LINE 4096
#define REPORT_INTERVAL 0.1

typedef struct {
    char name[64];
    double total_time;
    int count;
} syscall_stat_t;

syscall_stat_t syscalls[MAX_SYSCALLS];
int syscall_count = 0;
struct timeval program_start;
double last_report_time = 0.0;
int child_pid = -1;

// 信号处理函数
static void signal_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        if(child_pid > 0) {
            kill(child_pid, SIGTERM);
        }
    }
}

// 获取当前的系统时间
static double get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int parse_strace_line(const char* line, char *syscall_name, double *time) {
    const char *paren = strchr(line, '(');
    if(!paren) return 0;

    const char *start = line;
    while(*start == ' '|| *start == '\t') start ++;

    int syscall_line = paren - start;
    if(syscall_line <= 0 || syscall_line >= 64) return 0;

    strncpy(syscall_name, start, syscall_line);
    syscall_name[syscall_line] = '\0';

    const char *time_start = strchr(line, '<');
    const char *time_end = strchr(line, '>');
    if(time_start && time_end && time_end > time_start) {
        *time = atof(time_start + 1);
        return 1;
    }

    return 0;
}

syscall_stat_t *find_or_create_syscall(const char *name) {
    for(int i = 0; i < syscall_count; i ++) {
        if(strcmp(syscalls[i].name, name) == 0) {
            return &syscalls[i];
        }
    }

    if(syscall_count < MAX_SYSCALLS) {
        strncpy(syscalls[syscall_count].name, name, 63);
        syscalls[syscall_count].name[63] = '\0';
        syscalls[syscall_count].count = 0;
        syscalls[syscall_count].total_time = 0.0;
        return &syscalls[syscall_count++];
    }
    return NULL;
}

static int compare_syscalls(const void *a, const void *b) {
    syscall_stat_t *sa = (syscall_stat_t*)a;
    syscall_stat_t *sb = (syscall_stat_t*)b;

    if(sb->total_time > sa->total_time) return -1;
    else if(sb->total_time < sa->total_time) return 1;
    else return 0;
    
}

void print_report(int is_final) {
    if(syscall_count == 0) return ;

    qsort(syscalls, syscall_count, sizeof(syscall_stat_t), compare_syscalls);

    // 计算总时间
    double total_time = 0.0;
    for(int i = 0; i < syscall_count; i ++) total_time += syscalls[i].total_time;

    // 清屏并输出表头
    if (!is_final) {
        printf("\033[2J\033[H");  // ANSI转义码：清屏并移动光标到左上角
    } else {
        printf("\n");
    }
    
    printf("================================================================================\n");
    if (is_final) {
        printf("                         Final Performance Report\n");
    } else {
        printf("                      Real-time Performance Report\n");
    }
    printf("================================================================================\n");
    printf("%-20s %10s %12s %10s\n", "Syscall", "Count", "Time(s)", "Percentage");
    printf("--------------------------------------------------------------------------------\n");

    // 输出数据
    int display_count = syscall_count >= 10 ? 10 : syscall_count;
    for(int i = 0; i < display_count; i ++) {
        double percentage = (total_time > 0) ? (syscalls[i].total_time / total_time * 100) : 0;
        printf("%-20s %10d %12.6f %9.2f%%\n",
            syscalls[i].name,
            syscalls[i].count,
            syscalls[i].total_time,
            percentage
        );
    }

    printf("--------------------------------------------------------------------------------\n");
    printf("Total Time: %.6f seconds\n", total_time);
    printf("Total Syscalls: %d types, ", syscall_count);

    int total_calls = 0;
    for(int i = 0; i < syscall_count; i ++) total_calls += syscalls[i].count;
    printf("%d calls\n", total_calls);
    printf("================================================================================\n");

    if(!is_final) {
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        fprintf(stderr, "Example: %s ls -l\n", argv[0]);
        return 1;
    }

    gettimeofday(&program_start, NULL);  // 记录程序开始时间

    int pipefd[2];
    if(pipe(pipefd) == -1) {             // 创建进程管道
        perror("pipe");
        return 1;
    }

    child_pid = fork();
    if(child_pid == -1) {
        perror("fork");
        return 1;
    }

    if(child_pid == 0){
        // ========== 子进程 ==========
        close(pipefd[0]);

        // 将 stderr 重定向到管道写端（strace 默认输出到 stderr）
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char** strace_args = malloc(sizeof(char *)*(argc + 2));
        if(!strace_args) {
            perror("malloc");
            exit(1);
        }
        strace_args[0] = "strace";
        strace_args[1] = "-T";

        for(int i = 1; i < argc; i ++) {
            strace_args[i + 1] = argv[i];
        }
        strace_args[argc + 1] = NULL;

        extern char **environ;
        execve("/usr/bin/strace", strace_args, environ);
        perror("execve strace");
        exit(1);
    } else {
        // ========== 父进程 ==========
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        close(pipefd[1]);

        FILE *fp = fdopen(pipefd[0], "r");
        if(!fp) {
            perror("fdopen");
            close(pipefd[0]);
            kill(child_pid, SIGTERM);
            return 1;
        }

        char line[MAX_LINE];
        char syscall_name[64];
        double syscall_time;

        printf("Starting performance monitoring...\n");
        printf("Press Ctrl+C to stop\n\n");

        last_report_time = get_current_time();

        while (fgets(line, sizeof(line), fp) != NULL) {
            if(parse_strace_line(line, syscall_name, &syscall_time)) {
                syscall_stat_t* stat = find_or_create_syscall(syscall_name);
                if(stat) {
                    stat->count ++;
                    stat->total_time += syscall_time;
                }

                double current_time = get_current_time();
                if(current_time - last_report_time >= REPORT_INTERVAL) {
                    print_report(0);
                    last_report_time = current_time;
                }
            }
        }
        fclose(fp);

        int status;
        waitpid(child_pid, &status, 0);         // 等待子进程结束
        print_report(1);                        // 输出最终报告

        printf("\nProgram exited with status: %d\n", WEXITSTATUS(status));
    }
    return 0;
}