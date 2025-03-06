// #include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

// void sem_op(int semid, int sem_num, int sem_op_val) {
//     struct sembuf sop;
//     sop.sem_num = sem_num;
//     sop.sem_op = sem_op_val;
//     sop.sem_flg = 0;
//     if (semop(semid, &sop, 1) == -1) {
//         perror("semop failed");
//         exit(1);
//     }
// }

// void print_time(int minutes) {
//     int hours = 11 + minutes / 60;
//     int mins = minutes % 60;
//     char am_pm = (hours < 12) ? 'a' : 'p';
//     hours = hours % 12;
//     if (hours == 0) hours = 12;
//     printf("[%d:%02d %cm] ", hours, mins, am_pm);
// }

// int update_sim_time(int *M, int time_before, int delay) {
//     int time_after = time_before + delay;
//     if(time_after < M[Tid]) {
//         printf("Warning: setting time fails\n");
//         return M[Tid];
//     }
//     M[Tid] = time_after;
//     return time_after;
// }