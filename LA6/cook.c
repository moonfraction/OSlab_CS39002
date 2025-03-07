#include "common.h"
#include <sys/shm.h>
#include <sys/wait.h>

// Global variables for shared memory and semaphores
int shmid;
int semid_mutex;
int semid_cook;
int semid_waiter;
int semid_cus;

void sem_op(int semid, int sem_num, int sem_op_val) {
    struct sembuf sop;
    sop.sem_num = sem_num;
    sop.sem_op = sem_op_val;
    sop.sem_flg = 0;
    if (semop(semid, &sop, 1) == -1) {
        perror("semop failed");
        exit(1);
    }
}
void print_time(int minutes) {
    int hours = 11 + minutes / 60;
    int mins = minutes % 60;
    char am_pm = (hours < 12) ? 'a' : 'p';
    hours = hours % 12;
    if (hours == 0) hours = 12;
    printf("[%d:%02d %cm] ", hours, mins, am_pm);
}

int update_sim_time(int *M, int time_before, int delay) {
    int time_after = time_before + delay;
    if(time_after < M[Tid]) {
        printf("Warning: setting time fails\n");
        return M[Tid];
    }
    
    sem_op(semid_mutex, 0, P); //lock 
    M[Tid] = time_after;
    sem_op(semid_mutex, 0, V); //unlock
    return time_after;
}


int create_shmem(const char *path, int proj_id, size_t size) {
    key_t shm_key = ftok(path, proj_id);
    int shmid = shmget(shm_key, size, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed in cook wrapper");
        exit(1);
    }
    return shmid;
}

int create_sem(const char *path, int proj_id, int nsems) {
    key_t sem_key = ftok(path, proj_id);
    int semid = semget(sem_key, nsems, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget failed in cook wrapper");
        exit(1);
    }
    return semid;
}

void init_sem(int semid, int sem_num, int sem_val) {
    if (semctl(semid, sem_num, SETVAL, sem_val) == -1) {
        perror("semctl failed in cook wrapper");
        exit(1);
    }
}

void init_waiter_queue(int *M, int WQoff){
    M[WQoff + FRid] = 0; // stores cus id (a pos integer) whose food is ready
    M[WQoff + POid] = 0; // no. of customers that have placed order and are waiting for food
    M[WQoff + F] = 4; // front index
    M[WQoff + B] = 4; // back index
}

// handle dequeuing from cooking queue
// return -1 if empty queue, 1 if empty after dequeuing, 0 otherwise
int dq_CQ(int *M, int *waiter_id, int *cus_id, int *cus_cnt){
    int front = M[CQF];
    int rear = M[CQB];
    if(front == rear) return -1; // empty queue
    
    *waiter_id = M[CQoff + front];
    *cus_id = M[CQoff + front + 1];
    *cus_cnt = M[CQoff + front + 2];
    M[CQF] = (front + 3); // pop from cooking queue
    
    // check if the queue is empty, after preparing the order
    if(M[CQF] == M[CQB]) return 1;

    return 0;
}

// the last cook sees that there are no orders to be served. It then wakes up all the five waiters.
void wake_all_waiters(){
    for (int i = 0; i < 5; i++) {
        sem_op(semid_waiter, i, V);
    }
}

// print cook exit message
void print_cook_exit(int time, int cook_id){
    print_time(time);
    printf("%sCook %c: Leaving\n", spc[cook_id], COOKS[cook_id]);
}

