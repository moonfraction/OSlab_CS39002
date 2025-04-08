#include "resource.c"

void cmain ( char kno )
{
   int t, ct, cno, ccnt, wno;

   sleep(1);

   getsemshm();

   P(mutex,0);
   prntime(time);
   V(mutex,0);
   if (kno == 'C')
      printf("Cook %c is ready\n", kno);
   else
      printf("\tCook %c is ready\n", kno);

   while (1) {
      P(cook,0);
      P(mutex,0);
      if (cookfront > cookback) {
         prntime(time);
         V(mutex,0);
         printf("Cook %c: Why am I woken up?\n", kno);
      } else {
         t = time;
         wno = M[cookfront];
         cno = M[cookfront+1];
         ccnt = M[cookfront+2];
         ct = 5 * ccnt;
         cookfront += 3;
         V(mutex,0);

         prntime(t);
         if (kno == 'C')
            printf("Cook C: Preparing order (Waiter %c, Customer %d, Count %d)\n", 'U' + wno, cno, ccnt);
         else
            printf("\tCook D: Preparing order (Waiter %c, Customer %d, Count %d)\n", 'U' + wno, cno, ccnt);
         fflush(stdout);

         mntwait(ct);

         t += ct;
         P(mutex,0);
         settime(t);
         M[100 + 200 * wno] = cno;
         prntime(t);
         --order_pending;
         V(mutex,0);

         if (kno == 'C')
            printf("Cook C: Prepared order (Waiter %c, Customer %d, Count %d)\n", 'U' + wno, cno, ccnt);
         else
            printf("\tCook D: Prepared order (Waiter %c, Customer %d, Count %d)\n", 'U' + wno, cno, ccnt);
         fflush(stdout);

         V(waiter,wno);
      }

      P(mutex,0);
      if ( (time > 240) && (cookfront > cookback) ) {
         /* No more new orders, and no orders are pendning */
         if (order_pending == 0) {
            for (wno = 0; wno < 5; ++wno) V(waiter,wno);
         }
         prntime(time);
         printf("Cook %c: Leaving\n", kno);
         shmdt(M);
         V(mutex,0);
         exit(0);
      }
      V(mutex,0);
   }
}

int main ( )
{
   if (!fork()) cmain('C');
   if (!fork()) cmain('D');

   newsemshm();

   wait(NULL);
   wait(NULL);

   exit(0);
}
