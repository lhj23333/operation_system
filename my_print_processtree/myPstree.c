#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>

#define VERSION "1.0.0.0"
#define MAX_PROCESSES 32768

typedef struct Process {
    int pid;
    int ppid;
    char comm[256];
    struct Process **children;
    int childCount;
    int childCapacity;
} Process;

typedef struct {
    char comm[256];
    int count;
    Process **procs;
    int procCount;
} MergedProc;

Process *processes[MAX_PROCESSES];
int processCount = 0;
bool isShowPids = false;
bool isNumericSort = false;

static void freeProcesses() {
    for(int i = 0; i < processCount; i++) {
        if(processes[i]->children) {
            free(processes[i]->children);
        }
        free(processes[i]);
    }
}

static bool processNameIsNumber(const char *pstr) {
    if(!pstr || !*pstr) return false;
    while (*pstr) {
        if(!isdigit(*pstr)) return false;
        pstr++;
    }
    return true;
}

static bool readProcessInfo(int pid, Process *proc) {
    char path[512];
    char line[1024];
    FILE *fp;
    
    proc->pid = pid;
    proc->ppid = 0;
    proc->children = NULL;
    proc->childCount = 0;
    proc->childCapacity = 0;
    
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    fp = fopen(path, "r");
    if(!fp) return false;
    
    if(fgets(proc->comm, sizeof(proc->comm), fp)) {
        proc->comm[strcspn(proc->comm, "\n")] = 0;
    }
    fclose(fp);
    
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if(!fp) return false;
    
    while(fgets(line, sizeof(line), fp)) {
        if(sscanf(line, "PPid:\t%d", &proc->ppid) == 1) break;
    }
    fclose(fp);
    
    return true;
}

static void addChild(Process *parent, Process *child) {
    if(parent->childCount >= parent->childCapacity) {
        parent->childCapacity = parent->childCapacity == 0 ? 4 : parent->childCapacity * 2;
        parent->children = (Process **)realloc(parent->children, 
                                               parent->childCapacity * sizeof(Process *));
        if(!parent->children) {
            perror("realloc");
            exit(1);
        }
    }
    parent->children[parent->childCount++] = child;
}

static int compareProcesses(const void *a, const void *b) {
    Process *p1 = *(Process**)a;
    Process *p2 = *(Process**)b;
    
    if(isNumericSort) {
        return p1->pid - p2->pid;
    } else {
        int cmp = strcmp(p1->comm, p2->comm);
        return cmp == 0 ? p1->pid - p2->pid : cmp;
    }
}

void scanProcfs() {
    DIR *dir = opendir("/proc");
    if(!dir) {
        perror("opendir /proc");
        exit(1);
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if(!processNameIsNumber(entry->d_name)) continue;
        
        int pid = atoi(entry->d_name);
        Process *proc = (Process *)malloc(sizeof(Process));
        if(!proc) {
            perror("malloc");
            continue;
        }
        
        if(readProcessInfo(pid, proc)) {
            processes[processCount++] = proc;
            if(processCount >= MAX_PROCESSES) break;
        } else {
            free(proc);
        }
    }
    closedir(dir);
}

void buildTree() {
    for(int i = 0; i < processCount; i++) {
        Process *child = processes[i];
        for(int j = 0; j < processCount; j++) {
            if(processes[j]->pid == child->ppid) {
                addChild(processes[j], child);
                break;
            }
        }
    }
    
    for(int i = 0; i < processCount; i++) {
        if(processes[i]->childCount > 0) {
            qsort(processes[i]->children, processes[i]->childCount, 
                  sizeof(Process *), compareProcesses);
        }
    }
}

void printTree(Process *proc, const char *prefix) {
    if (isShowPids) {
        printf("%s(%d)\n", proc->comm, proc->pid);
    } else {
        printf("%s\n", proc->comm);
    }
    
    for (int i = 0; i < proc->childCount; i++) {
        bool isChildLast = (i == proc->childCount - 1);
        
        printf("%s%s", prefix, isChildLast ? "└─" : "├─");
        
        char newPrefix[1024];
        snprintf(newPrefix, sizeof(newPrefix), "%s%s", prefix, isChildLast ? "  " : "│ ");
        
        printTree(proc->children[i], newPrefix);
    }
}

void printVersion() {
    printf("myPstree Version %s\n", VERSION);
}

void printUsage(const char *progname) {
    printf("Usage: %s [OPTION]...\n", progname);
    printf("Display a tree of processes.\n\n");
    printf("Options:\n");
    printf("  -p, --show-pids    Show PIDs\n");
    printf("  -n, --numeric-sort Sort by PID instead of name\n");
    printf("  -V, --version      Display version information\n");
    printf("  -h, --help         Display this help message\n");
}

int main(int argc, char *argv[]) {
    int opt;
    static struct option options[] = {
        {"show-pids", no_argument, 0, 'p'},
        {"numeric-sort", no_argument, 0, 'n'},
        {"version", no_argument, 0, 'V'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while((opt = getopt_long(argc, argv, "pnVh", options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                isShowPids = true;
                break;
            case 'n':
                isNumericSort = true;
                break;
            case 'V':
                printVersion();
                return 0;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    scanProcfs();
    
    if(processCount == 0) {
        fprintf(stderr, "No processes found\n");
        return 1;
    }
    
    buildTree();
    
    Process *root = NULL;
    for(int i = 0; i < processCount; i++) {
        if(processes[i]->pid == 1) {
            root = processes[i];
            break;
        }
    }
    
    if(!root) {
        for(int i = 0; i < processCount; i++) {
            if(processes[i]->ppid == 0) {
                root = processes[i];
                break;
            }
        }
    }
    
    if(root) {
        printTree(root, "");
    } else {
        fprintf(stderr, "Could not find root process\n");
    }
    
    freeProcesses();
    return 0;
}
