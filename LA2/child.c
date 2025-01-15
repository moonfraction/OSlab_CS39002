#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define PLAYING     0
#define CATCHMADE   1
#define CATCHMISSED 2
#define OUTOFGAME   3

int n, my_index;
pid_t *child_pids;
int is_playing = 1;
int last_action = PLAYING;

void print_border() {
    printf("\n+----------------------------------------------------------------------------------------------+\n");
    fflush(stdout);
}


void handle_throw(int signo) {
    if (signo == SIGUSR2 && is_playing) {
        double prob = (double)rand() / RAND_MAX;
        if (prob < 0.8) {
            last_action = CATCHMADE;
            kill(getppid(), SIGUSR1);
        } else {
            last_action = CATCHMISSED;
            kill(getppid(), SIGUSR2);
        }
    } else if (signo == SIGUSR1) {
        if(my_index==0) printf("|\t");
        if (!is_playing && last_action != CATCHMISSED)
            printf("        ");  // OUTOFGAME
        else if (last_action == CATCHMADE)
            printf("CATCH   ");
        else if (last_action == CATCHMISSED)
            printf("MISS    ");
        else if (last_action == PLAYING)
            printf("....    ");
        fflush(stdout);
        if(my_index==n-1) printf("       |");
        
        if (last_action == CATCHMISSED) {
            is_playing = 0;
        }
            
        if (my_index < n - 1) {
            kill(child_pids[my_index + 1], SIGUSR1);
        } else {
            print_border();
            FILE *fp = fopen("dummycpid.txt", "r");
            pid_t dummy_pid;
            fscanf(fp, "%d", &dummy_pid);
            fclose(fp);
            kill(dummy_pid, SIGINT);
        }
        last_action = 0;  // Reset action after print
    }
    else if (signo == SIGINT) {
        if (is_playing && last_action == PLAYING) {
            printf("\n+++ Child %d: Yay! I am the winner!\n", my_index + 1);
            exit(0);
        }
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    my_index = atoi(argv[1]) - 1;
    unsigned int seed = getpid() + my_index + time(NULL);
    srand(seed);
    sleep(1);
    
    FILE *fp = fopen("childpid.txt", "r");
    fscanf(fp, "%d", &n);
    child_pids = malloc(n * sizeof(pid_t));
    for(int i = 0; i < n; i++) 
        fscanf(fp, "%d", &child_pids[i]);
    fclose(fp);
    
    signal(SIGUSR1, handle_throw);
    signal(SIGUSR2, handle_throw);
    signal(SIGINT, handle_throw);
    
    while(1) pause();
    return 0;
}