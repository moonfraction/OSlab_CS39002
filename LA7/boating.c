#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "pthread_barrier.h" 
// Custom barrier implementation for macOS (defined for __APPLE__)

/* counting semaphore using  mutex and condition variable */
typedef struct {
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;

void P(semaphore *s){
    pthread_mutex_lock(&s->mtx);
    s->value--;
    if(s->value < 0){
        pthread_cond_wait(&s->cv, &s->mtx);
    }
    pthread_mutex_unlock(&s->mtx);
}

void V(semaphore *s){
    pthread_mutex_lock(&s->mtx);
    s->value++;
    if(s->value <= 0){
        pthread_cond_signal(&s->cv);
    }
    pthread_mutex_unlock(&s->mtx);
}

/* global shared variables  */ 
int m; // boats
int n; // visitors

int *BA; // availability
int *BC; // visitor id or -1
int *BT; // ride time of current visitor
pthread_barrier_t *BB; // array of barriers, one for each boat -> init to 2

// mutex for accessing BA, BC, BT
pthread_mutex_t bmtx = PTHREAD_MUTEX_INITIALIZER;

// semaphores
semaphore rider = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
semaphore boat = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

// all visitors have left
int vis_left;
pthread_mutex_t vismutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t viscond = PTHREAD_COND_INITIALIZER;

// sleep for m minutes => 1min ~ 100ms = 100000us
void msleep(int m){
    usleep(m * 100000);
}

void init_arrays(){
    BA = (int *) malloc(m * sizeof(int));
    BC = (int *) malloc(m * sizeof(int));
    BT = (int *) malloc(m * sizeof(int));
    for(int i = 0; i < m; i++){
        BA[i] = 0; // not available
        BC[i] = -1; // no visitor
        BT[i] = 0; // ride time
    }
}


void *boat_thread(void *arg){
    int i = *((int *) arg);
    free(arg);

    while(1){
        V(&rider); // signal rider for its availability
        P(&boat); // wait for rider to board
        
        // check if all visitors have left
        pthread_mutex_lock(&vismutex);
        if(vis_left == 0){
            pthread_mutex_unlock(&vismutex);
            break;
        }
        pthread_mutex_unlock(&vismutex);
        
        // mark boat available
        pthread_mutex_lock(&bmtx);
        BC[i] = -1; // no visitor
        BA[i] = 1; // mark boat available

        // init barrier
        pthread_barrier_init(&BB[i], NULL, 2);
        pthread_mutex_unlock(&bmtx); // unlock mutex

        // boat made available for visitors tp ride
        // printf("Boat      %4d    Ready\n", i+1);
        // fflush(stdout);

        // wait for visitor to board
        // -> (one[boat] is waiting at the barrier, another visitor will come and lift the barrier)
        pthread_barrier_wait(&BB[i]);
        // barrier lifted

        // boat no longer available
        pthread_mutex_lock(&bmtx);
        BA[i] = 0; // boat no longer available
        int ride_time = BT[i];
        int visitor_id = BC[i];
        pthread_barrier_destroy(&BB[i]);
        pthread_mutex_unlock(&bmtx);

        printf("Boat      %4d    Start of ride for visitor %4d\n", i+1, visitor_id);
        fflush(stdout);
        msleep(ride_time);

        printf("Boat      %4d    End of ride for visitor %4d (ride time = %4d)\n", i+1, visitor_id, ride_time);
        fflush(stdout);
    }
    
    pthread_exit(NULL);
}

void *visitor_thread(void *arg){
    int vis_id = *((int *) arg);
    free(arg);

    int vtime = 30 + rand() % 91;
    int rtime = 15 + rand() % 46;

    printf("Visitor   %4d    Starts sightseeing for %4d minutes\n", vis_id, vtime);
    fflush(stdout);
    msleep(vtime);

    printf("Visitor   %4d    Ready to ride a boat (ride time = %4d)\n", vis_id, rtime);
    fflush(stdout);

    V(&boat); // signal boat for availability of vis
    P(&rider); // wait for boat to board

    // find a boat to ride
    int boat_id = -1;
    while(boat_id == -1){
        pthread_mutex_lock(&bmtx);
        for(int i = 0; i < m; i++){
            if(BA[i] == 1){
                boat_id = i;
                BC[i] = vis_id;
                BT[i] = rtime;
                break;
            }
        }
        pthread_mutex_unlock(&bmtx);
        if(boat_id == -1){
            // no boat available, wait for next boat
            msleep(1);
        }
    }

    // boat found, wait for boat to start ride
    pthread_barrier_wait(&BB[boat_id]);
    // barrier lifted

    // boat started ride
    printf("Visitor   %4d    Finds boat %4d\n", vis_id, boat_id+1);
    fflush(stdout);
    msleep(rtime);

    // boat ride over
    printf("Visitor   %4d    Leaving\n", vis_id);
    fflush(stdout);

    // visitor left
    pthread_mutex_lock(&vismutex);
    vis_left--;
    pthread_mutex_unlock(&vismutex);

    pthread_exit(NULL);
}


void create_boats(pthread_t *boat_tid){
    for(int i = 0; i < m; i++){
        int *arg = (int *) malloc(sizeof(int));
        *arg = i; // 0 indexed due to the global shared arrays
        if(pthread_create(&boat_tid[i], NULL, boat_thread, (void *) arg) != 0){
            perror("Failed to create boat thread");
            exit(EXIT_FAILURE);
        }
        printf("Boat      %4d    Ready\n", i+1);
        fflush(stdout);
    }
}

void create_visitors(pthread_t *visitor_tid){
    for(int i = 0; i < n; i++){
        int *arg = (int *) malloc(sizeof(int));
        *arg = i+1; // visitor id 1 indexed
        if(pthread_create(&visitor_tid[i], NULL, visitor_thread, (void *) arg) != 0){
            perror("Failed to create visitor thread");
            exit(EXIT_FAILURE);
        }
    }
}


int main(int argc, char *argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <no. of boats[5, 10]> <no. of visitors[20, 100]>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    m = atoi(argv[1]);
    n = vis_left = atoi(argv[2]);

    // check m, n ranges
    if(m < 5 || m > 10 || n < 20 || n > 100){
        fprintf(stderr, "Invalid values: m should be between 5 and 10, n between 20 and 100\n");
        exit(EXIT_FAILURE);
    }

    srand((unsigned int)time(NULL));

    // allocate and init shared memory arrays (BA, BC, BT)
    init_arrays();

    BB = (pthread_barrier_t *) malloc(m * sizeof(pthread_barrier_t)); // init in boat_thread
    
    // create boat threads
    pthread_t boat_tid[m];
    create_boats(boat_tid);

    // create visitor threads
    pthread_t visitor_tid[n];
    create_visitors(visitor_tid);

    // wait for all visitors to leave
    for(int i = 0; i < n; i++){
        if(pthread_join(visitor_tid[i], NULL) != 0){
            perror("Failed to join visitor thread");
            exit(EXIT_FAILURE);
        }
    }

    // wait for all boats to finish
    for(int i = 0; i < m; i++){
        if(pthread_join(boat_tid[i], NULL) != 0){
            perror("Failed to join boat thread");
            exit(EXIT_FAILURE);
        }
    }

    // free memory
    free(BA);
    free(BC);
    free(BT);
    free(BB);

    exit(EXIT_SUCCESS);
}