#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

#define COOKS "CD"
#define WAITERS "UVWXY"
#define S1 ""
#define S2 "  "
#define S3 "    "
#define S4 "      "
#define S5 "        "

const char* spc[] = {S1, S2, S3, S4, S5};


// to get keys for shared memory and semaphores
#define FTOK_PATH "common.h"
#define SHM_ID 'S'
#define SEM_MUTEX 'M'
#define SEM_COOK 'C'
#define SEM_WAITER 'W'
#define SEM_CUS 'U'
#define SEM_CUSNUM 256
#define SEM_OUTPUT 'O' // for output semaphore

// global shmem indices
#define Tid 0 // time
#define ETid 1 // no. of empty table
#define NWid 2 // next waiter number
#define PdOid 3 // no. of pending order
/* NOTE: we have PO for placing order (so PdOid for pending order, to avoid confusion) */
#define CQF 4 // cooking queue front
#define CQB 5 // cooking queue back

// waiter queue -> sliding window
#define WQ_SIZE 200
#define WUoff 100 // waiter U offset
#define WVoff 300 // waiter V offset
#define WWoff 500 // waiter W offset
#define WXoff 700 // waiter X offset
#define WYoff 900 // waiter Y offset
#define FRid 0
#define POid 1
#define F 2
#define B 3

int waiters_offset[] = {WUoff, WVoff, WWoff, WXoff, WYoff};


// cooking queue -> sliding window
#define CQ_SIZE 900
#define CQoff 1100 // cooking queue offset

// semaphoes operations
#define P -1 // P operation
#define V 1 // V operation

// time scaling factor
#define TIME_SF 100000 // 1 min => 100ms = 100,000 us

// function prototypes
void sem_op(int semid, int sem_num, int sem_op_val);
void print_time(int minutes);
int update_sim_time(int *M, int time_before, int delay);

#endif // COMMON_H