/*
	                 PROCESS FORKING 		

  This program illustrates the fork() system call.

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>  
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

void print_with_time(const char* message, int x) {
    time_t now;
    struct tm *local;
    pid_t pid = getpid();
    time(&now);
    local = localtime(&now);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    printf("%s: x= %d \tPID= %d \tat %02d:%02d:%03ld\n", message, x, pid, local->tm_min, local->tm_sec, ts.tv_nsec / 1000000);
}

int main()
{
	int i ;
	int x = 10 ;
	int pid1, pid2, status ;

	print_with_time("Before forking, the value of x is", x);

	/*
	   After forking, we make the parent and its two children
           increment x in different ways to illustrate that they
	   have different copies of x
	*/
	if ((pid1 = fork()) == 0) {

		/* First child process */
		for (i=0 ; i < 5; i++) {
		   print_with_time("\t\t\t At first child", x);
		   x= x+10;
		   sleep(1) ; /* Sleep for 1 second */
		}
		exit(0); // Ensure the child exits after completing its task
	}
	else {

		/* Parent process */

		/* Create another child process */
		if ((pid2 = fork()) == 0) {

		    /* Second child process */
                    for (i=0 ; i < 5; i++) {
                   	print_with_time("\t\t\t\t\t\t At second child", x);
                   	x= x+20;
			sleep(1) ; /* Sleep for 1 second */
                    }
		    exit(0); // Ensure the child exits after completing its task
		}
		else {

			/* Parent process */
			for (i=0 ; i < 5; i++) {
		  	 	print_with_time("At parent", x);
		   		x= x+5;
				sleep(1) ; /* Sleep for 1 second */
			}

			/* 
			    The waitpid() system call causes the parent
			    to wait for a child process with a specific pid to complete
			    its execution. The input parameter can
			    specify the PID of the child process for
			    which it has to wait.
			*/

			if (waitpid(pid1, &status, 0) > 0) {
			    if (WIFEXITED(status)) {
			        printf("First child exited with status: %d\n", WEXITSTATUS(status));
			    } else {
			        printf("First child did not exit successfully.\n");
			    }
			}

			if (waitpid(pid2, &status, 0) > 0) {
			    if (WIFEXITED(status)) {
			        printf("Second child exited with status: %d\n", WEXITSTATUS(status));
			    } else {
			        printf("Second child did not exit successfully.\n");
			    }
			}
		}
	}

	print_with_time("After forking, the value of x is", x);

	return 0;
}