#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define BURST_LIMIT 25
#define PROC_LIMIT 1500
#define TIME_INFINITY 1000000000

// Process states
typedef enum {
    PROC_INIT,
    PROC_QUEUED,
    PROC_ACTIVE,
    PROC_BLOCKED,
    PROC_DONE
} ProcStatus;

// PCB
typedef struct {
    int task_id;
    int start_time;
    int burst_count;
    int compute_times[BURST_LIMIT];
    int wait_times[BURST_LIMIT];
    int burst_index;
    int time_left;
    ProcStatus status;
    int completion_time;
    int queue_time;
    int activity_time;
} Task;

// Events
typedef enum {
    EVT_START,
    EVT_COMPLETE,
    EVT_UNBLOCK,
    EVT_PREEMPT
} EventCategory;

// Event structure
typedef struct {
    int timestamp;
    int task_index;  // Index into process-info table
    EventCategory category;
} SchedulerEvent;

// Global state
Task task_list[PROC_LIMIT];  // Process-info table
int task_count;
SchedulerEvent event_queue[PROC_LIMIT * BURST_LIMIT * 4];
int queue_size = 0;

// Ready queue --> circular array
int ready_queue[PROC_LIMIT];  // Stores indices into process-info table
int ready_front = 0;
int ready_rear = 0;

// System state
int system_time = 0;
int idle_periods = 0;
int running_task = -1;  // Index into process-info table

// Ready queue ops
int is_ready_queue_empty() {
    return ready_front == ready_rear;
}

int is_ready_queue_full() {
    return (ready_rear + 1) % PROC_LIMIT == ready_front;
}

void ready_queue_enqueue(int task_index) {
    if (is_ready_queue_full()) {
        printf("Error: Ready queue overflow\n");
        exit(1);
    }
    ready_queue[ready_rear] = task_index;
    ready_rear = (ready_rear + 1) % PROC_LIMIT;
    task_list[task_index].status = PROC_QUEUED;
}

int ready_queue_dequeue() {
    if (is_ready_queue_empty()) {
        return -1;
    }
    int task_index = ready_queue[ready_front];
    ready_front = (ready_front + 1) % PROC_LIMIT;
    return task_index;
}

// Event queue ops
void swap_events(SchedulerEvent* a, SchedulerEvent* b) {
    SchedulerEvent temp = *a;
    *a = *b;
    *b = temp;
}

int compare_events(SchedulerEvent* a, SchedulerEvent* b) {
    if (a->timestamp != b->timestamp) 
        return a->timestamp - b->timestamp;
    
    if (a->category != b->category) {
        // Give priority to arrivals and unblocks over preemptions
        if ((a->category == EVT_START || a->category == EVT_UNBLOCK) && 
            b->category == EVT_PREEMPT)
            return -1;
        if ((b->category == EVT_START || b->category == EVT_UNBLOCK) && 
            a->category == EVT_PREEMPT)
            return 1;
    }
    
    // Break ties using process ID
    return task_list[a->task_index].task_id - task_list[b->task_index].task_id;
}

void event_queue_push(SchedulerEvent evt) {
    if (queue_size >= PROC_LIMIT * BURST_LIMIT * 4) {
        printf("Error: Event queue overflow\n");
        exit(1);
    }
    
    int pos = queue_size++;
    event_queue[pos] = evt;
    
    // Bubble up
    while (pos > 0) {
        int parent = (pos - 1) / 2;
        if (compare_events(&event_queue[parent], &event_queue[pos]) > 0) {
            swap_events(&event_queue[parent], &event_queue[pos]);
            pos = parent;
        } else {
            break;
        }
    }
}

SchedulerEvent event_queue_pop() {
    if (queue_size == 0) {
        printf("Error: Event queue underflow\n");
        exit(1);
    }
    
    SchedulerEvent result = event_queue[0];
    queue_size--;
    
    if (queue_size > 0) {
        event_queue[0] = event_queue[queue_size];
        
        // Sink down
        int pos = 0;
        while (1) {
            int min_pos = pos;
            int left = 2 * pos + 1;
            int right = 2 * pos + 2;
            
            if (left < queue_size && 
                compare_events(&event_queue[left], &event_queue[min_pos]) < 0)
                min_pos = left;
                
            if (right < queue_size && 
                compare_events(&event_queue[right], &event_queue[min_pos]) < 0)
                min_pos = right;
                
            if (min_pos == pos)
                break;
                
            swap_events(&event_queue[pos], &event_queue[min_pos]);
            pos = min_pos;
        }
    }
    
    return result;
}

