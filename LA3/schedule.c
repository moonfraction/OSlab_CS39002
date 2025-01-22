#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX_BURSTS 20
#define MAX_PROCESSES 1000
#define INF 1000000000

// Process states
typedef enum {
    STATE_NEW,
    STATE_READY,
    STATE_RUNNING,
    STATE_WAITING,
    STATE_TERMINATED
} ProcessState;

// Process Control Block structure
typedef struct {
    int id;
    int arrival_time;
    int num_bursts;
    int cpu_bursts[MAX_BURSTS];
    int io_bursts[MAX_BURSTS];
    int current_burst;
    int remaining_time;  // Remaining time in current CPU burst
    ProcessState state;
    int turnaround_time;
    int wait_time;
    int running_time;    // Sum of all CPU and IO bursts
} Process;

// Event types
typedef enum {
    EVENT_ARRIVAL,
    EVENT_CPU_FINISH,
    EVENT_IO_FINISH,
    EVENT_CPU_TIMEOUT
} EventType;

// Event structure
typedef struct {
    int time;
    int process_index;
    EventType type;
} Event;

// Global variables
Process processes[MAX_PROCESSES];
int n_processes;
Event event_heap[MAX_PROCESSES * MAX_BURSTS * 4];  // Increased size for all possible events
int heap_size = 0;
int ready_queue[MAX_PROCESSES];
int ready_front = 0, ready_rear = 0;
int current_time = 0;
int cpu_idle_time = 0;
int current_running_process = -1;

// Helper functions for min-heap
void swap_events(Event* a, Event* b) {
    Event temp = *a;
    *a = *b;
    *b = temp;
}

int compare_events(Event* a, Event* b) {
    if (a->time != b->time) 
        return a->time - b->time;
    
    // If times are equal, handle tie-breaking based on event types
    if (a->type != b->type) {
        // Arrival and IO completion have higher priority than timeout
        if ((a->type == EVENT_ARRIVAL || a->type == EVENT_IO_FINISH) && b->type == EVENT_CPU_TIMEOUT)
            return -1;
        if ((b->type == EVENT_ARRIVAL || b->type == EVENT_IO_FINISH) && a->type == EVENT_CPU_TIMEOUT)
            return 1;
    }
    
    // For same event types, break ties by process ID
    return processes[a->process_index].id - processes[b->process_index].id;
}

void heapify_up(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (compare_events(&event_heap[parent], &event_heap[index]) > 0) {
            swap_events(&event_heap[parent], &event_heap[index]);
            index = parent;
        } else {
            break;
        }
    }
}

void heapify_down(int index) {
    while (1) {
        int smallest = index;
        int left = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < heap_size && compare_events(&event_heap[left], &event_heap[smallest]) < 0)
            smallest = left;

        if (right < heap_size && compare_events(&event_heap[right], &event_heap[smallest]) < 0)
            smallest = right;

        if (smallest != index) {
            swap_events(&event_heap[index], &event_heap[smallest]);
            index = smallest;
        } else {
            break;
        }
    }
}

void insert_event(Event event) {
    event_heap[heap_size] = event;
    heapify_up(heap_size);
    heap_size++;
}

Event extract_min_event() {
    Event min_event = event_heap[0];
    heap_size--;
    if (heap_size > 0) {
        event_heap[0] = event_heap[heap_size];
        heapify_down(0);
    }
    return min_event;
}

// Ready queue operations
void enqueue_process(int process_index) {
    ready_queue[ready_rear] = process_index;
    ready_rear = (ready_rear + 1) % MAX_PROCESSES;
    processes[process_index].state = STATE_READY;
}

int dequeue_process() {
    if (ready_front == ready_rear) return -1;
    int process_index = ready_queue[ready_front];
    ready_front = (ready_front + 1) % MAX_PROCESSES;
    return process_index;
}

int is_ready_queue_empty() {
    return ready_front == ready_rear;
}

// Read input file
void read_input() {
    FILE* fp = fopen("proc.txt", "r");
    if (!fp) {
        printf("Error opening proc.txt\n");
        exit(1);
    }

    fscanf(fp, "%d", &n_processes);
    for (int i = 0; i < n_processes; i++) {
        Process* p = &processes[i];
        p->state = STATE_NEW;
        fscanf(fp, "%d %d", &p->id, &p->arrival_time);
        
        p->running_time = 0;
        int j = 0;
        while (1) {
            fscanf(fp, "%d", &p->cpu_bursts[j]);
            p->running_time += p->cpu_bursts[j];
            fscanf(fp, "%d", &p->io_bursts[j]);
            if (p->io_bursts[j] == -1) break;
            p->running_time += p->io_bursts[j];
            j++;
        }
        p->num_bursts = j + 1;
        p->current_burst = 0;
        p->remaining_time = p->cpu_bursts[0];
        p->wait_time = 0;
    }
    fclose(fp);
}

