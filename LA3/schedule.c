#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX_BURSTS 20  // Maximum number of bursts per process
#define MAX_PROCESSES 1000  // Maximum number of processes
#define INF 1000000000  // Used for FCFS scheduling

// Process Control Block structure
typedef struct {
    int id;
    int arrival_time;
    int num_bursts;
    int cpu_bursts[MAX_BURSTS];
    int io_bursts[MAX_BURSTS];
    int current_burst;
    int remaining_time;  // Remaining time in current CPU burst
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
Event event_heap[MAX_PROCESSES * MAX_BURSTS];
int heap_size = 0;
int ready_queue[MAX_PROCESSES];
int ready_front = 0, ready_rear = 0;
int current_time = 0;
int cpu_idle_time = 0;
Process* running_process = NULL;

// Helper functions for min-heap (event queue)
void swap_events(Event* a, Event* b) {
    Event temp = *a;
    *a = *b;
    *b = temp;
}

void heapify_up(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (event_heap[parent].time > event_heap[index].time ||
            (event_heap[parent].time == event_heap[index].time &&
             processes[event_heap[parent].process_index].id > processes[event_heap[index].process_index].id)) {
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

        if (left < heap_size &&
            (event_heap[left].time < event_heap[smallest].time ||
             (event_heap[left].time == event_heap[smallest].time &&
              processes[event_heap[left].process_index].id < processes[event_heap[smallest].process_index].id)))
            smallest = left;

        if (right < heap_size &&
            (event_heap[right].time < event_heap[smallest].time ||
             (event_heap[right].time == event_heap[smallest].time &&
              processes[event_heap[right].process_index].id < processes[event_heap[smallest].process_index].id)))
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
    event_heap[0] = event_heap[heap_size];
    if (heap_size > 0) {
        heapify_down(0);
    }
    return min_event;
}

// Ready queue operations
void enqueue_process(int process_index) {
    ready_queue[ready_rear] = process_index;
    ready_rear = (ready_rear + 1) % MAX_PROCESSES;
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

// Read input file and initialize processes
void read_input() {
    FILE* fp = fopen("proc.txt", "r");
    if (!fp) {
        printf("Error opening proc.txt\n");
        exit(1);
    }

    fscanf(fp, "%d", &n_processes);
    for (int i = 0; i < n_processes; i++) {
        Process* p = &processes[i];
        fscanf(fp, "%d %d", &p->id, &p->arrival_time);
        
        int j = 0;
        while (1) {
            fscanf(fp, "%d", &p->cpu_bursts[j]);
            fscanf(fp, "%d", &p->io_bursts[j]);
            if (p->io_bursts[j] == -1) break;
            j++;
        }
        p->num_bursts = j + 1;
        p->current_burst = 0;
        p->remaining_time = p->cpu_bursts[0];
        p->running_time = 0;
        
        // Calculate total running time
        for (int k = 0; k < p->num_bursts; k++) {
            p->running_time += p->cpu_bursts[k];
            if (k < p->num_bursts - 1) {
                p->running_time += p->io_bursts[k];
            }
        }
        
        // Add arrival event
        Event arrival_event = {p->arrival_time, i, EVENT_ARRIVAL};
        insert_event(arrival_event);
    }
    fclose(fp);
}

// Schedule next process on CPU
void schedule_process(int quantum) {
    if (running_process != NULL || is_ready_queue_empty()) return;
    
    int process_index = dequeue_process();
    running_process = &processes[process_index];
    
    int run_time = quantum < running_process->remaining_time ? 
                   quantum : running_process->remaining_time;
    
    Event next_event;
    next_event.process_index = process_index;
    next_event.time = current_time + run_time;
    
    if (run_time == running_process->remaining_time) {
        next_event.type = EVENT_CPU_FINISH;
    } else {
        next_event.type = EVENT_CPU_TIMEOUT;
    }
    
    #ifdef VERBOSE
    printf("%d : Process %d is scheduled to run for time %d\n",
           current_time, running_process->id, run_time);
    #endif
    
    insert_event(next_event);
}

// Main simulation function
void simulate(int quantum) {
    printf("**** %s Scheduling %s ****\n",
           quantum == INF ? "FCFS" : "RR",
           quantum == INF ? "" : "with q = 5");

    #ifdef VERBOSE
    printf("0 : Starting\n");
    #endif

    current_time = 0;
    cpu_idle_time = 0;
    heap_size = 0;
    ready_front = ready_rear = 0;
    running_process = NULL;
    
    // Reset process states
    for (int i = 0; i < n_processes; i++) {
        processes[i].current_burst = 0;
        processes[i].remaining_time = processes[i].cpu_bursts[0];
        processes[i].wait_time = 0;
        processes[i].turnaround_time = 0;
        Event arrival_event = {processes[i].arrival_time, i, EVENT_ARRIVAL};
        insert_event(arrival_event);
    }

    int completed_processes = 0;
    int last_event_time = 0;

    while (heap_size > 0) {
        Event event = extract_min_event();
        
        // Update CPU idle time
        if (running_process == NULL) {
            cpu_idle_time += event.time - current_time;
        }
        
        current_time = event.time;
        Process* process = &processes[event.process_index];

        switch (event.type) {
            case EVENT_ARRIVAL:
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue upon arrival\n",
                       current_time, process->id);
                #endif
                enqueue_process(event.process_index);
                break;

            case EVENT_CPU_FINISH:
                running_process = NULL;
                process->current_burst++;
                
                if (process->current_burst == process->num_bursts) {
                    // Process completed
                    process->turnaround_time = current_time - process->arrival_time;
                    process->wait_time = process->turnaround_time - process->running_time;
                    completed_processes++;
                    
                    printf("%d : Process %d exits. Turnaround time = %d (%d%%), Wait time = %d\n",
                           current_time, process->id,
                           process->turnaround_time,
                           (process->turnaround_time * 100) / process->running_time,
                           process->wait_time);
                    
                    #ifdef VERBOSE
                    printf("%d : CPU goes idle\n", current_time);
                    #endif
                } else {
                    // Schedule IO burst
                    Event io_finish = {
                        current_time + process->io_bursts[process->current_burst - 1],
                        event.process_index,
                        EVENT_IO_FINISH
                    };
                    insert_event(io_finish);
                }
                break;

            case EVENT_IO_FINISH:
                process->remaining_time = process->cpu_bursts[process->current_burst];
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue after IO completion\n",
                       current_time, process->id);
                #endif
                enqueue_process(event.process_index);
                break;

            case EVENT_CPU_TIMEOUT:
                running_process = NULL;
                process->remaining_time -= quantum;
                #ifdef VERBOSE
                printf("%d : Process %d joins ready queue after timeout\n",
                       current_time, process->id);
                #endif
                enqueue_process(event.process_index);
                break;
        }

        schedule_process(quantum);
        last_event_time = current_time;
    }

    // Print final statistics
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
}

int main() {
    read_input();
    
    // Simulate FCFS (RR with infinite quantum)
    simulate(INF);
    printf("\n");
    
    // Simulate RR with quantum = 10
    simulate(10);
    printf("\n");
    
    // Simulate RR with quantum = 5
    simulate(5);
    
    return 0;
}