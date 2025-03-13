#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "pthread_barrier.h" // Custom barrier implementation for macOS

// Counting sem
typedef struct {
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;


void P(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    s->value--;
    if (s->value < 0) {
        pthread_cond_wait(&s->cv, &s->mtx);
    }
    pthread_mutex_unlock(&s->mtx);
}

void V(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    s->value++;
    if (s->value <= 0) {
        pthread_cond_signal(&s->cv);
    }
    pthread_mutex_unlock(&s->mtx);
}

// Global shared variables
int m; // no. of boats
int n; // no. of visitors

// Shared array for boat/visitor:
int *BC; // visid or -1
int *BA; // 1 = avail
int *BT; // ride time of cur vis

pthread_barrier_t *BB; // array of barriers, 1/noat

// Mutex for accessing BA, BC, BT.
pthread_mutex_t bmtx = PTHREAD_MUTEX_INITIALIZER;

semaphore rider_sem = { 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER };
semaphore boat_sem = { 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER };

int finished = 0;
pthread_mutex_t finish_mutex = PTHREAD_MUTEX_INITIALIZER;

// all visitors have left.
volatile int done = 0;

void sleep_min(int minutes) {
    usleep(minutes * 100000);
}

void *boat_thread(void *arg) {
    int i = *((int *) arg);
    free(arg);

    while (1) {
        V(&rider_sem);

        P(&boat_sem);

        if (done) {
            break;
        }

        pthread_mutex_lock(&bmtx);
        BC[i] = -1;     // no vis yet
        BA[i] = 1;      // mark boat avail
        
        pthread_barrier_init(&BB[i], NULL, 2);
        pthread_mutex_unlock(&bmtx);

        pthread_barrier_wait(&BB[i]);

        pthread_mutex_lock(&bmtx);
        BA[i] = 0; // boat as no longer avail
        int ride_time = BT[i];
        int visitor_id = BC[i];
        pthread_barrier_destroy(&BB[i]);
        pthread_mutex_unlock(&bmtx);

        printf("Boat        %4d    Start of ride for visitor %4d\n", i+1, visitor_id);
        fflush(stdout);
        sleep_min(ride_time);
        printf("Boat        %4d    End of ride for visitor %4d (ride time = %4d)\n", i+1, visitor_id, ride_time);
        fflush(stdout);
    }
    return NULL;
}

void *visitor_thread(void *arg) {
    int id = *((int *) arg);
    free(arg);

    int vtime = 30 + rand() % 91;
    int rtime = 15 + rand() % 46;

    printf("Visitor     %4d    Starts sightseeing for %4d minutes\n", id, vtime);
    fflush(stdout);

    sleep_min(vtime);

    printf("Visitor     %4d    Ready to ride a boat (ride time = %4d)\n", id, rtime);
    fflush(stdout);
    V(&boat_sem);

    P(&rider_sem);

    int boat_index = -1;
    while (boat_index == -1) {
        pthread_mutex_lock(&bmtx);
        for (int i = 0; i < m; i++) {
            if (BA[i] == 1 && BC[i] == -1) {
                boat_index = i;
                BT[i] = rtime;
                BC[i] = id;   
                pthread_barrier_wait(&BB[i]);
                break;
            }
        }
        pthread_mutex_unlock(&bmtx);
        if (boat_index == -1) {
            usleep(5000);
        }
    }

    printf("Visitor     %4d    Finds boat %4d\n", id, boat_index+1);
    fflush(stdout);

    sleep_min(rtime);

    printf("Visitor     %4d    Leaving\n", id);
    fflush(stdout);

    pthread_mutex_lock(&finish_mutex);
    finished++;
    int local_finished = finished;
    pthread_mutex_unlock(&finish_mutex);

    if (local_finished >= n) {
        done = 1;
        for (int i = 0; i < m; i++) {
            V(&boat_sem);
        }
    }

    return NULL;
}

void create_boats(pthread_t *boat_tid) {
    for (int i = 0; i < m; i++) {
        int *arg = malloc(sizeof(int));
        *arg = i;
        if (pthread_create(&boat_tid[i], NULL, boat_thread, arg) != 0) {
            perror("Failed to create boat thread");
            exit(EXIT_FAILURE);
        }
        printf("Boat        %4d    Ready\n", i+1);
        fflush(stdout);
    }
}

void create_visitors(pthread_t *visitor_tid) {
    for (int i = 0; i < n; i++) {
        int *arg = malloc(sizeof(int));
        *arg = i+1; // visitor id start at 1
        if (pthread_create(&visitor_tid[i], NULL, visitor_thread, arg) != 0) {
            perror("Failed to create visitor thread");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s m n\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    if (m < 5 || m > 10 || n < 20 || n > 100) {
        fprintf(stderr, "Invalid values: m should be between 5 and 10, n between 20 and 100\n");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    // Allocate shared arrays.
    BA = malloc(m * sizeof(int));
    BC = malloc(m * sizeof(int));
    BT = malloc(m * sizeof(int));
    BB = malloc(m * sizeof(pthread_barrier_t));

    for (int i = 0; i < m; i++) {
        BA[i] = 0;    
        BT[i] = 0;
        BC[i] = -1;
    }

    // Create boat threads.
    pthread_t *boat_tid = malloc(m * sizeof(pthread_t));
    create_boats(boat_tid);

    // Create visitor threads.
    pthread_t *visitor_tid = malloc(n * sizeof(pthread_t));
    create_visitors(visitor_tid);

    // Wait for all visitor 
    for (int i = 0; i < n; i++) {
        pthread_join(visitor_tid[i], NULL);
    }

    for (int i = 0; i < m; i++) {
        pthread_join(boat_tid[i], NULL);
    }

    // Cleanup.
    free(BA);
    free(BC);
    free(BT);
    free(BB);
    free(boat_tid);
    free(visitor_tid);

    return 0;
}
