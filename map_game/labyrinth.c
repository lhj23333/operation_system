#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_SIZE 100

int playerID = -1;
int moveStep = 1;
char *direction = NULL;
char *mapFile = NULL;

// 全局变量
char **map = NULL;
int rows = 0;
int *col_lens = NULL;  // 每行实际长度

void freeSpace(char **object, int *col_lens, int rows) {
    if (object) {
        for (int i = 0; i < rows; i++) {
            if (object[i]) free(object[i]);
        }
        free(object);
    }
    if (col_lens) free(col_lens);
}

bool **visitedNew(int rows, int *col_lens) {
    bool **v = calloc(rows, sizeof(bool*));
    if (!v) { perror("calloc"); exit(1); }
    for (int i = 0; i < rows; ++i) {
        v[i] = calloc(col_lens[i], sizeof(bool));
        if (!v[i]) {
            perror("calloc");
            for (int j = 0; j < i; ++j) free(v[j]);
            free(v);
            exit(1);
        }
    }
    return v;
}

void dfs(char **map, bool **vis, int *col_lens, int r, int c, int rows) {
    if (r < 0 || r >= rows || c < 0 || c >= col_lens[r] || vis[r][c] || map[r][c] == '#') return;
    vis[r][c] = true;
    dfs(map, vis, col_lens, r - 1, c, rows);
    dfs(map, vis, col_lens, r + 1, c, rows);
    dfs(map, vis, col_lens, r, c + 1, rows);
    dfs(map, vis, col_lens, r, c - 1, rows);
}

char **loadMap(const char *path, int *rows_out, int **col_lens_out) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[Error]: Map file not found: %s\n", path);
        exit(1);
    }

    char **lines = NULL;
    int *lens = NULL;
    char buf[512];
    int row_count = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';
        if (len == 0) continue;

        char *line = strdup(buf);
        if (!line) {
            perror("strdup");
            freeSpace(lines, lens, row_count);
            fclose(fp);
            exit(1);
        }

        char **newLines = realloc(lines, (row_count + 1) * sizeof(char*));
        int *newLens = realloc(lens, (row_count + 1) * sizeof(int));
        if (!newLines || !newLens) {
            perror("realloc");
            free(line);
            freeSpace(lines, lens, row_count);
            fclose(fp);
            exit(1);
        }
        lines = newLines;
        lens = newLens;

        lines[row_count] = line;
        lens[row_count] = (int)len;
        row_count++;
    }
    fclose(fp);

    if (row_count == 0) {
        fprintf(stderr, "[Error]: Empty map\n");
        freeSpace(lines, lens, row_count);
        exit(1);
    }

    *rows_out = row_count;
    *col_lens_out = lens;
    return lines;
}

bool isConnected(char **map, int *col_lens, int rows) {
    bool **vis = visitedNew(rows, col_lens);
    int startR = -1, startC = -1;
    int total = 0;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < col_lens[i]; j++) {
            if (map[i][j] != '#') {
                total++;
                if (startR == -1) {
                    startR = i;
                    startC = j;
                }
            }
        }
    }

    if (total == 0) {
        for (int i = 0; i < rows; i++) free(vis[i]);
        free(vis);
        return false;
    }

    dfs(map, vis, col_lens, startR, startC, rows);

    int reached = 0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < col_lens[i]; j++) {
            if (vis[i][j]) reached++;
        }
    }

    for (int i = 0; i < rows; i++) free(vis[i]);
    free(vis);

    return reached == total;
}

bool findPlayer(char **map, int *col_lens, int rows, int id, int *pr, int *pc) {
    char ch = '0' + id;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < col_lens[i]; j++) {
            if (map[i][j] == ch) {
                *pr = i; *pc = j;
                return true;
            }
        }
    }
    return false;
}

bool placePlayer(char **map, int *col_lens, int rows, int id, int *pr, int *pc) {
    char ch = '0' + id;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < col_lens[i]; j++) {
            if (map[i][j] == '.') {
                map[i][j] = ch;
                *pr = i; *pc = j;
                return true;
            }
        }
    }
    return false;
}

bool movePlayer(char **map, int *col_lens, int rows, int r, int c, const char *dir, int step, int id) {
    if (!dir) return false;

    int nr = r, nc = c;
    if (strcmp(dir, "up") == 0) nr -= step;
    else if (strcmp(dir, "down") == 0) nr += step;
    else if (strcmp(dir, "left") == 0) nc -= step;
    else if (strcmp(dir, "right") == 0) nc += step;
    else return false;

    if (nr < 0 || nr >= rows || nc < 0 || nc >= col_lens[nr] || map[nr][nc] != '.') {
        return false;
    }

    char ch = '0' + id;
    map[r][c] = '.';
    map[nr][nc] = ch;
    return true;
}

void saveMapToFile(char **map, int rows, int *col_lens, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) { perror("fopen"); return; }
    for (int i = 0; i < rows; i++) {
        fprintf(fp, "%.*s\n", col_lens[i], map[i]);
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s -m <mapfile> -p <id> [-d <dir>] [-s <step>]\n", argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        if (!argv[i]) {
            fprintf(stderr, "Error: NULL argument at position %d\n", i);
            exit(1);
        }
        if (strcmp(argv[i], "-m") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Missing argument for -m\n"); exit(1); }
            mapFile = argv[++i];
        }
        else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Missing argument for -p\n"); exit(1); }
            playerID = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Missing argument for -d\n"); exit(1); }
            direction = argv[++i];
        }
        else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Missing argument for -s\n"); exit(1); }
            moveStep = atoi(argv[++i]);
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            exit(1);
        }
    }

    if (!mapFile) {
        fprintf(stderr, "Error: -m <mapfile> is required\n");
        exit(1);
    }
    map = loadMap(mapFile, &rows, &col_lens);

    if (!isConnected(map, col_lens, rows)) {
        freeSpace(map, col_lens, rows);
        exit(1);
    }

    int pr, pc;
    bool hasPlayer = findPlayer(map, col_lens, rows, playerID, &pr, &pc);

    if (direction) {
        if (!hasPlayer) {
            if (!placePlayer(map, col_lens, rows, playerID, &pr, &pc)) {
                freeSpace(map, col_lens, rows);
                exit(1);
            }
        }
        bool canMove = movePlayer(map, col_lens, rows, pr, pc, direction, moveStep, playerID);
        if (canMove) {
            saveMapToFile(map, rows, col_lens, mapFile);
            for (int i = 0; i < rows; ++i) puts(map[i]);
        }
        freeSpace(map, col_lens, rows);
        return canMove ? 0 : 1;
    } else {
        if (!hasPlayer) {
            fprintf(stderr, "Error: Player %d not found in map\n", playerID);
            freeSpace(map, col_lens, rows);
            exit(1);
        }
        for (int i = 0; i < rows; ++i) puts(map[i]);
        freeSpace(map, col_lens, rows);
        return 0;
    }
}