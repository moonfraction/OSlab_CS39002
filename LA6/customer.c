#include "common.h"
#include <sys/shm.h>
#include <sys/wait.h>

int shmid;
int semid_mutex;
int semid_cook;// no need
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
    
    sem_op(semid_mutex, 0, P); // lock 
    M[Tid] = time_after;
    sem_op(semid_mutex, 0, V); // unlock
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

void eq_WQ(int *M, int waiter_id, int cus_id, int cus_cnt){
    int waiter_offset = waiters_offset[waiter_id];
    int rear = M[waiter_offset + B];
    M[waiter_offset + rear] = cus_id;
    M[waiter_offset + rear + 1] = cus_cnt;
    M[waiter_offset + B] = (rear + 2); // push to waiter queue
}

void cmain(int cus_id, int arrival_time, int cus_cnt){
    // check time
    if(arrival_time > 240){
        print_time(arrival_time);
        printf("%sCustomer %dleaves (late arrival)\n", spc[4], cus_id);
        exit(0);
    }

    int *M = shmat(shmid, NULL, 0);
    if (M == (void *)-1) {
        char error_msg[100];
        sprintf(error_msg, "shmat failed in customer %d", cus_id);
        perror(error_msg);
        exit(1);
    }

    // check for empty table
    sem_op(semid_mutex, 0, P); // lock mutex
    if(M[ETid] == 0){
        // no empty table
        sem_op(semid_mutex, 0, V); // release mutex
        print_time(arrival_time);
        printf("%sCustomer %d leaves (no empty table)\n", spc[4], cus_id);
        exit(0);
    }
    M[ETid]--; // dec empty table
    sem_op(semid_mutex, 0, V); // release mutex

    // take table
    print_time(arrival_time);
    printf("Customer %d arrives (count = %d)\n", cus_id, cus_cnt);

    // wait for waiter -> find a waiter
    sem_op(semid_mutex, 0, P); // lock mutex
    int waiter_id = M[NWid]; // get next waiter id
    M[NWid] = (M[NWid] + 1) % 5; // update next waiter id
    sem_op(semid_mutex, 0, V); // release mutex

    // write in waiter queue
    sem_op(semid_mutex, 0, P); // lock mutex
    eq_WQ(M, waiter_id, cus_id, cus_cnt);
    sem_op(semid_mutex, 0, V); // release mutex

    // wake up waiter to take order
    sem_op(semid_waiter, waiter_id, V);

    // wait for waiter to tell that order is placed
    sem_op(semid_cus, cus_id, P);

    sem_op(semid_mutex, 0, P); // lock mutex
    int cur_time = M[Tid];
    sem_op(semid_mutex, 0, V); // release mutex
    print_time(cur_time);
    printf("%sCustomer %d: Order placed to Waiter %c\n", spc[1], cus_id, WAITERS[waiter_id]);

    // wait for food to be served
    sem_op(semid_cus, cus_id, P);

    sem_op(semid_mutex, 0, P); // lock mutex
    cur_time = M[Tid];
    sem_op(semid_mutex, 0, V); // release mutex
    print_time(cur_time);
    printf("%sCustomer %d gets food [Waiting time = %d]\n", spc[2], cus_id, cur_time - arrival_time);

    // eat food -> delay
    int eat_delay = 30;
    usleep(eat_delay * TIME_SF);

    // eat delay time update
    int new_time = update_sim_time(M, cur_time, eat_delay);

    // release table and update time
    sem_op(semid_mutex, 0, P); // lock mutex
    M[ETid]++; // inc empty table
    sem_op(semid_mutex, 0, V); // release mutex

    // leave
    print_time(new_time);
    printf("%sCustomer %d finishes eating and leaves\n", spc[3], cus_id);

    // detach shared memory
    if (shmdt(M) == -1) {
        char error_msg[100];
        sprintf(error_msg, "shmdt failed in customer %d", cus_id);
        perror(error_msg);
        exit(1);
    }

    exit(0);
}
    

// custormer wrapper
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

    // read customer.txt
    FILE *fp = fopen("customers.txt", "r");
    if (fp == NULL) {
        perror("fopen failed in customer wrapper");
        exit(1);
    }

    // attach shared memory
    int *M = shmat(shmid, NULL, 0);
    if (M == (void *)-1) {
        perror("shmat failed in customer wrapper");
        exit(1);
    }


    int cus_id, arrival_time, cus_cnt;
    int prev_arrival_time = 0;
    pid_t pid;
    int total_customers = 0;
    while (fscanf(fp, "%d %d %d", &cus_id, &arrival_time, &cus_cnt) == 3 && cus_id != -1) {
        // parent process
        // sleep for the difference in arrival time
        int diff = arrival_time - prev_arrival_time; // this is >=0 as per gencustomers code
        // sleep for the difference in arrival time
        usleep(diff * TIME_SF);
        
        // update time
        sem_op(semid_mutex, 0, P); // lock mutex
        M[Tid] = arrival_time;
        sem_op(semid_mutex, 0, V); // release mutex
        
        prev_arrival_time = arrival_time;

        total_customers++;
        pid = fork();
        if (pid == -1) {
            perror("fork failed in customer wrapper");
            exit(1);
        }
        else if (pid == 0) {
            cmain(cus_id, arrival_time, cus_cnt); // cmain does not return, the child process will exit
        }
    }

    // detach shared memory
    if (shmdt(M) == -1) {
        perror("shmdt failed in customer wrapper");
        exit(1);
    }

    // close file
    fclose(fp);

    // wait for all children to exit
    for (int i = 0; i < total_customers; i++) {
        wait(NULL);
    }

    sleep(2);  // Give processes time to finish

    // clean up
    printf("Cleaning up...\n");
    semctl(semid_mutex, 0, IPC_RMID);
    semctl(semid_cook, 0, IPC_RMID);
    semctl(semid_waiter, 0, IPC_RMID);
    semctl(semid_cus, 0, IPC_RMID);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;

}