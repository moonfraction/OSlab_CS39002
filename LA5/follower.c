#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]){
    if(argc != 2 && argc != 1){
        printf("Usage: %s <n>\n or \n%s\n", argv[0], argv[0]);
        exit(1);
    }

    // number of followers wanting to join
    int nf = 1;
    if(argc == 2){
        nf = atoi(argv[1]);
    }
    if(nf<1){
        printf("n should be greater than 0\n");
    }

    int key  = ftok("/", 1);

    for(int i=1; i<=nf; i++){
        int pid = fork();
        if(pid == 0){
            // follower
            srand(getpid() * time(NULL));

            int shmid = shmget(key, 0, 0777);
            if (shmid == -1) {
                printf("Error getting shared memory segment\n");
                exit(1);
            }
            
            int * M = shmat(shmid, NULL, 0);            // attach shared memory
            if(M == (void *)-1){
                printf("Error attaching shared memory segment\n");
                exit(1);
            }
            int n = M[0];

            if(M[1] == n){
                printf("follower error: %d followers already joined\n", n);
                shmdt(M);
                exit(0);
            }

            int f_no = ++M[1];
            printf("follower %d joins\n", f_no);

            while(1){
                if(M[2] == f_no){
                    M[3+f_no] = 1 + rand()%9;
                    M[2] = (M[2]+1)%(n+1);
                }
                else if(M[2] == -f_no) {
                    printf("follower %d leaves\n", f_no);
                    if(M[2] == -n) M[2] = 0;
                    else M[2] = M[2] - 1;
                    shmdt(M);
                    if(M[2] == -n) break;
                }
                usleep(1000);
            }
        }
    }

    for(int i=1; i<=nf; i++){
        wait(NULL);
    }

    return 0;
}