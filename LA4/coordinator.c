#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "boardgen.c"

#define PIPE_READ 0
#define PIPE_WRITE 1

// Structure to store pipe information for each block
typedef struct {
    int block_num;
    int pipe_fd[2];  // pipe for communication with coordinator
    int row_neighbors[2];  // write ends of row neighbor pipes
    int col_neighbors[2];  // write ends of column neighbor pipes
} BlockInfo;

// Get row neighbors for a block
void get_row_neighbors(int block, int *n1, int *n2) {
    int row = block / 3;
    int start = row * 3;
    *n1 = (block + 1) % 3 + start;
    *n2 = (block + 2) % 3 + start;
}

// Get column neighbors for a block
void get_col_neighbors(int block, int *n1, int *n2) {
    int col = block % 3;
    *n1 = (block + 3) % 9;
    if (*n1 / 3 == block / 3) *n1 = (block + 6) % 9;
    *n2 = (block + 6) % 9;
    if (*n2 / 3 == block / 3) *n2 = (block + 3) % 9;
}

void print_help() {
    printf("\nFoodoku Commands:\n");
    printf("h - Show this help message\n");
    printf("n - Start new puzzle\n");
    printf("p b c d - Place digit d in cell c of block b\n");
    printf("s - Show solution\n");
    printf("q - Quit game\n");
    printf("\nBlocks and cells are numbered 0-8 in row-major order\n\n");
}

int main() {
    int pipes[9][2];  // Array of pipes for each block
    BlockInfo blocks[9];
    pid_t pids[9];
    int A[9][9], S[9][9];  // Puzzle and solution arrays
    char cmd;
    int running = 1;

    // Create pipes for each block
    for (int i = 0; i < 9; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe creation failed");
            exit(1);
        }
    }

    // Initialize block information
    for (int i = 0; i < 9; i++) {
        blocks[i].block_num = i;
        blocks[i].pipe_fd[0] = pipes[i][0];
        blocks[i].pipe_fd[1] = pipes[i][1];
        
        // Get row and column neighbors
        int rn1, rn2, cn1, cn2;
        get_row_neighbors(i, &rn1, &rn2);
        get_col_neighbors(i, &cn1, &cn2);
        
        blocks[i].row_neighbors[0] = pipes[rn1][1];
        blocks[i].row_neighbors[1] = pipes[rn2][1];
        blocks[i].col_neighbors[0] = pipes[cn1][1];
        blocks[i].col_neighbors[1] = pipes[cn2][1];
    }

    // Fork child processes for each block
    for (int i = 0; i < 9; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) {  // Child process
            // Convert pipe file descriptors to strings
            char block_num[8], fd_in[8], fd_out[8];
            char rn1[8], rn2[8], cn1[8], cn2[8];
            
            sprintf(block_num, "%d", i);
            sprintf(fd_in, "%d", blocks[i].pipe_fd[0]);
            sprintf(fd_out, "%d", blocks[i].pipe_fd[1]);
            sprintf(rn1, "%d", blocks[i].row_neighbors[0]);
            sprintf(rn2, "%d", blocks[i].row_neighbors[1]);
            sprintf(cn1, "%d", blocks[i].col_neighbors[0]);
            sprintf(cn2, "%d", blocks[i].col_neighbors[1]);

            // Calculate xterm position
            int row = i / 3;
            int col = i % 3;
            char geometry[32];
            sprintf(geometry, "17x8+%d+%d", 700 + col * 210, 100 + row * 220);
            for (int j = 0; j < 9; j++) {
                if (j != i) {
                    close(pipes[j][0]);
                }
                if (j != i && j != blocks[i].row_neighbors[0]/2 && 
                    j != blocks[i].row_neighbors[1]/2 && 
                    j != blocks[i].col_neighbors[0]/2 && 
                    j != blocks[i].col_neighbors[1]/2) {
                    close(pipes[j][1]);
                }
            }

            // Launch xterm with block process
            char title[16];
            sprintf(title, "Block %d", i);
            execlp("xterm", "xterm", 
                   "-T", title,
                   "-fa", "Monospace",
                   "-fs", "15",
                   "-geometry", geometry,
                   "-bg", "#fff",
                   "-e", "./block",
                   block_num, fd_in, fd_out,
                   rn1, rn2, cn1, cn2,
                   NULL);
            
            perror("execlp failed");
            exit(1);
        }
    }

    // Close read ends of pipes in parent
    for (int i = 0; i < 9; i++) {
        close(pipes[i][0]);
    }

    // Main game loop
    while (running) {
        printf("Enter command (h for help): ");
        scanf(" %c", &cmd);

        switch (cmd) {
            case 'h':
                print_help();
                break;

            case 'n': {
                newboard(A, S);
                // Send initial board state to each block
                for (int i = 0; i < 9; i++) {
                    dprintf(pipes[i][1], "n ");
                    int block_row = (i / 3) * 3;
                    int block_col = (i % 3) * 3;
                    for (int r = 0; r < 3; r++) {
                        for (int c = 0; c < 3; c++) {
                            dprintf(pipes[i][1], "%d ", A[block_row + r][block_col + c]);
                        }
                    }
                    dprintf(pipes[i][1], "\n");
                }
                break;
            }

            case 'p': {
                int b, c, d;
                scanf("%d %d %d", &b, &c, &d);
                if (b < 0 || b > 8 || c < 0 || c > 8 || d < 1 || d > 9) {
                    printf("Invalid input ranges\n");
                } else {
                    dprintf(pipes[b][1], "p %d %d\n", c, d);
                }
                break;
            }

            case 's': {
                // Send solution to each block
                for (int i = 0; i < 9; i++) {
                    dprintf(pipes[i][1], "n ");
                    int block_row = (i / 3) * 3;
                    int block_col = (i % 3) * 3;
                    for (int r = 0; r < 3; r++) {
                        for (int c = 0; c < 3; c++) {
                            dprintf(pipes[i][1], "%d ", S[block_row + r][block_col + c]);
                        }
                    }
                    dprintf(pipes[i][1], "\n");
                }
                break;
            }

            case 'q':
                running = 0;
                // Send quit command to all blocks
                for (int i = 0; i < 9; i++) {
                    dprintf(pipes[i][1], "q\n");
                }
                // Wait for all children to exit
                for (int i = 0; i < 9; i++) {
                    wait(NULL);
                }
                break;

            default:
                printf("Invalid command. Use 'h' for help.\n");
        }
    }

    return 0;
}