// Task initialization
void initialize_tasks() {
    FILE* input = fopen("proc.txt", "r");
    if (!input) {
        printf("Failed to open proc.txt\n");
        exit(1);
    }

    fscanf(input, "%d", &task_count);
    for (int i = 0; i < task_count; i++) {
        Task* t = &task_list[i];
        t->status = PROC_INIT;
        fscanf(input, "%d %d", &t->task_id, &t->start_time);
        
        t->activity_time = 0;
        int j = 0;
        while (1) {
            fscanf(input, "%d", &t->compute_times[j]);
            t->activity_time += t->compute_times[j];
            fscanf(input, "%d", &t->wait_times[j]);
            if (t->wait_times[j] == -1) break;
            t->activity_time += t->wait_times[j];
            j++;
        }
        t->burst_count = j + 1;
        t->burst_index = 0;
        t->time_left = t->compute_times[0];
        t->queue_time = 0;
    }
    fclose(input);
}

void check_idle_state() {
    #ifdef VERBOSE
    if (running_task == -1 && is_ready_queue_empty()) {
        printf("%d : CPU goes idle\n", system_time);
    }
    #endif
}

void schedule_next_task(int quantum) {
    if (running_task != -1 || is_ready_queue_empty()) 
        return;
    
    int next_task = ready_queue_dequeue();
    running_task = next_task;
    Task* t = &task_list[next_task];
    t->status = PROC_ACTIVE;
    
    int duration = quantum < t->time_left ? quantum : t->time_left;
    
    SchedulerEvent next_evt = {
        .timestamp = system_time + duration,
        .task_index = next_task,
        .category = (duration == t->time_left) ? EVT_COMPLETE : EVT_PREEMPT
    };
    
    #ifdef VERBOSE
    printf("%d : Process %d is scheduled to run for time %d\n",
           system_time, t->task_id, duration);
    #endif
    
    event_queue_push(next_evt);
}

void run_scheduler(int quantum) {
    printf("**** %s Scheduling %s ****\n",
           quantum == TIME_INFINITY ? "FCFS" : "RR",
           quantum == TIME_INFINITY ? "" : quantum == 10 ? "with q = 10" : "with q = 5");

    #ifdef VERBOSE
    printf("0 : Starting\n");
    #endif

    // Initialize system state
    system_time = 0;
    idle_periods = 0;
    queue_size = 0;
    ready_front = ready_rear = 0;
    running_task = -1;
    
    // Schedule initial arrivals
    for (int i = 0; i < task_count; i++) {
        Task* t = &task_list[i];
        t->burst_index = 0;
        t->time_left = t->compute_times[0];
        t->queue_time = 0;
        t->completion_time = 0;
        t->status = PROC_INIT;
        
        SchedulerEvent evt = {
            .timestamp = t->start_time,
            .task_index = i,
            .category = EVT_START
        };
        event_queue_push(evt);
    }

    int final_time = 0;

    // Main event loop
    while (queue_size > 0) {
        SchedulerEvent evt = event_queue_pop();
        
        if (running_task == -1) {
            idle_periods += evt.timestamp - system_time;
        }
        
        system_time = evt.timestamp;
        Task* t = &task_list[evt.task_index];

        switch (evt.category) {
            case EVT_START:
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue upon arrival\n",
                       system_time, t->task_id);
                #endif
                ready_queue_enqueue(evt.task_index);
                break;

            case EVT_COMPLETE:
                running_task = -1;
                t->burst_index++;
                
                if (t->burst_index == t->burst_count) {
                    t->status = PROC_DONE;
                    t->completion_time = system_time - t->start_time;
                    t->queue_time = t->completion_time - t->activity_time;
                    
                    printf("%d : Process %d exits. Turnaround time = %d (%d%%), Wait time = %d\n",
                           system_time, t->task_id,
                           t->completion_time,
                           (t->completion_time * 100) / t->activity_time,
                           t->queue_time);
                } else {
                    t->status = PROC_BLOCKED;
                    SchedulerEvent unblock = {
                        .timestamp = system_time + t->wait_times[t->burst_index - 1],
                        .task_index = evt.task_index,
                        .category = EVT_UNBLOCK
                    };
                    event_queue_push(unblock);
                }
                check_idle_state();
                break;

            case EVT_UNBLOCK:
                t->time_left = t->compute_times[t->burst_index];
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue after IO completion\n",
                       system_time, t->task_id);
                #endif
                ready_queue_enqueue(evt.task_index);
                break;

            case EVT_PREEMPT:
                running_task = -1;
                t->time_left -= quantum;
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue after timeout\n",
                       system_time, t->task_id);
                #endif
                ready_queue_enqueue(evt.task_index);
                break;
        }

        schedule_next_task(quantum);
        final_time = system_time;
    }

    // Print statistics
    double avg_wait = 0;
    for (int i = 0; i < task_count; i++) {
        avg_wait += task_list[i].queue_time;
    }
    avg_wait /= task_count;

    printf("Average wait time = %.2f\n", avg_wait);
    printf("Total turnaround time = %d\n", final_time);
    printf("CPU idle time = %d\n", idle_periods);
    printf("CPU utilization = %.2f%%\n", 
           (100.0 * (final_time - idle_periods)) / final_time);
    printf("\n");
}

int main() {
    initialize_tasks();
    run_scheduler(TIME_INFINITY);  // FCFS
    run_scheduler(10);            // RR with q=10
    // run_scheduler(5);             // RR with q=5
    return 0;
}