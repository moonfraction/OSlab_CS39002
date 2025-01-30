#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "boardgen.c"
#include <stdarg.h>

#define BLOCK_COUNT 9
#define BLOCK_SIZE 3
#define BOARD_SIZE 9

typedef struct {
    int read_fd;
    int write_fd;
} PipePair;

// original stdout for later use
static int original_stdout;

void write_to_pipe(int fd, const char *format, ...) {
    va_list args;
    int saved_stdout;
    
    // Save current stdout
    saved_stdout = dup(STDOUT_FILENO);
    
    // Redirect stdout to pipe
    dup2(fd, STDOUT_FILENO);
    
    // Write to pipe using printf
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    
    // Restore original stdout
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
}

// Rest of the helper functions remain the same
void get_row_neighbors(int block, int *n1, int *n2) {
    int row = block / 3;
    int base = row * 3;
    *n1 = (block + 1) % 3 + base;
    *n2 = (block + 2) % 3 + base;
}

void get_column_neighbors(int block, int *n1, int *n2) {
    int col = block % 3;
    *n1 = (block + 3) % 9;
    *n2 = (block + 6) % 9;
    if (*n1 % 3 != col) *n1 = (*n1 / 3) * 3 + col;
    if (*n2 % 3 != col) *n2 = (*n2 / 3) * 3 + col;
}

char* find_xterm() {
    const char* paths[] = {
        "/usr/bin/xterm",
        "/bin/xterm",
        "/usr/local/bin/xterm",
        "xterm"
    };
    
    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
        if (access(paths[i], X_OK) == 0 || strcmp(paths[i], "xterm") == 0) {
            return strdup(paths[i]);
        }
    }
    return NULL;
}

void launch_block(int block_num, PipePair *pipes, int *neighbor_write_fds) {
    char block_str[4], read_fd[8], write_fd[8];
    char n1_fd[8], n2_fd[8], n3_fd[8], n4_fd[8];
    
    int x = (block_num % 3) * 210 + 700;
    int y = (block_num / 3) * 220 + 100;
    char geometry[32];
    sprintf(geometry, "17x8+%d+%d", x, y);
    
    sprintf(block_str, "%d", block_num);
    sprintf(read_fd, "%d", pipes[block_num].read_fd);
    sprintf(write_fd, "%d", pipes[block_num].write_fd);
    sprintf(n1_fd, "%d", neighbor_write_fds[0]);
    sprintf(n2_fd, "%d", neighbor_write_fds[1]);
    sprintf(n3_fd, "%d", neighbor_write_fds[2]);
    sprintf(n4_fd, "%d", neighbor_write_fds[3]);
    
    char title[20];
    sprintf(title, "Block %d", block_num);

    char* xterm_path = find_xterm();
    if (xterm_path == NULL) {
        fprintf(stderr, "Could not find xterm executable\n");
        exit(1);
    }
    
    execl(xterm_path, "xterm",
          "-T", title,
          "-fa", "Monospace",
          "-fs", "15",
          "-geometry", geometry,
          "-bg", "white",
          "-e", "./block",
          block_str, read_fd, write_fd,
          n1_fd, n2_fd, n3_fd, n4_fd,
          NULL);
    
    free(xterm_path);
    perror("execl failed");
    exit(1);
}

void print_help() {
    printf("\nFoodoku Commands:\n");
    printf("h - Show this help message\n");
    printf("n - Start new game\n");
    printf("p b c d - Place digit d in cell c of block b\n");
    printf("s - Show solution\n");
    printf("q - Quit game\n\n");
    printf("Blocks and cells are numbered 0-8 in row-major order\n");
}

int main() {
    PipePair pipes[BLOCK_COUNT];
    pid_t child_pids[BLOCK_COUNT];
    int A[BOARD_SIZE][BOARD_SIZE];
    int S[BOARD_SIZE][BOARD_SIZE];
    char command;
    
    // Save original stdout
    original_stdout = dup(STDOUT_FILENO);
    
    // Create pipes
    for (int i = 0; i < BLOCK_COUNT; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe creation failed");
            exit(1);
        }
        pipes[i].read_fd = pipefd[0];
        pipes[i].write_fd = pipefd[1];
    }
    
    // Fork children
    for (int block = 0; block < BLOCK_COUNT; block++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            int row_n1, row_n2, col_n1, col_n2;
            get_row_neighbors(block, &row_n1, &row_n2);
            get_column_neighbors(block, &col_n1, &col_n2);
            
            int neighbor_write_fds[4] = {
                pipes[row_n1].write_fd,
                pipes[row_n2].write_fd,
                pipes[col_n1].write_fd,
                pipes[col_n2].write_fd
            };
            
            // Close unused pipe ends
            for (int i = 0; i < BLOCK_COUNT; i++) {
                if (i != block) {
                    close(pipes[i].read_fd);
                }
                if (!((i == row_n1) || (i == row_n2) || 
                      (i == col_n1) || (i == col_n2) || (i == block))) {
                    close(pipes[i].write_fd);
                }
            }
            
            launch_block(block, pipes, neighbor_write_fds);
        } else if (pid > 0) {
            child_pids[block] = pid;
        } else {
            perror("fork failed");
            exit(1);
        }
    }
    
    print_help();
    while (1) {
        printf("\nEnter command: ");
        scanf(" %c", &command);
        
        switch (command) {
            case 'h':
                print_help();
                break;
                
            case 'n': {
                newboard(A, S);
                for (int block = 0; block < BLOCK_COUNT; block++) {
                    write_to_pipe(pipes[block].write_fd, "n ");
                    int row_start = (block / 3) * 3;
                    int col_start = (block % 3) * 3;
                    for (int i = 0; i < 3; i++) {
                        for (int j = 0; j < 3; j++) {
                            write_to_pipe(pipes[block].write_fd, "%d ", 
                                    A[row_start + i][col_start + j]);
                        }
                    }
                    write_to_pipe(pipes[block].write_fd, "\n");
                }
                break;
            }
                
            case 'p': {
                int block, cell, digit;
                scanf("%d %d %d", &block, &cell, &digit);
                if (block < 0 || block >= 9 || cell < 0 || cell >= 9 || 
                    digit < 1 || digit > 9) {
                    printf("Invalid input values\n");
                    continue;
                }
                write_to_pipe(pipes[block].write_fd, "p %d %d\n", cell, digit);
                break;
            }
                
            case 's': {
                for (int block = 0; block < BLOCK_COUNT; block++) {
                    write_to_pipe(pipes[block].write_fd, "s ");
                    int row_start = (block / 3) * 3;
                    int col_start = (block % 3) * 3;
                    for (int i = 0; i < 3; i++) {
                        for (int j = 0; j < 3; j++) {
                            write_to_pipe(pipes[block].write_fd, "%d ", 
                                    A[row_start + i][col_start + j]);
                        }
                    }
                    for (int i = 0; i < 3; i++) {
                        for (int j = 0; j < 3; j++) {
                            write_to_pipe(pipes[block].write_fd, "%d ", 
                                    S[row_start + i][col_start + j]);
                        }
                    }
                    write_to_pipe(pipes[block].write_fd, "\n");
                }
                break;
            }
                
            case 'q':
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    write_to_pipe(pipes[i].write_fd, "q\n");
                }
                
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    waitpid(child_pids[i], NULL, 0);
                }
                
                // Restore original stdout before exiting
                dup2(original_stdout, STDOUT_FILENO);
                close(original_stdout);
                
                printf("Game over. Goodbye!\n");
                exit(0);
                break;
                
            default:
                printf("Invalid command. Use 'h' for help.\n");
        }
    }
    
    return 0;
}