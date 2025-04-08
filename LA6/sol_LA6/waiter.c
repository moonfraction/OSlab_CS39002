#include "resource.c"

void wmain ( int w )
{
   int K, C, F, B, S;
   int t, cno, ccnt, i;

   getsemshm();

   P(mutex,0);
   K = 100 + 200 * w;
   C = K + 1;
   F = K + 2;
   B = K + 3; 
   S = 0;
   prntime(time);
   V(mutex,0);

   for (i=0; i<w; ++i) printf("\t");
   printf("Waiter %c is ready\n", 'U' + w);

   while (1) {
      P(waiter,w);

      P(mutex,0);
      if (M[K]) {
         cno = M[K];
         t = time;
         M[K] = 0;
         V(mutex,0);

         V(customer,cno);
         --S;
         P(mutex,0);
         prntime(time);
         V(mutex,0);
         for (i=0; i<w; ++i) printf("\t");
         printf("Waiter %c: Serving food to Customer %d\n", 'U' + w, cno);
         fflush(stdout);
      } else if (M[C]) {
         if (M[F] > M[B]) {
            prntime(time);
            V(mutex,0);
            printf("\t\t\tWaiter %d: Where is the customer?\n", w);
            fflush(stdout);
         } else {
            cno = M[M[F]];
            ccnt = M[M[F]+1];
            M[F] += 2;
            t = time;
            --M[C];
            V(mutex,0);

            mntwait(1);

            t += 1;
            P(mutex,0);
            settime(t);
            V(customer,cno);
            cookback += 3;
            M[cookback] = w;
            M[cookback+1] = cno;
            M[cookback+2] = ccnt;
            ++order_pending;
            prntime(time);
            V(mutex,0);
            for (i=0; i<w; ++i) printf("\t");
            printf("Waiter %c: Placing order for Customer %d (count = %d)\n", 'U' + w, cno, ccnt);
            ++S;
            V(cook,0);
            fflush(stdout);
         }
      } else {
        V(mutex,0);
      }

      P(mutex,0);
      if ( (time > 240) && (M[F] > M[B]) && (S == 0) ) {
         prntime(time);
         V(mutex,0);
         shmdt(M);
         for (i=0; i<w; ++i) printf("\t");
         printf("Waiter %c leaving (no more customer to serve)\n", 'U' + w);
         fflush(stdout);
         exit(0);
      }
      V(mutex,0);
   }
}

int main ( )
{
   int i;

   for (i=0; i<5; ++i) {
      if (!fork()) wmain(i);
   }
   for (i=0; i<5; ++i) wait(NULL);
   exit(0);
}
