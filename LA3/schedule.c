#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define BURST_LIMIT 25
#define PROC_LIMIT 1500
#define TIME_INFINITY 1000000000

typedef enum {
    PROC_INIT,
    PROC_QUEUED,
    PROC_ACTIVE,
    PROC_BLOCKED,
    PROC_DONE
} ProcStatus;

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

typedef enum {
    EVT_START,
    EVT_COMPLETE,
    EVT_UNBLOCK,
    EVT_PREEMPT
} EventCategory;

typedef struct {
    int timestamp;
    int task_index;
    EventCategory category;
} SchedulerEvent;

// System state variables
Task task_list[PROC_LIMIT];
int task_count;
SchedulerEvent event_queue[PROC_LIMIT * BURST_LIMIT * 4];
int queue_size = 0;
int ready_list[PROC_LIMIT];
int list_head = 0, list_tail = 0;
int system_time = 0;
int idle_periods = 0;
int running_task = -1;

// Event queue management
void exchange_events(SchedulerEvent* first, SchedulerEvent* second) {
    SchedulerEvent temp = *first;
    *first = *second;
    *second = temp;
}

int prioritize_events(SchedulerEvent* first, SchedulerEvent* second) {
    if (first->timestamp != second->timestamp) 
        return first->timestamp - second->timestamp;
    
    if (first->category != second->category) {
        if ((first->category == EVT_START || first->category == EVT_UNBLOCK) && 
            second->category == EVT_PREEMPT)
            return -1;
        if ((second->category == EVT_START || second->category == EVT_UNBLOCK) && 
            first->category == EVT_PREEMPT)
            return 1;
    }
    
    return task_list[first->task_index].task_id - task_list[second->task_index].task_id;
}

void bubble_up(int position) {
    while (position > 0) {
        int parent = (position - 1) / 2;
        if (prioritize_events(&event_queue[parent], &event_queue[position]) > 0) {
            exchange_events(&event_queue[parent], &event_queue[position]);
            position = parent;
        } else {
            break;
        }
    }
}

void sink_down(int position) {
    while (1) {
        int min_pos = position;
        int left_child = 2 * position + 1;
        int right_child = 2 * position + 2;

        if (left_child < queue_size && 
            prioritize_events(&event_queue[left_child], &event_queue[min_pos]) < 0)
            min_pos = left_child;

        if (right_child < queue_size && 
            prioritize_events(&event_queue[right_child], &event_queue[min_pos]) < 0)
            min_pos = right_child;

        if (min_pos != position) {
            exchange_events(&event_queue[position], &event_queue[min_pos]);
            position = min_pos;
        } else {
            break;
        }
    }
}

void schedule_event(SchedulerEvent evt) {
    event_queue[queue_size] = evt;
    bubble_up(queue_size);
    queue_size++;
}

SchedulerEvent get_next_event() {
    SchedulerEvent next = event_queue[0];
    queue_size--;
    if (queue_size > 0) {
        event_queue[0] = event_queue[queue_size];
        sink_down(0);
    }
    return next;
}

// Ready list ops
void append_task(int task_index) {
    ready_list[list_tail] = task_index;
    list_tail = (list_tail + 1) % PROC_LIMIT;
    task_list[task_index].status = PROC_QUEUED;
}

int remove_next_task() {
    if (list_head == list_tail) return -1;
    int task_index = ready_list[list_head];
    list_head = (list_head + 1) % PROC_LIMIT;
    return task_index;
}

int is_list_empty() {
    return list_head == list_tail;
}

// Task data initialization
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

// Task scheduling
void select_next_task(int quantum) {
    if (running_task != -1 || is_list_empty()) return;
    
    int next_task = remove_next_task();
    running_task = next_task;
    Task* t = &task_list[next_task];
    t->status = PROC_ACTIVE;
    
    int duration = quantum < t->time_left ? quantum : t->time_left;
    
    SchedulerEvent next_evt;
    next_evt.task_index = next_task;
    next_evt.timestamp = system_time + duration;
    next_evt.category = (duration == t->time_left) ? EVT_COMPLETE : EVT_PREEMPT;
    
    #ifdef VERBOSE
    printf("%d : Process %d is scheduled to run for time %d\n",
           system_time, t->task_id, duration);
    #endif
    
    schedule_event(next_evt);
}

void check_idle_state() {
    #ifdef VERBOSE
    if (running_task == -1 && is_list_empty()) {
        printf("%d : CPU goes idle\n", system_time);
    }
    #endif
}

// simulation
void run_scheduler(int quantum) {
    printf("**** %s Scheduling %s ****\n",
           quantum == TIME_INFINITY ? "FCFS" : "RR",
           quantum == TIME_INFINITY ? "" : quantum == 10 ? "with q = 10" : "with q = 5");

    #ifdef VERBOSE
    printf("0 : Starting\n");
    #endif

    system_time = 0;
    idle_periods = 0;
    queue_size = 0;
    list_head = list_tail = 0;
    running_task = -1;
    
    for (int i = 0; i < task_count; i++) {
        Task* t = &task_list[i];
        t->burst_index = 0;
        t->time_left = t->compute_times[0];
        t->queue_time = 0;
        t->completion_time = 0;
        t->status = PROC_INIT;
        
        SchedulerEvent evt = {t->start_time, i, EVT_START};
        schedule_event(evt);
    }

    int final_time = 0;

    while (queue_size > 0) {
        SchedulerEvent evt = get_next_event();
        
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
                append_task(evt.task_index);
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
                        system_time + t->wait_times[t->burst_index - 1],
                        evt.task_index,
                        EVT_UNBLOCK
                    };
                    schedule_event(unblock);
                }
                check_idle_state();
                break;

            case EVT_UNBLOCK:
                t->time_left = t->compute_times[t->burst_index];
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue after IO completion\n",
                       system_time, t->task_id);
                #endif
                append_task(evt.task_index);
                break;

            case EVT_PREEMPT:
                running_task = -1;
                t->time_left -= quantum;
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue after timeout\n",
                       system_time, t->task_id);
                #endif
                append_task(evt.task_index);
                break;
        }

        select_next_task(quantum);
        final_time = system_time;
    }

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
    run_scheduler(5);             // RR with q=5
    return 0;
}