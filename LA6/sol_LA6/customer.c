#include "resource.c"

void cmain ( int cno, int t, int ccnt )
{
   int w, offset, B, tarr;

   if (t > 240) {
      prntime(t);
      printf("\t\t\t\t\t\tCustomer %d leaves (late arrival)\n", cno);
      exit(1);
   }

   getsemshm();

   P(mutex,0);
   if (no_of_empty_tables == 0) {
      V(mutex,0);
      prntime(t);
      printf("\t\t\t\t\t\tCustomer %d leaves (no empty table)\n", cno);
      exit(2);
   }
   --no_of_empty_tables;
   w = next_waiter;
   next_waiter = (w + 1) % 5;
   offset = 100 + 200 * w;
   ++M[offset + 1];
   B = (M[offset + 3] += 2);
   M[B] = cno;
   M[B+1] = ccnt;
   settime(t);
   V(mutex,0);

   tarr = t;
   prntime(t);
   printf("Customer %d arrives (count = %d)\n", cno, ccnt);
   fflush(stdout);

   V(waiter,w);

   P(customer,cno); /* Wait for the waiter to take order */

   P(mutex,0);
   prntime(time);
   V(mutex,0);
   printf("\tCustomer %d: Order placed to Waiter %c\n", cno, 'U' + w);
   fflush(stdout);

   P(customer,cno); /* Wait for the waiter to serve order */

   P(mutex,0);
   t = time;
   prntime(t);
   V(mutex,0);

   printf("\t\tCustomer %d gets food [Waiting time = %d]\n", cno, t - tarr);
   fflush(stdout);

   mntwait(30);

   t += 30;
   P(mutex,0);
   settime(t);
   ++no_of_empty_tables;
   prntime(t);
   V(mutex,0);

   printf("\t\t\tCustomer %d finishes eating and leaves\n", cno);
   fflush(stdout);

   shmdt(M);

   exit(0);
}

int main ( )
{
   FILE *fp;
   int cno, t, ccnt, nc = 0, last = 0;

   fp = (FILE *)fopen("customers.txt", "r");
   while (1) {
      fscanf(fp, "%d", &cno);
      if (cno < 0) break;
      ++nc;
      fscanf(fp, "%d%d", &t, &ccnt);
      if (last < t) mntwait(t - last);
      last = t;
      if (!fork()) cmain(cno,t,ccnt);
   }

   while (nc > 0) {
      wait(NULL);
      --nc;
   }

   getsemshm();
   delsemshm();
   
   fclose(fp);
}
