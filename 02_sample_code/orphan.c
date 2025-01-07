#include <stdio.h>
#include <unistd.h>

int main(){
    int pid = fork();
    if(pid == 0){   
        printf("Child process: PID = %d, PPID = %d\n", getpid(), getppid());
        sleep(10);
        printf("Child process: PID = %d, PPID = %d\n", getpid(), getppid());
    }
    else{
        printf("Parent process: PID = %d, PPID = %d\n", getpid(), getppid());
    }
}