// Schedule next process
void schedule_process(int quantum) {
    if (current_running_process != -1 || is_ready_queue_empty()) return;
    
    int process_index = dequeue_process();
    current_running_process = process_index;
    Process* p = &processes[process_index];
    p->state = STATE_RUNNING;
    
    int run_time = quantum < p->remaining_time ? quantum : p->remaining_time;
    
    Event next_event;
    next_event.process_index = process_index;
    next_event.time = current_time + run_time;
    
    if (run_time == p->remaining_time) {
        next_event.type = EVENT_CPU_FINISH;
    } else {
        next_event.type = EVENT_CPU_TIMEOUT;
    }
    
    #ifdef VERBOSE
    printf("%d : Process %d is scheduled to run for time %d\n",
           current_time, p->id, run_time);
    #endif
    
    insert_event(next_event);
}

void simulate(int quantum) {
    printf("**** %s Scheduling %s ****\n",
           quantum == INF ? "FCFS" : "RR",
           quantum == INF ? "" : quantum == 10 ? "with q = 10" : "with q = 5");

    #ifdef VERBOSE
    printf("0 : Starting\n");
    #endif

    // Initialize simulation
    current_time = 0;
    cpu_idle_time = 0;
    heap_size = 0;
    ready_front = ready_rear = 0;
    current_running_process = -1;
    
    // Reset process states
    for (int i = 0; i < n_processes; i++) {
        Process* p = &processes[i];
        p->current_burst = 0;
        p->remaining_time = p->cpu_bursts[0];
        p->wait_time = 0;
        p->turnaround_time = 0;
        p->state = STATE_NEW;
        
        // Add arrival event
        Event arrival = {p->arrival_time, i, EVENT_ARRIVAL};
        insert_event(arrival);
    }

    int last_event_time = 0;
    int last_busy_time = 0;

    // Main simulation loop
    while (heap_size > 0) {
        Event event = extract_min_event();
        
        // Update CPU idle time
        if (current_running_process == -1) {
            cpu_idle_time += event.time - current_time;
        }
        
        current_time = event.time;
        Process* p = &processes[event.process_index];

        switch (event.type) {
            case EVENT_ARRIVAL:
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue upon arrival\n",
                       current_time, p->id);
                #endif
                enqueue_process(event.process_index);
                break;

            case EVENT_CPU_FINISH:
                current_running_process = -1;
                p->current_burst++;
                
                if (p->current_burst == p->num_bursts) {
                    // Process completed
                    p->state = STATE_TERMINATED;
                    p->turnaround_time = current_time - p->arrival_time;
                    p->wait_time = p->turnaround_time - p->running_time;
                    
                    printf("%d : Process %d exits. Turnaround time = %d (%d%%), Wait time = %d\n",
                           current_time, p->id,
                           p->turnaround_time,
                           (p->turnaround_time * 100) / p->running_time,
                           p->wait_time);
                    
                    #ifdef VERBOSE
                    printf("%d : CPU goes idle\n", current_time);
                    #endif
                } else {
                    // Schedule IO burst
                    p->state = STATE_WAITING;
                    Event io_finish = {
                        current_time + p->io_bursts[p->current_burst - 1],
                        event.process_index,
                        EVENT_IO_FINISH
                    };
                    insert_event(io_finish);
                }
                break;

            case EVENT_IO_FINISH:
                p->remaining_time = p->cpu_bursts[p->current_burst];
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue after IO completion\n",
                       current_time, p->id);
                #endif
                enqueue_process(event.process_index);
                break;

            case EVENT_CPU_TIMEOUT:
                current_running_process = -1;
                p->remaining_time -= quantum;
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue after timeout\n",
                       current_time, p->id);
                #endif
                enqueue_process(event.process_index);
                break;
        }

        schedule_process(quantum);
        last_event_time = current_time;
        if (current_running_process != -1) {
            last_busy_time = current_time;
        }
    }

    // Print statistics
    double avg_wait_time = 0;
    for (int i = 0; i < n_processes; i++) {
        avg_wait_time += processes[i].wait_time;
    }
    avg_wait_time /= n_processes;

    printf("Average wait time = %.2f\n", avg_wait_time);
    printf("Total turnaround time = %d\n", last_event_time);
    printf("CPU idle time = %d\n", cpu_idle_time);
    printf("CPU utilization = %.2f%%\n", 
           (100.0 * (last_event_time - cpu_idle_time)) / last_event_time);
    printf("\n");
}

int main() {
    read_input();
    
    // FCFS scheduling (RR with infinite quantum)
    simulate(INF);
    
    // RR scheduling with quantum = 10
    simulate(10);
    
    // RR scheduling with quantum = 5 
    simulate(5);
    
    return 0;
}