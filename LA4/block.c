#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 3

// Function to draw the 3x3 block
void draw_block(int block_num, int board[BLOCK_SIZE][BLOCK_SIZE], int original[BLOCK_SIZE][BLOCK_SIZE]) {
    printf("\033[H\033[J");  // Clear screen
    printf("Block %d:\n", block_num);
    printf("+---+---+---+\n");
    for (int i = 0; i < BLOCK_SIZE; i++) {
        printf("|");
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (board[i][j] == 0) {
                printf(" . ");
            } else {
                if (original[i][j] != 0) {
                    // Red color for original numbers
                    printf(" \033[31m%d\033[0m ", board[i][j]);
                } else {
                    // Default color for player-placed numbers
                    printf(" \033[34m%d\033[0m ", board[i][j]);
                }
            }
            printf("|");
        }
        printf("\n");
        if (i < BLOCK_SIZE - 1)
            printf("+---+---+---+\n");
    }
    printf("+---+---+---+\n");
    fflush(stdout);
}

// Function to print error message and redraw after delay
void show_error(const char *message, int block_num, int board[BLOCK_SIZE][BLOCK_SIZE], int original[BLOCK_SIZE][BLOCK_SIZE]) {
    printf("%s\n", message);
    fflush(stdout);
    sleep(2);
    draw_block(block_num, board, original);
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Usage: %s block_num read_fd write_fd "
                "row_n1_fd row_n2_fd col_n1_fd col_n2_fd\n", argv[0]);
        exit(1);
    }
    
    // getting the block number and file descriptors
    int block_num = atoi(argv[1]);
    int read_fd = atoi(argv[2]);
    int write_fd = atoi(argv[3]);
    int row_n1_fd = atoi(argv[4]);
    int row_n2_fd = atoi(argv[5]);
    int col_n1_fd = atoi(argv[6]);
    int col_n2_fd = atoi(argv[7]);
    
    // Arrays to store original puzzle and current state
    int original[BLOCK_SIZE][BLOCK_SIZE] = {{0}};
    int current[BLOCK_SIZE][BLOCK_SIZE] = {{0}};

    // print block ready message
    printf("Block %d ready\n", block_num);

    // man dup2
    // In dup2(), the value of the new descriptor fildes2 is specified.  If fildes and fildes2 are
    //  equal, then dup2() just returns fildes2; no other changes are made to the existing
    //  descriptor.  Otherwise, if descriptor fildes2 is already in use, it is first deallocated as
    //  if a close(2) call had been done first.

    if (dup2(read_fd, STDIN_FILENO) == -1) { // redirecting the read_fd to stdin
        perror("dup2 failed");
        exit(1);
    }
    close(read_fd);
    
    char command;
    while (scanf(" %c", &command) != EOF) {
        switch (command) {
            case 'n': {
                // Read new board state
                for (int i = 0; i < BLOCK_SIZE; i++) {
                    for (int j = 0; j < BLOCK_SIZE; j++) {
                        scanf("%d", &original[i][j]);
                        current[i][j] = original[i][j];
                    }
                }
                draw_block(block_num, current, original);
                break;
            }
                
            case 'p': {
                int cell, digit;
                scanf("%d %d", &cell, &digit);
                int row = cell / 3;
                int col = cell % 3;
                
                // Check if cell is in original puzzle
                if (original[row][col] != 0) {
                    show_error("Read-only cell", block_num, current, original);
                    continue;
                }
                
                // Check for block conflicts
                int has_conflict = 0;
                for (int i = 0; i < BLOCK_SIZE && !has_conflict; i++) {
                    for (int j = 0; j < BLOCK_SIZE; j++) {
                        if (current[i][j] == digit) {
                            has_conflict = 1;
                            break;
                        }
                    }
                }
                if (has_conflict) {
                    show_error("Block conflict", block_num, current, original);
                    continue;
                }
                
                // Check row conflicts
                dprintf(row_n1_fd, "r %d %d %d\n", row, digit, write_fd);
                dprintf(row_n2_fd, "r %d %d %d\n", row, digit, write_fd);
                int response;
                scanf("%d", &response);
                if (response != 0) {
                    show_error("Row conflict", block_num, current, original);
                    continue;
                }
                scanf("%d", &response);
                if (response != 0) {
                    show_error("Row conflict", block_num, current, original);
                    continue;
                }
                
                // Check column conflicts
                dprintf(col_n1_fd, "c %d %d %d\n", col, digit, write_fd);
                dprintf(col_n2_fd, "c %d %d %d\n", col, digit, write_fd);
                scanf("%d", &response);
                if (response != 0) {
                    show_error("Column conflict", block_num, current, original);;
                    continue;
                }
                scanf("%d", &response);
                if (response != 0) {
                    show_error("Column conflict", block_num, current, original);;
                    continue;
                }
                
                // If no conflicts, update the cell
                current[row][col] = digit;
                draw_block(block_num, current, original);
                break;
            }
                
            case 'r': {
                int row, digit, resp_fd;
                scanf("%d %d %d", &row, &digit, &resp_fd);
                int has_conflict = 0;
                for (int j = 0; j < BLOCK_SIZE; j++) {
                    if (current[row][j] == digit) {
                        has_conflict = 1;
                        break;
                    }
                }
                dprintf(resp_fd, "%d\n", has_conflict);
                break;
            }
                
            case 'c': {
                int col, digit, resp_fd;
                scanf("%d %d %d", &col, &digit, &resp_fd);
                int has_conflict = 0;
                for (int i = 0; i < BLOCK_SIZE; i++) {
                    if (current[i][col] == digit) {
                        has_conflict = 1;
                        break;
                    }
                }
                dprintf(resp_fd, "%d\n", has_conflict);
                break;
            }
                
            case 'q': {
                printf("\nBye B%d...\n", block_num);
                fflush(stdout);
                sleep(2);
                // Close all pipe file descriptors
                close(write_fd);
                close(row_n1_fd);
                close(row_n2_fd);
                close(col_n1_fd);
                close(col_n2_fd);
                exit(0);
                break;
            }
                
            // default:
            //     fprintf(stderr, "Block %d: Unknown command '%c'\n", block_num, command);
        }
    }
    
    return 0;
}