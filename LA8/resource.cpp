#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <queue>
#include <vector>

#ifdef __APPLE__
#include "pthread_barrier.h"
#endif

// Maximum values
#define MAX_RESOURCES 20
#define MAX_THREADS 100

// sync variables
pthread_mutex_t rmtx; // Resource mutex
pthread_mutex_t pmtx; // Print mutex
pthread_barrier_t BOS; // Beginning of session barrier
pthread_barrier_t REQB; // Request barrier

// Thread-specific synchronization
pthread_barrier_t *ACKB; // Acknowledgment barriers for each thread
pthread_cond_t *cv; // Condition variables for each thread
pthread_mutex_t *cmtx; // Mutex for condition variables

// threads function prototypes
void *user_thread(void *arg);

// Global request variable
typedef struct {
    int type; // 0 for RELEASE, 1 for ADDITIONAL, 2 for QUIT
    int thread_id;
    int *request;
} Request;

typedef struct{
    int thread_id;
    int *req;
} local_request;

Request *g_request;

// function prototypes
void process_pending_requests(std::queue<local_request> &Q, int m, int n, int **ALLOC, int **NEED, int *AVAILABLE);
bool can_fulfill_req(int *request, int *NEED, int *AVAILABLE, int m);
bool is_safe_state(int *AVAILABLE, int **ALLOC, int **NEED, int m, int n, int *active_threads);
void printQ(std::queue<local_request> &Q);


