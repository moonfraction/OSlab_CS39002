#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 3

// Store original stdout for block display
static int original_stdout;

void draw_block(int block_num, int board[BLOCK_SIZE][BLOCK_SIZE], int original[BLOCK_SIZE][BLOCK_SIZE]) {
    dup2(original_stdout, STDOUT_FILENO);
    
    printf("\033[H\033[J");
    printf("Block %d:\n", block_num);
    printf("+---+---+---+\n");
    for (int i = 0; i < BLOCK_SIZE; i++) {
        printf("|");
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (board[i][j] == 0) {
                printf(" . ");
            } else {
                if (original[i][j] != 0) {
                    printf(" \033[31m%d\033[0m ", board[i][j]);
                } else {
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

void draw_solution(int block_num, int original[BLOCK_SIZE][BLOCK_SIZE], int solution[BLOCK_SIZE][BLOCK_SIZE]) {
    dup2(original_stdout, STDOUT_FILENO);
    
    printf("\033[H\033[J");
    printf("Block %d (Solution):\n", block_num);
    printf("+---+---+---+\n");
    for (int i = 0; i < BLOCK_SIZE; i++) {
        printf("|");
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (original[i][j] != 0) {
                printf(" \033[31m%d\033[0m ", original[i][j]);
            } else if (solution[i][j] != 0) {
                printf(" \033[32m%d\033[0m ", solution[i][j]);
            } else {
                printf(" . ");
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

void show_error(const char *message, int block_num, int board[BLOCK_SIZE][BLOCK_SIZE], int original[BLOCK_SIZE][BLOCK_SIZE]) {
    dup2(original_stdout, STDOUT_FILENO);
    printf("%s\n", message);
    fflush(stdout);
    sleep(2);
    draw_block(block_num, board, original);
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Usage: %s block_num read_fd write_fd row_n1_fd row_n2_fd col_n1_fd col_n2_fd\n", argv[0]);
        exit(1);
    }
    
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
    
    // Save original stdout
    original_stdout = dup(STDOUT_FILENO);
    
    // Redirect stdin to read from pipe
    dup2(read_fd, STDIN_FILENO);
    
    char command;
    while (scanf(" %c", &command) != EOF) {
        switch (command) {
            case 'n': {
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
                
                if (original[row][col] != 0) {
                    show_error("Read-only cell", block_num, current, original);
                    continue;
                }
                
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
                int stdout_backup = dup(STDOUT_FILENO);
                
                dup2(row_n1_fd, STDOUT_FILENO);
                printf("r %d %d %d\n", row, digit, write_fd);
                fflush(stdout);
                
                dup2(row_n2_fd, STDOUT_FILENO);
                printf("r %d %d %d\n", row, digit, write_fd);
                fflush(stdout);
                
                dup2(stdout_backup, STDOUT_FILENO);
                
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
                dup2(col_n1_fd, STDOUT_FILENO);
                printf("c %d %d %d\n", col, digit, write_fd);
                fflush(stdout);
                
                dup2(col_n2_fd, STDOUT_FILENO);
                printf("c %d %d %d\n", col, digit, write_fd);
                fflush(stdout);
                
                dup2(stdout_backup, STDOUT_FILENO);
                
                scanf("%d", &response);
                if (response != 0) {
                    show_error("Column conflict", block_num, current, original);
                    continue;
                }
                scanf("%d", &response);
                if (response != 0) {
                    show_error("Column conflict", block_num, current, original);
                    continue;
                }
                
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
                int stdout_backup = dup(STDOUT_FILENO);
                dup2(resp_fd, STDOUT_FILENO);
                printf("%d\n", has_conflict);
                fflush(stdout);
                dup2(stdout_backup, STDOUT_FILENO);
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
                int stdout_backup = dup(STDOUT_FILENO);
                dup2(resp_fd, STDOUT_FILENO);
                printf("%d\n", has_conflict);
                fflush(stdout);
                dup2(stdout_backup, STDOUT_FILENO);
                break;
            }
                
            case 'q': {
                dup2(original_stdout, STDOUT_FILENO);
                printf("\nBye from B%d...\n", block_num);
                fflush(stdout);
                sleep(1);
                exit(0);
                break;
            }
                
            case 's': {
                int solution[BLOCK_SIZE][BLOCK_SIZE] = {{0}};
                for (int i = 0; i < BLOCK_SIZE; i++) {
                    for (int j = 0; j < BLOCK_SIZE; j++) {
                        scanf("%d", &original[i][j]);
                    }
                }
                for (int i = 0; i < BLOCK_SIZE; i++) {
                    for (int j = 0; j < BLOCK_SIZE; j++) {
                        scanf("%d", &solution[i][j]);
                    }
                }
                draw_solution(block_num, original, solution);
                break;
            }
        }
    }
    
    return 0;
}