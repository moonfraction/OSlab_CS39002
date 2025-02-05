#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    srand(time(NULL));
    if(argc != 2 && argc != 1){
        printf("Usage: %s <n>\n or \n%s\n", argv[0], argv[0]);
        exit(1);
    }
    int n = 10;
    if(argc == 2){
        n = atoi(argv[1]);
    }
    if(n>100 || n<1){
        printf("n should be less than 100 and greater than 0\n");
        exit(1);
    }

    int key  = ftok("/", 1);
    int shmid = shmget(key, (n+4)*sizeof(int), 0777|IPC_CREAT|IPC_EXCL);

    if (shmid == -1) {
        printf("Error creating shared memory segment\n");
        exit(1);
    }

    int * M = shmat(shmid, NULL, 0);            // attach shared memory

    M[0] = n; // total followes
    M[1] = 0; // no of followers joined
    M[2] = 0; // turn
    M[3] = 0; // leader cell

    while(M[1] != n); // wait for all followers to join

    // followers -> 1-9
    // leader -> 1-99
    // sum always less than 1000
    int sums[2000] = {0}; 

    int flag = 0;
    while(flag == 0){
        // next loop
        int l_no = 1 + rand()%99;
        M[3] = l_no;

        M[2] = 1; // give turn to follower 1

        while(M[2] != 0); // wait for leader turn

        int cur = 0;
        for(int i=0; i<=n; i++) cur += M[3+i];
        if(sums[cur] == 1) flag = 1;

        for(int i=0; i<=n; i++){
            if(i != 0) printf(" + ");
            printf("%d", M[3+i]);
        }
        printf(" = %d\n", cur);
        
        if(flag == 1) break;
        sums[cur] = 1;
    }

    M[2] = -1;

    while(M[2] != 0); // wait till all followers exit

    shmdt(M);           // detach shared memory
    shmctl(shmid, IPC_RMID, NULL); // delete shared memory

    return 0;
}