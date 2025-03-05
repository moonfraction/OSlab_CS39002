#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAXFLR 100

int joinasfollower ( int *p )
{
   if (p[1] == p[0]) {
      fprintf(stderr, "follower error: %d followers have already joined\n", p[0]);
      return 0;
   }
   ++p[1];
   return p[1];
}

void loopwithothers ( int f, int *p )
{
   while (1) {
      while ((p[2] != f) && (p[2] != -f)) { }
      if (p[2] == f) {
         p[3+f] = 1 + rand() % 9;
         p[2] = (f == p[0]) ? 0 : f+1;
      } else {
         p[2] = (f == p[0]) ? 0 : -(f+1);
         printf("follower %d leaves\n", f);
         break;
      }
   }
}

void fmain ( )
{
   int f;
   key_t key = ftok("/home",'A');
   int shmid, *p;

   srand((unsigned int)getpid());
   shmid = shmget(key, (MAXFLR + 4) * sizeof(int), 0);
   if (shmid < 0) {
      fprintf(stderr, "follower error: unable to get ID of shared memory segment\n");
      exit(1);
   }
   p = (int *)shmat(shmid,NULL,0);

   if ((f = joinasfollower(p))) {
      printf("follower %d joins\n", f);
      loopwithothers(f,p);
   }

   shmdt(p);

   exit(0);
}

int main ( int argc, char *argv[] )
{
   int nf, i;

   nf = (argc > 1) ? atoi(argv[1]) : 1;

   for (i=0; i<nf; ++i) {
      if (!fork()) fmain();
      sleep(1);
   }
   for (i=0; i<nf; ++i) wait(NULL);
   exit(0);
}