int main(){
    /***** Read system configuration *****/
    FILE *system_file = fopen("input/system.txt", "r");
    if (!system_file) {
        perror("Error opening system.txt");
        return 1;
    }

    int m, n; // Number of resource types, Number of threads
    fscanf(system_file, "%d %d", &m, &n);
    if(m > MAX_RESOURCES || n > MAX_THREADS){
        perror("Number of resource types or threads exceeds maximum");
        return 1;
    }

    int AVAILABLE[m]; // Available resources
    for (int i = 0; i < m; i++) {
        fscanf(system_file, "%d", &AVAILABLE[i]);
    }
    fclose(system_file);

    // Initialize matrices
    int ** ALLOC = (int **)malloc(n * sizeof(int *));
    int ** MAX_NEED = (int **)malloc(n * sizeof(int *));
    int ** NEED = (int **)malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        ALLOC[i] = (int *)malloc(m * sizeof(int));
        MAX_NEED[i] = (int *)malloc(m * sizeof(int));
        NEED[i] = (int *)malloc(m * sizeof(int));
    }
    memset(ALLOC, 0, sizeof(ALLOC));

    // Read thread files to initialize MAX_NEED and NEED matrices
    for (int i = 0; i < n; i++) {
        char filename[30];
        sprintf(filename, "input/thread%02d.txt", i);
        FILE *thread_file = fopen(filename, "r");
        if (!thread_file) {
            perror("Error opening thread file");
            return 1;
        }

        for (int j = 0; j < m; j++) {
            fscanf(thread_file, "%d", &MAX_NEED[i][j]);
            NEED[i][j] = MAX_NEED[i][j]; // Initially, need = max need
        }
        fclose(thread_file);
    }

    /***** Initialize synchronization primitives *****/
    pthread_mutex_init(&rmtx, NULL);
    pthread_mutex_init(&pmtx, NULL);
    pthread_barrier_init(&BOS, NULL, n + 1);
    pthread_barrier_init(&REQB, NULL, 2);

    // Initialize thread-specific synchronization
    ACKB = (pthread_barrier_t *)malloc(n * sizeof(pthread_barrier_t));
    cv = (pthread_cond_t *)malloc(n * sizeof(pthread_cond_t));
    cmtx = (pthread_mutex_t *)malloc(n * sizeof(pthread_mutex_t));

    for (int i = 0; i < n; i++) {
        pthread_barrier_init(&ACKB[i], NULL, 2);
        pthread_cond_init(&cv[i], NULL);
        pthread_mutex_init(&cmtx[i], NULL);
    }

    // Create user threads
    pthread_t *users = (pthread_t *)malloc(n * sizeof(pthread_t));
    for (int i = 0; i < n; i++) {
        int *id = (int *)malloc(sizeof(int));
        *id = i;
        pthread_create(&users[i], NULL, user_thread, id);
    }

    /************************ master work ************************/

    // local master request queue
    std::queue<local_request> Q;

    // Wait for all threads to be ready
    pthread_barrier_wait(&BOS);

    pthread_mutex_lock(&pmtx);
    printf("==> Master: All threads ready, simulation starting\n");
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);

    int terminated_threads = 0;
    int active_threads[n];
    for(int i = 0; i < n; i++){
        active_threads[i] = 1;
    }

    // Main processing loop
    while(terminated_threads < n){
        // wait for a req
        pthread_barrier_wait(&REQB);

        // process req
        int thread_id = g_request->thread_id;
        int request_type = g_request->type;
        int *request = g_request->request;

        // // Acknowledge receipt of request
        // pthread_barrier_wait(&ACKB[thread_id]);

        if(request_type == 2){ // quit
            // release all resources held by the thread
            for (int i = 0; i < m; i++) {
                AVAILABLE[i] += ALLOC[thread_id][i];
                ALLOC[thread_id][i] = 0;
                NEED[thread_id][i] = MAX_NEED[thread_id][i];
            }

            terminated_threads++;
            active_threads[thread_id] = 0;

            pthread_mutex_lock(&pmtx);
            printf("Master thread releases resources of thread %d\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);

            // Acknowledge receipt of request
            pthread_barrier_wait(&ACKB[thread_id]);

            // print active threads
            pthread_mutex_lock(&pmtx);
            printf("%d threads left: ", n - terminated_threads);
            for(int i = 0; i < n; i++){
                if(active_threads[i]){
                    printf("%d ", i);
                }
            }
            printf("\n");
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);

            // print avaiilable resources
            pthread_mutex_lock(&pmtx);
            printf("Available resources: ");
            for(int i = 0; i < m; i++){
                printf("%d ", AVAILABLE[i]);
            }
            printf("\n");
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);

            process_pending_requests(Q, m, n, ALLOC, NEED, AVAILABLE);

            continue;
        }
        else if(request_type == 1){ // additional
            // handle release components first
            for (int i = 0; i < m; i++) {
                if(request[i] < 0){
                    AVAILABLE[i] += -request[i];
                    ALLOC[thread_id][i] += request[i];
                    NEED[thread_id][i] -= request[i];
                    request[i] = 0;
                }
            }

            // Acknowledge receipt of request
            pthread_barrier_wait(&ACKB[thread_id]);
            

            // enqueue the request
            local_request lr;
            lr.thread_id = thread_id;
            lr.req = request;
            Q.push(lr);

            pthread_mutex_lock(&pmtx);
            printf("Master thread stores resource request of thread %d\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
        }
        else{ // release
            for (int i = 0; i < m; i++) {
                if(request[i] < 0){
                    AVAILABLE[i] += -request[i];
                    ALLOC[thread_id][i] += request[i];
                    NEED[thread_id][i] -= request[i];
                }
            }
            // Acknowledge receipt of request
            pthread_barrier_wait(&ACKB[thread_id]);
        }

        // process pending requests
        process_pending_requests(Q, m, n, ALLOC, NEED, AVAILABLE);
    }

    /********************** master work end **********************/

    // Wait for thread to complete
    for(int i = 0; i < n; i++){
        pthread_join(users[i], NULL);
    }

    pthread_mutex_lock(&pmtx);
    printf("==> Master: All threads terminated, simulation ending\n");
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);



    // Cleanup
    pthread_mutex_destroy(&rmtx);
    pthread_mutex_destroy(&pmtx);
    pthread_barrier_destroy(&BOS);
    pthread_barrier_destroy(&REQB);

    for (int i = 0; i < n; i++) {
        pthread_barrier_destroy(&ACKB[i]);
        pthread_cond_destroy(&cv[i]);
        pthread_mutex_destroy(&cmtx[i]);
    }

    free(users);
    free(ACKB);
    free(cv);
    free(cmtx);

    return 0;    
}

