#include "common.h"
#include <sys/shm.h>
#include <sys/wait.h>

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
    M[Tid] = time_after;
    return time_after;
}

void get_sem(){
    semid_mutex = semget(ftok(FTOK_PATH, SEM_MUTEX), 1, 0666);
    if (semid_mutex == -1) {
        perror("semget failed in waiter wrapper");
        exit(1);
    }

    semid_cook = semget(ftok(FTOK_PATH, SEM_COOK), 1, 0666);
    if (semid_cook == -1) {
        perror("semget failed in waiter wrapper");
        exit(1);
    }

    semid_waiter = semget(ftok(FTOK_PATH, SEM_WAITER), 5, 0666);
    if (semid_waiter == -1) {
        perror("semget failed in waiter wrapper");
        exit(1);
    }

    semid_cus = semget(ftok(FTOK_PATH, SEM_CUS), 256, 0666);
    if (semid_cus == -1) {
        perror("semget failed in waiter wrapper");
        exit(1);
    }
}

int dq_WQ(int *M, int waiter_id, int *cus_id, int *cus_cnt){
    int waiter_offset = waiters_offset[waiter_id];
    int front = M[waiter_offset + F];
    int back = M[waiter_offset + B];
    if(front == back) return -1; // empty queue

    *cus_id = M[waiter_offset + front];
    *cus_cnt = M[waiter_offset + front + 1];
    M[waiter_offset + F] = (front + 2); // pop from waiter queue

    return 0;
}

void eq_CQ(int *M, int waiter_id, int cus_id, int cus_cnt){
    int rear = M[CQB];
    M[CQoff + rear] = waiter_id;
    M[CQoff + rear + 1] = cus_id;
    M[CQoff + rear + 2] = cus_cnt;
    M[CQB] = (rear + 3); // push to cooking queue
}

void print_waiter_exit(int time, int waiter_id){
    print_time(time);
    printf("%sWaiter %c leaving (no more customer to serve)\n", spc[waiter_id], WAITERS[waiter_id]);
}

// waiter main
void wmain(int waiter_id){
    // attach shared memory
    int *M = shmat(shmid, NULL, 0);
    if (M == (void *)-1) {
        char error_msg[100];
        sprintf(error_msg, "shmat failed in waiter %c", WAITERS[waiter_id]);
        perror(error_msg);
        exit(1);
    }

    // get waiter offset
    int waiter_offset = waiters_offset[waiter_id];


    while(1){
        // wait for signal (from new customer or cook)
        sem_op(semid_waiter, waiter_id, P);

        // check if food is ready or a new customer has arrived
        sem_op(semid_mutex, 0, P); // lock mutex for checking cook
        int cus_id = M[waiter_offset + FRid]; // customer id whose food is ready
        int cur_time = M[Tid]; // get the cur time
        sem_op(semid_mutex, 0, V); // release mutex
        
        if(cus_id == 0){ // no food ready
            // i.e. woken up new customer that wants to place order
            // get customer index in waiter queue

            sem_op(semid_mutex, 0, P); // lock mutex to get customer
            int cus_id, cus_cnt;
            int ret = dq_WQ(M, waiter_id, &cus_id, &cus_cnt);
            sem_op(semid_mutex, 0, V); // release mutex

            // if no customer in queue, and time is up, break
            if(ret == -1 && cur_time > 240){ 
                print_waiter_exit(cur_time, waiter_id);
                break;
            }
            if(ret == -1){
                continue; // no customer in queue
            }

            // take order from customer -> take order delay
            int take_delay = 1;
            usleep(take_delay * TIME_SF);

            // print placing order
            
            // 1. update time
            // 2. print placing order
            // 3. update placing order, that have not been served yet
            // 4. add order to cooking queue
            // 5. signal customer that order is placed
            // 6. signal cook that order is placed

            sem_op(semid_mutex, 0, P); // lock mutex
            // update time
            int new_time = update_sim_time(M, cur_time, take_delay);
            
            // print placing order
            print_time(new_time);
            printf("%sWaiter %c: Placing order for Customer %d (Count %d)\n", spc[waiter_id], WAITERS[waiter_id], cus_id, cus_cnt);

            // update placing order
            M[waiter_offset + POid]++; // that have not been served yet

            // add order to cooking queue
            eq_CQ(M, waiter_id, cus_id, cus_cnt);
            sem_op(semid_cook, 0, V); // signal cook that order is placed

            // signal customer that order is placed
            sem_op(semid_cus, cus_id, V);

            sem_op(semid_mutex, 0, V); // release mutex

        }
        else{ // food is ready
            // serve food to customer
            print_time(cur_time);
            printf("%sWaiter %c: Serving food to Customer %d\n", spc[waiter_id], WAITERS[waiter_id], cus_id);
            sem_op(semid_cus, cus_id, V); // signal customer that food is served

            // reset cus_id in waiter queue and dec the placing order 
            sem_op(semid_mutex, 0, P); // lock mutex
            M[waiter_offset + FRid] = 0; // reset cus_id
            M[waiter_offset + POid]--; // dec placing order
            sem_op(semid_mutex, 0, V); // release mutex
        }
    }

    // detach shared memory
    if (shmdt(M) == -1) {
        char error_msg[100];
        sprintf(error_msg, "shmdt failed in waiter %c", WAITERS[waiter_id]);
        perror(error_msg);
        exit(1);
    }

    // exit
    exit(0);
}

// waiter wrapper
int main(){
    // get shared memory
    key_t shm_key = ftok(FTOK_PATH, SHM_ID);
    shmid = shmget(shm_key, 2000*sizeof(int), 0666);
    if (shmid == -1) {
        perror("shmget failed in waiter wrapper");
        exit(1);
    }

    // get all semaphores (semid_mutex, semid_cook, semid_waiter, semid_cus)
    get_sem(); 

    // fork waiter processes
    pid_t pid;
    for (int i = 0; i < 5; i++) {
        pid = fork();
        if (pid == -1) {
            perror("fork failed in waiter wrapper");
            exit(1);
        }
        else if (pid == 0) {
            printf("%sWaiter %c is ready\n", spc[i], WAITERS[i]);
            wmain(i); // wmain does not return, the child process will exit
        }
    }

    // wait for all children to exit
    for (int i = 0; i < 5; i++) {
        wait(NULL);
    }

    return 0;
}