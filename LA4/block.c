#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define GRID_DIM 3
#define TRUE 1
#define FALSE 0
#define EMPTY_CELL 0
#define DISPLAY_EMPTY "."

#define RED_TEXT "\033[31m"
#define BLUE_TEXT "\033[34m"
#define GREEN_TEXT "\033[32m"
#define RESET_COLOR "\033[0m"
#define CLEAR_SCREEN "\033[H\033[J"

#define HORIZONTAL_LINE "+---+---+---+"
#define VERTICAL_LINE "|"

static int stdout_backup;

// Function prototypes
void initialize_grid(int grid[GRID_DIM][GRID_DIM]);
void render_border(void);
int validate_position(int pos);
int check_conflicts_in_block(int value, int grid[GRID_DIM][GRID_DIM]);
void handle_pipe_communication(int output_fd, int result);
void display_temporary_message(const char* msg, int block_id, int current[GRID_DIM][GRID_DIM], int initial[GRID_DIM][GRID_DIM]);

void display_grid_state(int block_id, int current[GRID_DIM][GRID_DIM], int initial[GRID_DIM][GRID_DIM]) {
    dup2(stdout_backup, STDOUT_FILENO);
    printf(CLEAR_SCREEN);
    printf("Block %d:\n", block_id);
    render_border();
    
    for (int row = 0; row < GRID_DIM; row++) {
        printf(VERTICAL_LINE);
        for (int col = 0; col < GRID_DIM; col++) {
            switch(current[row][col]) {
                case EMPTY_CELL:
                    printf(" %s ", DISPLAY_EMPTY);
                    break;
                default:
                    switch(initial[row][col]) {
                        case EMPTY_CELL:
                            printf(" %s%d%s ", BLUE_TEXT, current[row][col], RESET_COLOR);
                            break;
                        default:
                            printf(" %s%d%s ", RED_TEXT, current[row][col], RESET_COLOR);
                            break;
                    }
                    break;
            }
            printf(VERTICAL_LINE);
        }
        printf("\n");
        if (row < GRID_DIM - 1) render_border();
    }
    render_border();
    fflush(stdout);
}

void display_solution_grid(int block_id, int initial[GRID_DIM][GRID_DIM], int solution[GRID_DIM][GRID_DIM]) {
    dup2(stdout_backup, STDOUT_FILENO);
    printf(CLEAR_SCREEN);
    printf("Block %d (Solution):\n", block_id);
    render_border();
    
    for (int row = 0; row < GRID_DIM; row++) {
        printf(VERTICAL_LINE);
        for (int col = 0; col < GRID_DIM; col++) {
            switch(initial[row][col]) {
                case EMPTY_CELL:
                    switch(solution[row][col]) {
                        case EMPTY_CELL:
                            printf(" %s ", DISPLAY_EMPTY);
                            break;
                        default:
                            printf(" %s%d%s ", GREEN_TEXT, solution[row][col], RESET_COLOR);
                            break;
                    }
                    break;
                default:
                    printf(" %s%d%s ", RED_TEXT, initial[row][col], RESET_COLOR);
                    break;
            }
            printf(VERTICAL_LINE);
        }
        printf("\n");
        if (row < GRID_DIM - 1) render_border();
    }
    render_border();
    fflush(stdout);
}

void render_border(void) {
    printf(HORIZONTAL_LINE"\n");
}

void initialize_grid(int grid[GRID_DIM][GRID_DIM]) {
    for (int i = 0; i < GRID_DIM; i++) {
        for (int j = 0; j < GRID_DIM; j++) {
            grid[i][j] = EMPTY_CELL;
        }
    }
}

int validate_position(int pos) {
    return (pos >= 0 && pos < GRID_DIM);
}

int check_conflicts_in_block(int value, int grid[GRID_DIM][GRID_DIM]) {
    for (int i = 0; i < GRID_DIM; i++) {
        for (int j = 0; j < GRID_DIM; j++) {
            if (grid[i][j] == value) return TRUE;
        }
    }
    return FALSE;
}

void handle_pipe_communication(int output_fd, int result) {
    int temp_stdout = dup(STDOUT_FILENO);
    dup2(output_fd, STDOUT_FILENO);
    printf("%d\n", result);
    fflush(stdout);
    dup2(temp_stdout, STDOUT_FILENO);
}

