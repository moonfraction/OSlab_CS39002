#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NTHREADS 2
#define N 1000
#define MEGEXTRA 10000

pthread_attr_t attr;

void *dowork(void *threadid)
{
   char A[N][N];
   int i, j;
   long tid;
   size_t mystacksize;
   
   tid = (long)threadid;
   printf("Thread %ld starting\n", tid);
   pthread_attr_getstacksize(&attr, &mystacksize);
   printf("Thread %ld: stack size = %li bytes \n", tid, mystacksize);
   for (i = 0; i < N; i++) {
      for (j = 0; j < N; j++) {
         A[i][j] = 'a' + i+j;
      }
   }
   pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
   pthread_t threads[NTHREADS];
   size_t stacksize;
   int rc;
   long t;

   pthread_attr_init(&attr);
   pthread_attr_getstacksize(&attr, &stacksize);
   printf("Default stack size = %li\n", stacksize);

//    stacksize = sizeof(char)*N*N+MEGEXTRA;
   printf("Amount of stack needed per thread = %li\n", stacksize);
   pthread_attr_setstacksize (&attr, stacksize);

   printf("Creating threads with stack size = %li bytes\n", stacksize);
   for(t=0; t<NTHREADS; t++){
      rc = pthread_create(&threads[t], &attr, dowork, (void *)t);
      printf("Created thread %ld: %d\n", t, rc);
      if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
      }
   }
   printf("Created %ld threads.\n", t);
   pthread_exit(NULL);
}