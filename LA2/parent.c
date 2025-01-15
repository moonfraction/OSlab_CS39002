#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

int n;
pid_t *child_pids;
int *playing;
int current_player = 0;
int players_left;
int received_signal = 0;

void handle_signal(int signo) {
    received_signal = signo;
}

void print_header() {
    printf("\t");
    for(int i = 1; i <= n; i++) printf("%-7d ", i);
    printf("\n+----------------------------------------------------------------------------------------------+");
    printf("\n");
}

int next_player() {
    do {
        current_player = (current_player + 1) % n;
    } while (!playing[current_player]);
    return current_player;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_children>\n", argv[0]);
        exit(1);
    }
    
    n = atoi(argv[1]);
    players_left = n;
    child_pids = malloc(n * sizeof(pid_t));
    playing = calloc(n, sizeof(int));
    
    printf("Parent: %d child processes created\n", n);
    
    FILE *fp = fopen("childpid.txt", "w");
    fprintf(fp, "%d\n", n);
    
    for(int i = 0; i < n; i++) {
        playing[i] = 1;
        if ((child_pids[i] = fork()) == 0) {
            char index[10];
            sprintf(index, "%d", i + 1);
            execl("./child", "child", index, NULL);
            exit(1);
        }
        fprintf(fp, "%d ", child_pids[i]);
    }
    // we have written the child pids to the file, now we can close it
    fclose(fp);
    
    printf("Parent: Waiting for child processes to read child database\n");
    sleep(2);
    
    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);
    
    print_header();
    
    while(players_left > 1) {
        pid_t dummy_pid = fork();
        if (dummy_pid == 0) {
            execl("./dummy", "dummy", NULL);
            exit(1);
        }
        
        fp = fopen("dummycpid.txt", "w");
        fprintf(fp, "%d", dummy_pid);
        fclose(fp);
        
        kill(child_pids[0], SIGUSR1);
        waitpid(dummy_pid, NULL, 0);
        
        kill(child_pids[current_player], SIGUSR2);
        
        received_signal = 0;
        while(!received_signal) pause();
        
        if(received_signal == SIGUSR2) {
            playing[current_player] = 0;
            players_left--;
        }
        current_player = next_player();
    }

    // Printing final status before ending game
    pid_t dummy_pid = fork();
    if (dummy_pid == 0) {
        execl("./dummy", "dummy", NULL);
        exit(1);
    }

    fp = fopen("dummycpid.txt", "w");
    fprintf(fp, "%d", dummy_pid);
    fclose(fp);

    // print_header();
    kill(child_pids[0], SIGUSR1);  // final print
    waitpid(dummy_pid, NULL, 0);

    print_header();

    
    for(int i = 0; i < n; i++)
        kill(child_pids[i], SIGINT);

    while(wait(NULL) > 0);

    free(child_pids);
    free(playing);
    return 0;
}