void display_temporary_message(const char* msg, int block_id, int current[GRID_DIM][GRID_DIM], int initial[GRID_DIM][GRID_DIM]) {
    dup2(stdout_backup, STDOUT_FILENO);
    printf("%s\n", msg);
    fflush(stdout);
    sleep(2);
    display_grid_state(block_id, current, initial);
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Usage: %s block_id input_fd output_fd row_check1_fd row_check2_fd col_check1_fd col_check2_fd\n", argv[0]);
        exit(1);
    }
    
    int block_id = atoi(argv[1]);
    int input_fd = atoi(argv[2]);
    int output_fd = atoi(argv[3]);
    int row_validator1_fd = atoi(argv[4]);
    int row_validator2_fd = atoi(argv[5]);
    int col_validator1_fd = atoi(argv[6]);
    int col_validator2_fd = atoi(argv[7]);
    
    int initial[GRID_DIM][GRID_DIM];
    int current[GRID_DIM][GRID_DIM];
    initialize_grid(initial);
    initialize_grid(current);
    
    stdout_backup = dup(STDOUT_FILENO);
    dup2(input_fd, STDIN_FILENO);
    
    char instruction;
    while (1) {
        if (scanf(" %c", &instruction) == EOF) break;
        
        if (instruction == 'n') {
            for (int i = 0; i < GRID_DIM; i++) {
                for (int j = 0; j < GRID_DIM; j++) {
                    scanf("%d", &initial[i][j]);
                    current[i][j] = initial[i][j];
                }
            }
            display_grid_state(block_id, current, initial);
        }
        else if (instruction == 'p') {
            int position, value;
            scanf("%d %d", &position, &value);
            int row = position / GRID_DIM;
            int col = position % GRID_DIM;
            
            if (initial[row][col] != EMPTY_CELL) {
                display_temporary_message("READ-ONLY CELL", block_id, current, initial);
                continue;
            }
            
            if (check_conflicts_in_block(value, current)) {
                display_temporary_message("BLOCK CONFLICT", block_id, current, initial);
                continue;
            }
            
            int temp_stdout = dup(STDOUT_FILENO);
            
            dup2(row_validator1_fd, STDOUT_FILENO);
            printf("r %d %d %d\n", row, value, output_fd);
            fflush(stdout);
            
            dup2(row_validator2_fd, STDOUT_FILENO);
            printf("r %d %d %d\n", row, value, output_fd);
            fflush(stdout);
            
            dup2(temp_stdout, STDOUT_FILENO);
            
            int response;
            scanf("%d", &response);
            if (response != FALSE) {
                display_temporary_message("ROW CONFLICT", block_id, current, initial);
                continue;
            }
            scanf("%d", &response);
            if (response != FALSE) {
                display_temporary_message("ROW CONFLICT", block_id, current, initial);
                continue;
            }
            
            dup2(col_validator1_fd, STDOUT_FILENO);
            printf("c %d %d %d\n", col, value, output_fd);
            fflush(stdout);
            
            dup2(col_validator2_fd, STDOUT_FILENO);
            printf("c %d %d %d\n", col, value, output_fd);
            fflush(stdout);
            
            dup2(temp_stdout, STDOUT_FILENO);
            
            scanf("%d", &response);
            if (response != FALSE) {
                display_temporary_message("COLUMN CONFLICT", block_id, current, initial);
                continue;
            }
            scanf("%d", &response);
            if (response != FALSE) {
                display_temporary_message("COLUMN CONFLICT", block_id, current, initial);
                continue;
            }
            
            current[row][col] = value;
            display_grid_state(block_id, current, initial);
        }
        else if (instruction == 'r') {
            int row, value, resp_fd;
            scanf("%d %d %d", &row, &value, &resp_fd);
            int has_conflict = FALSE;
            
            for (int j = 0; j < GRID_DIM; j++) {
                if (current[row][j] == value) {
                    has_conflict = TRUE;
                    break;
                }
            }
            handle_pipe_communication(resp_fd, has_conflict);
        }
        else if (instruction == 'c') {
            int col, value, resp_fd;
            scanf("%d %d %d", &col, &value, &resp_fd);
            int has_conflict = FALSE;
            
            for (int i = 0; i < GRID_DIM; i++) {
                if (current[i][col] == value) {
                    has_conflict = TRUE;
                    break;
                }
            }
            handle_pipe_communication(resp_fd, has_conflict);
        }
        else if (instruction == 'q') {
            dup2(stdout_backup, STDOUT_FILENO);
            printf("\nBYE #B%d...\n", block_id);
            fflush(stdout);
            sleep(1);
            exit(0);
        }
        else if (instruction == 's') {
            int solution[GRID_DIM][GRID_DIM];
            initialize_grid(solution);
            
            for (int i = 0; i < GRID_DIM; i++) {
                for (int j = 0; j < GRID_DIM; j++) {
                    scanf("%d", &initial[i][j]);
                }
            }
            for (int i = 0; i < GRID_DIM; i++) {
                for (int j = 0; j < GRID_DIM; j++) {
                    scanf("%d", &solution[i][j]);
                }
            }
            display_solution_grid(block_id, initial, solution);
        }
    }
    
    return 0;
}