#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define time M[0]
#define no_of_empty_tables M[1]
#define next_waiter M[2]
#define order_pending M[3]
#define cookfront M[1100]
#define cookback M[1101]

int shmid, cook, waiter, customer, mutex;
int *M;

void genresources ( int flag )
{
   key_t key;

   key = ftok("/home", 'M');
   shmid = shmget(key, 4096 * sizeof(int), flag);
   M = (int *)shmat(shmid,NULL,0);

   key = ftok("/home", 'K');
   cook = semget(key, 1, flag);

   key = ftok("/home", 'W');
   waiter = semget(key, 5, flag);

   key = ftok("/home", 'C');
   customer = semget(key, 256, flag);

   key = ftok("/home", 'X');
   mutex = semget(key, 1, flag);
}

void newsemshm ( )
{
   int i, offset;

   genresources(0777 | IPC_CREAT | IPC_EXCL);
   semctl(cook, 0, SETVAL, 0);
   for (i=0; i<5; ++i) semctl(waiter, i, SETVAL, 0);
   for (i=0; i<256; ++i) semctl(customer, i, SETVAL, 0);
   semctl(mutex, 0, SETVAL, 1);

   /* printf("Semaphore values:\n");
   printf("mutex: %d\n", semctl(mutex,0,GETVAL));
   printf("cook: %d\n", semctl(cook,0,GETVAL));
   printf("waiter: "); for (i=0; i<5; ++i) printf("%d,", semctl(waiter,i,GETVAL)); printf("\n");
   printf("customer: "); for (i=0; i<256; ++i) printf("%d,", semctl(customer,i,GETVAL)); printf("\n"); */

   /* Initialize shared memory */
   time = 0;
   no_of_empty_tables = 10;
   next_waiter = 0;
   order_pending = 0;

   /* Initialize shared memory for the waiters */
   for (i=0; i<5; ++i) {
      offset = 100 + 200 * i;
      M[offset] = 0;             /* No food_ready request */
      M[offset+1] = 0;           /* No new_customer request */
      M[offset+2] = offset + 4;  /* front of customer queue */
      M[offset+3] = offset + 2;  /* back of customer queue */
   }

   /* Initialize cooking queue to empty */
   cookfront = 1102;
   cookback = 1099;
}

void getsemshm ( )
{
   genresources(0777);
   /* printf("shmid = %d, cook = %d, waiter = %d, customer = %d, mutex = %d\n",
              shmid, cook, waiter, customer, mutex); */
}

void delsemshm ( )
{
   shmdt(M);
   shmctl(shmid, IPC_RMID, NULL);
   semctl(cook, 0, IPC_RMID, NULL);
   semctl(waiter, 0, IPC_RMID, NULL);
   semctl(customer, 0, IPC_RMID, NULL);
   semctl(mutex, 0, IPC_RMID, NULL);
}

void P ( int semid, int semno )
{
   struct sembuf pbuf;

   pbuf.sem_num = semno;
   pbuf.sem_op = -1;
   pbuf.sem_flg = 0;
   semop(semid, &pbuf, 1);
}

void V ( int semid, int semno )
{
   struct sembuf vbuf;

   vbuf.sem_num = semno;
   vbuf.sem_op = 1;
   vbuf.sem_flg = 0;
   semop(semid, &vbuf, 1);
}

void mntwait ( int m )
{
   usleep(m * 100000);
   // sleep(m);
}

void settime ( int t )
{
   if (time <= t) time = t;
   else printf("*** time = %d cannot be set to %d\n", time, t);
}

void prntime ( int m )
{
   int hr, mn;
   char ap = 'a';

   mn = m % 60;
   hr = 11 + (m / 60);
   if (hr >= 12) ap = 'p';
   if (hr > 12) hr -= 12;
   printf("[%02d:%02d %s] ", hr, mn, (ap == 'a') ? "am" : "pm");
}