void *user_thread(void *arg){
    int tid = *(int *)arg;  
    free(arg);

    // open thread file
    char filename[30];
    sprintf(filename, "input/thread%02d.txt", tid);
    FILE *thread_file = fopen(filename, "r");
    if(!thread_file){
        perror("Error opening thread file");
        return NULL;
    }

    // skip the max needs line-> already read in main
    char line[100];
    fgets(line, sizeof(line), thread_file);

    // wait for all threads to be ready
    pthread_barrier_wait(&BOS);

    pthread_mutex_lock(&pmtx);
    printf("    Thread %d born\n", tid);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);

    // process each request from the thread file
    while(fgets(line, sizeof(line), thread_file)){
        // parse request line
        int delay, r;
        int request[MAX_RESOURCES] = {0};

        char *token = strtok(line, " \t\n");
        if(!token) continue;

        delay = atoi(token);

        token = strtok(NULL, " \t\n");
        if(!token) continue;

        if(strcmp(token, "Q") == 0){
            // QUIT request
            usleep(delay * 1000); // convert to microseconds


            pthread_mutex_lock(&rmtx);

            g_request = (Request *)malloc(sizeof(Request));
            g_request->type = 2; // QUIT
            g_request->thread_id = tid;
            g_request->request = NULL;

            pthread_barrier_wait(&REQB);
            pthread_barrier_wait(&ACKB[tid]);

            pthread_mutex_unlock(&rmtx);
            break;
        }
        else{ // request
            int ri = 0;
            bool is_add = false;
            while((token = strtok(NULL, " \t\n"))){
                request[ri++] = atoi(token);
                if(atoi(token) > 0){
                    is_add = true;
                }
            }

            // wait for specified delay to send this request
            usleep(delay * 1000); // convert to microseconds

            // send the request -> store in global request
            pthread_mutex_lock(&rmtx);

            g_request = (Request *)malloc(sizeof(Request));
            g_request->type = is_add ? 1 : 0; // 1 for ADDITIONAL, 0 for RELEASE
            g_request->thread_id = tid;
            g_request->request = (int *)malloc(ri * sizeof(int));
            for(int i = 0; i < ri; i++){
                g_request->request[i] = request[i];
            }

            pthread_barrier_wait(&REQB);
            pthread_barrier_wait(&ACKB[tid]);

            pthread_mutex_unlock(&rmtx);

            pthread_mutex_lock(&pmtx);
            printf("    Thread %d sends resource request: type = %s\n", tid, is_add ? "ADDITIONAL" : "RELEASE");
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);

            // if request is ADDITIONAL, wait for it to be granted
            if(is_add){
                pthread_mutex_lock(&cmtx[tid]);

                pthread_cond_wait(&cv[tid], &cmtx[tid]);

                pthread_mutex_lock(&pmtx);
                printf("    Thread %dis granted its last resource request\n", tid);
                fflush(stdout);
                pthread_mutex_unlock(&pmtx);

                pthread_mutex_unlock(&cmtx[tid]);
            }
            else{
                pthread_mutex_lock(&pmtx);
                printf("    Thread %d is done with its resource release request\n", tid);
                fflush(stdout);
                pthread_mutex_unlock(&pmtx);
            }
        }
    }

    pthread_mutex_lock(&pmtx);
    printf("    Thread %d going to quit\n", tid);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);

    fclose(thread_file);

    return NULL;
}

void printQ(std::queue<local_request> &Q){
    printf("        Waiting thereads: ");
    std::queue<local_request> tempQ = Q;
    while(!tempQ.empty()){
        local_request lr = tempQ.front();
        tempQ.pop();
        printf("%d ", lr.thread_id);
    }
    printf("\n");
}

