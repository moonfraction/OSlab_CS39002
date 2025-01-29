#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "boardgen.c"

#define BLOCK_COUNT 9
#define BLOCK_SIZE 3
#define BOARD_SIZE 9

// Structure to store pipe information for each block
typedef struct {
    int read_fd;
    int write_fd;
} PipePair;

// Function to get row neighbors for a block
void get_row_neighbors(int block, int *n1, int *n2) {
    int row = block / 3;
    int base = row * 3;
    *n1 = (block + 1) % 3 + base;
    *n2 = (block + 2) % 3 + base;
}

// Function to get column neighbors for a block
void get_column_neighbors(int block, int *n1, int *n2) {
    int col = block % 3;
    *n1 = (block + 3) % 9;
    *n2 = (block + 6) % 9;
    if (*n1 % 3 != col) *n1 = (*n1 / 3) * 3 + col;
    if (*n2 % 3 != col) *n2 = (*n2 / 3) * 3 + col;
}

// Function to find xterm executable
char* find_xterm() {
    // Common paths for xterm
    const char* paths[] = {
        "/usr/bin/xterm",
        "/bin/xterm",
        "/usr/local/bin/xterm",
        "xterm"  // Will use PATH environment
    };
    
    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
        if (access(paths[i], X_OK) == 0 || strcmp(paths[i], "xterm") == 0) {
            return strdup(paths[i]);
        }
    }
    return NULL;
}

// Function to launch xterm for a block
void launch_block(int block_num, PipePair *pipes, int *neighbor_write_fds) {
    char block_str[4], read_fd[8], write_fd[8];
    char n1_fd[8], n2_fd[8], n3_fd[8], n4_fd[8];
    
    // Calculate window position
    char geometry[32];
    int x = (block_num % 3) * 210 + 700;
    int y = (block_num / 3) * 220 + 100;
    sprintf(geometry, "17x8+%d+%d", x, y);
    
    // Convert all numbers to strings
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

// Function to print help message
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
    
    // Create pipes for each block
    for (int i = 0; i < BLOCK_COUNT; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe creation failed");
            exit(1);
        }
        pipes[i].read_fd = pipefd[0];
        pipes[i].write_fd = pipefd[1];
    }
    
    // Fork children for each block
    for (int block = 0; block < BLOCK_COUNT; block++) {
        pid_t pid = fork();
        
        if (pid == 0) {  // Child process
            // Get neighbor blocks
            int row_n1, row_n2, col_n1, col_n2;
            get_row_neighbors(block, &row_n1, &row_n2);
            get_column_neighbors(block, &col_n1, &col_n2);
            
            // Prepare neighbor write FDs
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
        } else if (pid > 0) {  // Parent process
            child_pids[block] = pid;
        } else {
            perror("fork failed");
            exit(1);
        }
    }
    
    // Main game loop
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
                // Send initial board state to each block
                for (int block = 0; block < BLOCK_COUNT; block++) {
                    dprintf(pipes[block].write_fd, "n ");
                    int row_start = (block / 3) * 3;
                    int col_start = (block % 3) * 3;
                    for (int i = 0; i < 3; i++) {
                        for (int j = 0; j < 3; j++) {
                            dprintf(pipes[block].write_fd, "%d ", 
                                    A[row_start + i][col_start + j]);
                        }
                    }
                    dprintf(pipes[block].write_fd, "\n");
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
                dprintf(pipes[block].write_fd, "p %d %d\n", cell, digit);
                break;
            }
                
            case 's': {
                // Send solution to each block
                for (int block = 0; block < BLOCK_COUNT; block++) {
                    dprintf(pipes[block].write_fd, "n ");
                    int row_start = (block / 3) * 3;
                    int col_start = (block % 3) * 3;
                    for (int i = 0; i < 3; i++) {
                        for (int j = 0; j < 3; j++) {
                            dprintf(pipes[block].write_fd, "%d ", 
                                    S[row_start + i][col_start + j]);
                        }
                    }
                    dprintf(pipes[block].write_fd, "\n");
                }
                break;
            }
                
            case 'q':
                // Send quit command to all blocks
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    dprintf(pipes[i].write_fd, "q\n");
                }
                
                // Wait for all children to exit
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    waitpid(child_pids[i], NULL, 0);
                }
                
                printf("Game over. Goodbye!\n");
                exit(0);
                break;
                
            default:
                printf("Invalid command. Use 'h' for help.\n");
        }
    }
    
    return 0;
}