// cooks main
void cmain(int cook_id){
    // attach shared memory
    int *M = shmat(shmid, NULL, 0);
    if (M == (void *)-1) {
        char error_msg[100];
        sprintf(error_msg, "shmat failed in cook %c", COOKS[cook_id]);
        perror(error_msg);
        exit(1);
    }

    while(1){
        // wait for cooking request on cook semaphore
        sem_op(semid_cook, 0, P);

        // access cooking queue in shmem M(critical section)
        sem_op(semid_mutex, 0, P); // lock mutex
        int waiter_id, cus_id, cus_cnt;
        int ret = dq_CQ(M, &waiter_id, &cus_id, &cus_cnt);
        
        int cur_time = M[Tid]; // get the curr time
        sem_op(semid_mutex, 0, V); // release mutex
        
        // if no order and time is up, break
        if(ret == -1 && cur_time > 240){
            print_cook_exit(cur_time, cook_id);
            wake_all_waiters();
            break;
        }
        if(ret == -1){
            wake_all_waiters();
            continue; // no order in queue
        }

        // cook food
        print_time(cur_time); 
        printf("%sCook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n", spc[cook_id], COOKS[cook_id], WAITERS[waiter_id], cus_id, cus_cnt);

        
        // Prepared order -> cook delay
        int cook_delay = 5 * cus_cnt;
        usleep(cook_delay * TIME_SF);
        
        // update time
        int new_time = update_sim_time(M, cur_time, cook_delay);

        print_time(new_time);
        printf("%sCook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n", spc[cook_id], COOKS[cook_id], WAITERS[waiter_id], cus_id, cus_cnt);

        // about to signal waiter that food is ready
        // write cus_id in waiter queue
        int WQoff = waiters_offset[waiter_id];
        int fr = WQoff + FRid; // food ready index
        sem_op(semid_mutex, 0, P); // lock mutex
        M[fr] = cus_id; // write cus_id in waiter queue
        sem_op(semid_mutex, 0, V); // release mutex

        sem_op(semid_waiter, waiter_id, V); // signal waiter that food is ready


        // if time > 240, and no order in queue, break
        if(new_time > 240 && ret == 1){
            print_cook_exit(new_time, cook_id);
            wake_all_waiters();
            break;
        }
    }

    // detach shared memory
    if (shmdt(M) == -1) {
        char error_msg[100];
        sprintf(error_msg, "shmdt failed in cook %c", COOKS[cook_id]);
        perror(error_msg);
        exit(1);
    }

    // exit
    exit(0);
}


// cook wrapper
int main(){
    // create shared memory
    shmid = create_shmem(FTOK_PATH, SHM_ID, 2000*sizeof(int));
    
    // create semaphores
    semid_mutex = create_sem(FTOK_PATH, SEM_MUTEX, 1);  // single mutex semaphore
    semid_cook = create_sem(FTOK_PATH, SEM_COOK, 1);    // single cook semaphore
    semid_waiter = create_sem(FTOK_PATH, SEM_WAITER, 5); // five waiter semaphores
    semid_cus = create_sem(FTOK_PATH, SEM_CUS, 256);    // 256 customer semaphores
    
    // initialize semaphores
    init_sem(semid_mutex, 0, 1); // mutex semaphore
    init_sem(semid_cook, 0, 0); // cook semaphore
    // initialize waiter semaphores
    for (int i = 0; i < 5; i++) {
        init_sem(semid_waiter, i, 0);
    }
    // initialize customer semaphores
    for (int i = 0; i < 256; i++) {
        init_sem(semid_cus, i, 0);
    }

    // attach shared memory
    int *M = shmat(shmid, NULL, 0);
    if (M == (void *)-1) {
        perror("shmat failed in cook wrapper");
        exit(1);
    }

    // lock mutex before initializing shared memory
    sem_op(semid_mutex, 0, P);
    
    // init shared memory
    M[Tid] = 0; // time
    M[ETid] = 10; // no. of empty table
    M[NWid] = 0; // next waiter number
    M[PdOid] = 0; // no. of pending order

    // init cooking queue
    M[CQF] = 0; // cooking queue front
    M[CQB] = 0; // cooking queue back
    
    // init waiter(s) queue
    init_waiter_queue(M, WUoff);
    init_waiter_queue(M, WVoff);
    init_waiter_queue(M, WWoff);
    init_waiter_queue(M, WXoff);
    init_waiter_queue(M, WYoff);
    
    // release mutex after initialization
    sem_op(semid_mutex, 0, V);

    // detach shared memory
    if (shmdt(M) == -1) {
        perror("shmdt failed in cook wrapper");
        exit(1);
    }

    // fork two child processes for cooks C and D
    pid_t pid_C = fork();
    if (pid_C == -1) {
        perror("fork failed in cook wrapper");
        exit(1);
    }
    if(pid_C == 0) {
        print_time(0);
        printf("%sCook %c ready\n", spc[0], COOKS[0]);
        cmain(0); // cmain does not return, the child process will exit
    }

    pid_t pid_D = fork();
    if (pid_D == -1) {
        perror("fork failed in cook wrapper");
        exit(1);
    }
    if(pid_D == 0) {
        print_time(0);
        printf("%sCook %c ready\n", spc[1], COOKS[1]);
        cmain(1); // cmain does not return, the child process will exit
    }
    
    // wait for children cooks C and D to finish
    wait(NULL);
    wait(NULL);

    // printf("Cooks done, exiting without removing IPCs\n");
    return 0;

}