void process_pending_requests(std::queue<local_request> &Q, int m, int n, int **ALLOC, int **NEED, int *AVAILABLE, int *active_threads) {
    std::queue<local_request> temp_queue;

    pthread_mutex_lock(&pmtx);
    printQ(Q);    
    printf("Master thread tries to grant pending requests\n");
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);

    // process all requests in the queue
    while(!Q.empty()){
        local_request lr = Q.front();
        Q.pop();

        int thread_id = lr.thread_id;
        int *request = lr.req;

        // Only process requests for active threads
        if (!active_threads[thread_id]) {
            continue;
        }

        if(can_fulfill_req(request, NEED[thread_id], AVAILABLE, m)){
            // grant request
            for(int i = 0; i < m; i++){
                AVAILABLE[i] -= request[i];
                ALLOC[thread_id][i] += request[i];
                NEED[thread_id][i] -= request[i];
            }

            pthread_mutex_lock(&pmtx);
            printf("Master thread grants resource request for thread %d\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);

            // signal the thread
            pthread_mutex_lock(&cmtx[thread_id]);
            pthread_cond_signal(&cv[thread_id]);
            pthread_mutex_unlock(&cmtx[thread_id]);
        }
        else{
            // cannot fulfill request, put it back in the queue
            temp_queue.push(lr);
            pthread_mutex_lock(&pmtx);
            printf("    +++ Insufficient resources to grant request of thread %d\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
        }
    }

    // put back pending requests
    Q = temp_queue;
    pthread_mutex_lock(&pmtx);
    printQ(Q);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);
}
// Check if the system is in a safe state
bool is_safe_state(int *AVAILABLE, int **ALLOC, int **NEED, int m, int n, int *active_threads) {
    // Work array and finished array
    int work[m];
    bool finish[n];
    
    // Initialize work = available
    for (int i = 0; i < m; i++) {
        work[i] = AVAILABLE[i];
    }
    
    // Initialize finish array
    for (int i = 0; i < n; i++) {
        finish[i] = active_threads[i] == 0; // Inactive threads are already "finished"
    }
    
    // Find an unfinished thread that can complete
    bool found;
    do {
        found = false;
        for (int i = 0; i < n; i++) {
            if (!finish[i]) {
                // Check if thread i's needs can be satisfied
                bool can_satisfy = true;
                for (int j = 0; j < m; j++) {
                    if (NEED[i][j] > work[j]) {
                        can_satisfy = false;
                        break;
                    }
                }
                
                if (can_satisfy) {
                    // Thread can finish, release its resources
                    for (int j = 0; j < m; j++) {
                        work[j] += ALLOC[i][j];
                    }
                    finish[i] = true;
                    found = true;
                }
            }
        }
    } while (found);
    
    // System is safe if all active threads can finish
    for (int i = 0; i < n; i++) {
        if (active_threads[i] && !finish[i]) {
            return false;
        }
    }
    
    return true;
}

bool can_fulfill_req(int *request, int *NEED, int *AVAILABLE, int m) {
    // First check if request exceeds need or available
    for(int i = 0; i < m; i++){
        if(request[i] > NEED[i] || request[i] > AVAILABLE[i]){
            return false;
        }
    }

#ifdef _DLAVOID
    // Temporarily allocate resources to check safety
    int temp_available[m];
    int **temp_alloc = (int **)malloc(n * sizeof(int *));
    int **temp_need = (int **)malloc(n * sizeof(int *));
    
    // Make copies of allocation and need matrices
    for (int i = 0; i < n; i++) {
        temp_alloc[i] = (int *)malloc(m * sizeof(int));
        temp_need[i] = (int *)malloc(m * sizeof(int));
        
        for (int j = 0; j < m; j++) {
            temp_alloc[i][j] = ALLOC[i][j];
            temp_need[i][j] = NEED[i][j];
        }
    }
    
    // Make copy of available resources
    for (int i = 0; i < m; i++) {
        temp_available[i] = AVAILABLE[i] - request[i];
    }
    
    // Apply the request to the temporary state
    int thread_id = g_request->thread_id;
    for (int i = 0; i < m; i++) {
        temp_alloc[thread_id][i] += request[i];
        temp_need[thread_id][i] -= request[i];
    }
    
    // Check if resulting state is safe
    bool is_safe = is_safe_state(temp_available, temp_alloc, temp_need, m, n, active_threads);
    
    // Free temporary arrays
    for (int i = 0; i < n; i++) {
        free(temp_alloc[i]);
        free(temp_need[i]);
    }
    free(temp_alloc);
    free(temp_need);
    
    return is_safe;
#else
    return true;
#endif
}
