#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "boardgen.c"
// #include <time.h>  // included in boardgen.c

#define N 9
#define DIM 3
#define WIN_X 700
#define WIN_Y 100
#define X_STEP 210
#define Y_STEP 220
#define BUF_SZ 32

#define IS_VALID(r, c, v) \
    ((r) >= 0 && (r) < N && \
     (c) >= 0 && (c) < N && \
     (v) >= 1 && (v) <= N)

typedef struct {
    int in;
    int out;
} Pipe;

static int main_out;

int check_fd(int fd) {
    return fd != -1;
}

void get_pos(int idx, int* x, int* y) {
    *x = (idx % DIM) * X_STEP + WIN_X;
    *y = (idx / DIM) * Y_STEP + WIN_Y;
}

void get_row_adj(int idx, int* a1, int* a2) {
    int base = (idx / DIM) * DIM;
    *a1 = (idx + 1) % DIM + base;
    *a2 = (idx + 2) % DIM + base;
}

void get_col_adj(int idx, int* a1, int* a2) {
    int col = idx % DIM;
    *a1 = (idx + DIM) % N;
    *a2 = (idx + 2 * DIM) % N;
    
    if (*a1 % DIM != col) {
        *a1 = (*a1 / DIM) * DIM + col;
    }
    if (*a2 % DIM != col) {
        *a2 = (*a2 / DIM) * DIM + col;
    }
}

char* find_term() {
    const char* paths[] = {
        "/usr/bin/xterm",
        "/bin/xterm",
        "/usr/local/bin/xterm",
        "xterm"
    };
    
    for (int i = 0; i < 4; i++) {
        if (access(paths[i], X_OK) == 0 || 
            strcmp(paths[i], "xterm") == 0) {
            return strdup(paths[i]);
        }
    }
    return NULL;
}

void print_help() {
    printf("\nCommands:\n");
    printf("h - Help\n");
    printf("n - New game\n");
    printf("p r c v - Put value v in cell c of region r\n");
    printf("s - Show solution\n");
    printf("q - Quit\n\n");
    printf("Regions/cells: 0-8 row by row\n");
}

void init_proc(int idx, Pipe* pipes, int* adj_out) {
    char id[4], in[8], out[8];
    char adj[4][8];
    
    int x, y;
    get_pos(idx, &x, &y);
    
    char geo[BUF_SZ];
    sprintf(geo, "17x8+%d+%d", x, y);
    
    sprintf(id, "%d", idx);
    sprintf(in, "%d", pipes[idx].in);
    sprintf(out, "%d", pipes[idx].out);
    
    for (int i = 0; i < 4; i++) {
        sprintf(adj[i], "%d", adj_out[i]);
    }
    
    char title[BUF_SZ];
    sprintf(title, "Region %d", idx);
    
    char* term = find_term();
    if (!term) {
        fprintf(stderr, "xterm not found\n");
        exit(1);
    }
    
    execl(term, "xterm",
          "-T", title,
          "-fa", "Monospace",
          "-fs", "15",
          "-geometry", geo,
          "-bg", "white",
          "-e", "./block",
          id, in, out,
          adj[0], adj[1], adj[2], adj[3],
          NULL);
    
    free(term);
    perror("exec failed");
    exit(1);
}

int main() {
    Pipe pipes[N];
    pid_t pids[N];
    int grid[N][N];
    int soln[N][N];
    char cmd;
    
    main_out = dup(STDOUT_FILENO);
    
    // Create pipes
    for (int i = 0; i < N; i++) {
        int p[2];
        if (pipe(p) == -1) {
            perror("pipe failed");
            exit(1);
        }
        pipes[i].in = p[0];
        pipes[i].out = p[1];
    }
    
    // Fork processes
    for (int idx = 0; idx < N; idx++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            int h1, h2, v1, v2;
            get_row_adj(idx, &h1, &h2);
            get_col_adj(idx, &v1, &v2);
            
            int adj[4] = {
                pipes[h1].out,
                pipes[h2].out,
                pipes[v1].out,
                pipes[v2].out
            };
            
            // Close unused pipes
            for (int i = 0; i < N; i++) {
                if (i != idx) {
                    close(pipes[i].in);
                }
                if (!((i == h1) || (i == h2) || 
                      (i == v1) || (i == v2) || (i == idx))) {
                    close(pipes[i].out);
                }
            }
            
            init_proc(idx, pipes, adj);
        } else if (pid > 0) {
            pids[idx] = pid;
        } else {
            perror("fork failed");
            exit(1);
        }
    }
    
    print_help();
    while (1) {
        printf("\nEnter command: ");
        scanf(" %c", &cmd);
        
        if (cmd == 'h') {
            print_help();
            continue;
        }
        
        if (cmd == 'n') {
            newboard(grid, soln);
            for (int idx = 0; idx < N; idx++) {
                int old = dup(STDOUT_FILENO);
                dup2(pipes[idx].out, STDOUT_FILENO);
                printf("n ");
                int r = (idx / DIM) * DIM;
                int c = (idx % DIM) * DIM;
                for (int i = 0; i < DIM; i++) {
                    for (int j = 0; j < DIM; j++) {
                        printf("%d ", grid[r + i][c + j]);
                    }
                }
                printf("\n");
                fflush(stdout);
                dup2(old, STDOUT_FILENO);
            }
            continue;
        }
        
        if (cmd == 'p') {
            int r, c, v;
            scanf("%d %d %d", &r, &c, &v);
            if (!IS_VALID(r, c, v)) {
                printf("Invalid input\n");
                continue;
            }
            int old = dup(STDOUT_FILENO);
            dup2(pipes[r].out, STDOUT_FILENO);
            printf("p %d %d\n", c, v);
            fflush(stdout);
            dup2(old, STDOUT_FILENO);
            continue;
        }
        
        if (cmd == 's') {
            for (int idx = 0; idx < N; idx++) {
                int old = dup(STDOUT_FILENO);
                dup2(pipes[idx].out, STDOUT_FILENO);
                printf("s ");
                int r = (idx / DIM) * DIM;
                int c = (idx % DIM) * DIM;
                
                for (int i = 0; i < DIM; i++) {
                    for (int j = 0; j < DIM; j++) {
                        printf("%d ", grid[r + i][c + j]);
                    }
                }
                
                for (int i = 0; i < DIM; i++) {
                    for (int j = 0; j < DIM; j++) {
                        printf("%d ", soln[r + i][c + j]);
                    }
                }
                printf("\n");
                fflush(stdout);
                dup2(old, STDOUT_FILENO);
            }
            continue;
        }
        
        if (cmd == 'q') {
            for (int i = 0; i < N; i++) {
                int old = dup(STDOUT_FILENO);
                dup2(pipes[i].out, STDOUT_FILENO);
                printf("q\n");
                fflush(stdout);
                dup2(old, STDOUT_FILENO);
            }
            
            for (int i = 0; i < N; i++) {
                waitpid(pids[i], NULL, 0);
            }
            
            dup2(main_out, STDOUT_FILENO);
            printf("Game over!\n");
            break;
        }
        
        printf("Bad command. Type 'h' for help.\n");
    }
    
    return 0;
}