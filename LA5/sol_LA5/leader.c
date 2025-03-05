#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAXFLR 100

void waitforfollowers ( int n, int *p )
{
   p[0] = n;
   p[1] = 0;
   p[2] = 0;

   while (p[1] < n) { }
}

void loopwithfollowers ( int n, int *p )
{
   int maxsize, i, *H, sum;

   maxsize = 100 + 9*n;
   H = (int *)malloc(maxsize * sizeof(int));
   for (i=0; i<maxsize; ++i) H[i] = 0;
   while (1) {
      sum = p[3] = 1 + rand() % 99;
      p[2] = 1;
      while (p[2] != 0) { }
      printf("\t%2d", p[3]);
      for (i=1; i<=n; ++i) {
         sum += p[3+i];
         printf(" + %d", p[3+i]);
      }
      printf(" = %3d\n", sum);
      if (H[sum]) break;
      H[sum] = 1;
   }
   free(H);
}

void endloop ( int n, int *p )
{
   p[2] = -1;
   while (p[2] != 0) { }
}

int main ( int argc, char *argv[] )
{
   int n;
   key_t key = ftok("/home",'A');
   int shmid, *p;

   srand((unsigned int)getpid());
   n = (argc > 1) ? atoi(argv[1]) : 10;
   if (n > MAXFLR) {
      fprintf(stderr, "leader error: number of followers cannot exceed %d\n", MAXFLR);
      exit(1);
   }
   shmid = shmget(key, (MAXFLR + 4) * sizeof(int), 0777 | IPC_CREAT | IPC_EXCL);
   if (shmid < 0) {
      fprintf(stderr, "leader error: unable to create shared memory segment\n");
      exit(2);
   }
   p = (int *)shmat(shmid,NULL,0);

   waitforfollowers(n,p);

   loopwithfollowers(n,p);

   endloop(n,p);

   shmdt(p);
   shmctl(shmid, IPC_RMID, NULL);
   exit(0);
}
