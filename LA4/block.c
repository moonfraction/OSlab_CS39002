#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BLOCK_SIZE 3

void draw_block(int block[BLOCK_SIZE][BLOCK_SIZE]) {
    printf("\033[H\033[J");  // Clear screen
    printf("+---+---+---+\n");
    for (int i = 0; i < BLOCK_SIZE; i++) {
        printf("|");
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (block[i][j] == 0)
                printf(" . ");
            else
                printf(" %d ", block[i][j]);
            printf("|");
        }
        printf("\n");
        if (i < BLOCK_SIZE - 1)
            printf("+---+---+---+\n");
    }
    printf("+---+---+---+\n");
}

void print_error(const char *msg) {
    printf("\n%s\n", msg);
    fflush(stdout);
    sleep(2);
}

int check_row_conflict(int row[BLOCK_SIZE], int digit) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (row[i] == digit) return 1;
    }
    return 0;
}

int check_col_conflict(int block[BLOCK_SIZE][BLOCK_SIZE], int col, int digit) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (block[i][col] == digit) return 1;
    }
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Usage: %s block_num fd_in fd_out rn1 rn2 cn1 cn2\n", argv[0]);
        exit(1);
    }

    int block_num = atoi(argv[1]);
    int fd_in = atoi(argv[2]);
    int fd_out = atoi(argv[3]);
    int rn1_out = atoi(argv[4]);
    int rn2_out = atoi(argv[5]);
    int cn1_out = atoi(argv[6]);
    int cn2_out = atoi(argv[7]);

    // Redirect stdin to pipe input
    dup2(fd_in, STDIN_FILENO);
    close(fd_in);

    // Set pipes to be unbuffered
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    int A[BLOCK_SIZE][BLOCK_SIZE] = {{0}};  // Original puzzle
    int B[BLOCK_SIZE][BLOCK_SIZE] = {{0}};  // Current state

    char cmd;
    int running = 1;

    while (running && scanf(" %c", &cmd) == 1) {
        switch (cmd) {
            case 'n': {
                // Receive initial block state
                for (int i = 0; i < BLOCK_SIZE; i++) {
                    for (int j = 0; j < BLOCK_SIZE; j++) {
                        scanf("%d", &A[i][j]);
                        B[i][j] = A[i][j];
                    }
                }
                draw_block(B);
                break;
            }

            case 'p': {
                int cell, digit;
                scanf("%d %d", &cell, &digit);
                int row = cell / 3;
                int col = cell % 3;

                // Check if trying to modify original puzzle cell
                if (A[row][col] != 0) {
                    print_error("Read-only cell");
                    draw_block(B);
                    break;
                }

                // Check block conflict
                if (check_row_conflict(B[row], digit) || 
                    check_col_conflict(B, col, digit)) {
                    print_error("Block conflict");
                    draw_block(B);
                    break;
                }

                // Check row conflicts with neighbors
                int row_global = (block_num / 3) * 3 + row;
                dprintf(rn1_out, "r %d %d\n", row, digit);
                fsync(rn1_out);  // Force flush
                dprintf(rn2_out, "r %d %d\n", row, digit);
                fsync(rn2_out);  // Force flush
                
                int response;
                scanf("%d", &response);
                if (response) {
                    print_error("Row conflict");
                    draw_block(B);
                    break;
                }
                scanf("%d", &response);
                if (response) {
                    print_error("Row conflict");
                    draw_block(B);
                    break;
                }

                // Check column conflicts with neighbors
                int col_global = (block_num % 3) * 3 + col;
                dprintf(cn1_out, "c %d %d\n", col, digit);
                fsync(cn1_out);  // Force flush
                dprintf(cn2_out, "c %d %d\n", col, digit);
                fsync(cn2_out);  // Force flush
                
                scanf("%d", &response);
                if (response) {
                    print_error("Column conflict");
                    draw_block(B);
                    break;
                }
                scanf("%d", &response);
                if (response) {
                    print_error("Column conflict");
                    draw_block(B);
                    break;
                }

                // If no conflicts, update the cell
                B[row][col] = digit;
                draw_block(B);
                break;
            }

            case 'r': {
                int row, digit;
                scanf("%d %d", &row, &digit);
                int conflict = check_row_conflict(B[row], digit);
                dprintf(fd_out, "%d\n", conflict);
                break;
            }

            case 'c': {
                int col, digit;
                scanf("%d %d", &col, &digit);
                int conflict = check_col_conflict(B, col, digit);
                dprintf(fd_out, "%d\n", conflict);
                break;
            }

            case 'q':
                print_error("Bye...");
                running = 0;
                break;
        }
    }

    return 0;
}