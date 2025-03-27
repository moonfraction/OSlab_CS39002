#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>
using namespace std;

// Constants
const int PAGE_SIZE = 4096;                         // 4 KB
const int TOTAL_MEMORY = 64 * 1024 * 1024;          // 64 MB
const int OS_MEMORY = 16 * 1024 * 1024;             // 16 MB
const int USER_MEMORY = 48 * 1024 * 1024;           // 48 MB
const int TOTAL_FRAMES = TOTAL_MEMORY / PAGE_SIZE;  // 16384 frames
const int USER_FRAMES = USER_MEMORY / PAGE_SIZE;    // 12288 frames
const int OS_FRAMES = OS_MEMORY / PAGE_SIZE;        // 4096 frames
const int INTS_PER_PAGE = PAGE_SIZE / 4;            // 1024 integers per page
const int PAGE_TABLE_ENTRIES = 2048;                // per process page table entries
const int ESSENTIAL_PAGES = 10;                     // first 10 pages are essential

// Bit mask macros for page table entry (16 bits: MSB = valid flag, lower 15 bits = frame number)
inline bool isValid(uint16_t entry) {
    return (entry & 0x8000) != 0;
}
inline uint16_t makeEntry(int frame) {
    return (uint16_t)((frame & 0x7FFF) | 0x8000);
}
inline void invalidate(uint16_t &entry) {
    entry = 0;
}

// Process structure
struct Process {
    int pid;                                        // process id
    int s;                                          // size of array A (number of integers)
    int m;                                          // number of searches to perform
    int *keys;                                      // search keys (each search key is an index in A)
    int currentSearch;                              // index of next search to perform
    uint16_t *pt;                                   // page table of size PAGE_TABLE_ENTRIES (each entry is 16-bit)
};

void initproc(Process *proc, int id, int size, int searches) {
    proc->pid = id;
    proc->s = size;
    proc->m = searches;
    proc->currentSearch = 0;
    proc->keys = (int *)malloc(searches * sizeof(int));
    proc->pt = (uint16_t *)malloc(PAGE_TABLE_ENTRIES * sizeof(uint16_t));
    if (proc->keys == NULL || proc->pt == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
}

// Global kernel data
queue<int> readyQ;                                  // holds process ids of active processes (round robin)
queue<int> swappedQ;                                // FIFO queue of swapped-out processes (store process id)
int *freeFrames;                                    // free frame list (store frame numbers)
Process **processes;                                // all processes indexed by pid

// Statistics
int cntff = 0;                                      // count of free frames
uint64_t pageAccesses = 0;
uint64_t pageFaults = 0;
uint64_t swapCount = 0;
int activeProcesses = 0;                            // count of processes in memory (not swapped out)
int minActiveProcesses = 1000000;                   // track minimum active processes when memory full
int totalProcesses = 0;                             // n
int searchesPerProcess = 0;                         // m

// Function to allocate an essential page for a process
// This function allocates a free frame and assigns it to the given virtual page.
// Returns true if allocation succeeded.
bool allocateFrame(Process *proc, int vpage) {
    if(cntff == 0) {
        return false;
    }
    cntff--;

    int frame = freeFrames[cntff];
    proc->pt[vpage] = makeEntry(frame);
    return true;
}

// Allocate all ESSENTIAL_PAGES for process (virtual pages 0 to ESSENTIAL_PAGES-1)
bool allocateEssentialPages(Process *proc) {
    for (int vp = 0; vp < ESSENTIAL_PAGES; vp++) {
        if (!allocateFrame(proc, vp))
            return false;
    }
    return true;
}

// Free all frames allocated to process (both essential and additional pages)
// Walk through page table; if valid, free the frame and invalidate the entry.
void freeProcessFrames(Process *proc) {
    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        if (isValid(proc->pt[i])) {
            int frame = proc->pt[i] & 0x7FFF;
            freeFrames[cntff++] = frame;
            invalidate(proc->pt[i]);
        }
    }
}

// Simulate a binary search for process proc searching for key (k)
// The array A is conceptual: A[i] = i and stored starting at virtual page ESSENTIAL_PAGES.
// Returns true if search completes; false if a swap-out occurs due to no free frame.
bool simulateBinarySearch(Process *proc, int k) {
    int L = 0;
    int R = proc->s - 1;
    while (L < R) {
        int M = (L + R) / 2;
        // Determine which virtual page in A is being accessed.
        // Array A is mapped to virtual pages starting at ESSENTIAL_PAGES.
        int offset = M / INTS_PER_PAGE; 
        int vpage = ESSENTIAL_PAGES + offset;
        pageAccesses++;
        if (!isValid(proc->pt[vpage])) {
            // Page fault occurs: try to allocate a free frame.
            pageFaults++;
            if (cntff == 0) {
                // No free frame available: need to swap out this process.
                return false;
            } else {
                // Allocate a free frame and update the page table entry.
                if(!allocateFrame(proc, vpage)) {
                    // This should not happen as we already checked freeFrames.
                    return false;
                }
            }
        }
        // Simulate the access by evaluating the condition:
        // if (k <= A[M])  => since A[M] = M, compare k and M.
        if (k <= M)
            R = M;
        else
            L = M + 1;
    }
    return true;
}

// Swap out process proc:
// Free all frames allocated to it, mark it as swapped out, and add it to swappedQ.
// Print swap-out message and update swap count and active process count.
void swapOut(Process *proc) {
    swapCount++;
    freeProcessFrames(proc);
    activeProcesses--;
    // Update minimum active processes when memory is full.
    if (activeProcesses < minActiveProcesses)
        minActiveProcesses = activeProcesses;
    // Print swap-out message (non-verbose mode prints only swap messages)
    printf("+++ Swapping out process %4d [%d active processes]\n", proc->pid, activeProcesses);
    swappedQ.push(proc->pid);
}

// Swap in process proc:
// Allocate only the essential pages. (The additional pages for A will be reloaded on demand.)
// Print swap-in message.
bool swapIn(Process *proc) {
    // Allocate essential pages (virtual pages 0 to ESSENTIAL_PAGES-1)
    if(!allocateEssentialPages(proc)) {
        // This should not happen if frames were freed properly.
        return false;
    }
    printf("+++ Swapping in process %4d [%d active processes]\n", proc->pid, activeProcesses+1);

    return true;
}

void terminateProcess(Process *proc) {
    freeProcessFrames(proc);
    activeProcesses--;
}

bool swapInNextProcess() {
    if (swappedQ.empty()) {
        return false; // No processes to swap in
    }
    
    int pidToSwap = swappedQ.front();
    swappedQ.pop();
    Process* proc = processes[pidToSwap];
    
    if (!swapIn(proc)) {
        cerr << "Error during swap-in." << endl;
        return false;
    }
    activeProcesses++;
    
    // Immediately perform a search after swapping in
    int key = proc->keys[proc->currentSearch];
    
    #ifdef VERBOSE
    printf("\tSearch %d by Process %d\n", proc->currentSearch+1, proc->pid);
    #endif
    
    bool success = simulateBinarySearch(proc, key);
    
    if (!success) {
        // This is rare but possible if we run out of frames again
        swapOut(proc);
    } else {
        // Search completed successfully
        proc->currentSearch++;
        
        // Check if process has more searches
        if (proc->currentSearch < proc->m) {
            readyQ.push(proc->pid); // Requeue for more searches
        } else {
            // Process finished all searches
            terminateProcess(proc);
            return swapInNextProcess(); // Try to swap in another one
        }
    }
    
    return true;
}


int main() {
    // initially all frames are free
    freeFrames = (int *)malloc(USER_FRAMES * sizeof(int));
    if (freeFrames == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    for (int i = 0; i < USER_FRAMES; i++) {
        freeFrames[i] = i;
    }
    cntff = USER_FRAMES;

    // Read input from search.txt
    FILE *fin = fopen("search.txt", "r");
    if (!fin) {
        perror("search.txt");
        return 1;
    }

    fscanf(fin, "%d %d", &totalProcesses, &searchesPerProcess);

    // Creating processes and initializing their page tables
    processes = (Process **)malloc(totalProcesses * sizeof(Process *));
    if (processes == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    for (int i = 0; i < totalProcesses; i++) {
        int s;
        fscanf(fin, "%d", &s);
        Process *proc = (Process *)malloc(sizeof(Process));
        if (proc == NULL) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            exit(1);
        }
        initproc(proc, i, s, searchesPerProcess);
        
        // Read m search keys
        for (int j = 0; j < searchesPerProcess; j++) {
            int key;
            fscanf(fin, "%d", &key);
            proc->keys[j] = key;
        }
        
        // Allocate essential pages (pages 0 to ESSENTIAL_PAGES-1)
        if (!allocateEssentialPages(proc)) {
            fprintf(stderr, "Error: Not enough free frames to allocate essential pages for process %d\n", i);
            return 1;
        }
        processes[i] = proc;
        readyQ.push(i);
        activeProcesses++;
    }
    fclose(fin);

    printf("+++ Simulation data read from file\n");
    printf("+++ Kernel data initialized\n");

// #ifdef VERBOSE
//     printf("--> Running in VERBOSE mode\n");
// #endif

    int completedCount = 0;
    
    while (completedCount < totalProcesses) {
        // If readyQ is empty but there are swapped-out processes waiting, swap one in
        if (readyQ.empty()) {
            if (!swapInNextProcess()) {
                break; // No more processes to run
            }
        }
        
        int pid = readyQ.front();
        readyQ.pop();
        Process *proc = processes[pid];


        // If the process has finished all its searches, terminate it
        if (proc->currentSearch >= proc->m) {
            terminateProcess(proc);
            completedCount++;
            // Try to swap in another process
            swapInNextProcess();
            continue;
        }
        
        // Simulate one binary search for the current search key
        int key = proc->keys[proc->currentSearch];

        #ifdef VERBOSE
        printf("\tSearch %d by Process %d\n", proc->currentSearch+1, pid);
        #endif

        bool success = simulateBinarySearch(proc, key);
        
        if (!success) {
            // Swap out the process because free frame list was empty
            swapOut(proc);
            // The search will be restarted after swap-in
        } else {
            // Binary search finished successfully 
            proc->currentSearch++; // move to next search
            
            // Check if process has more searches
            if (proc->currentSearch < proc->m) {
                readyQ.push(pid); // Requeue for more searches
            } else {
                // Process finished all searches
                terminateProcess(proc);
                completedCount++;
                // Try to swap in another process
                swapInNextProcess();
            }
        }
    }

    // Print final statistics.
    cout << "+++ Page access summary" << endl;
    cout << "\tTotal number of page accesses = " << pageAccesses << endl;
    cout << "\tTotal number of page faults = " << pageFaults << endl;
    cout << "\tTotal number of swaps = " << swapCount << endl;
    cout << "\tDegree of multiprogramming = " << minActiveProcesses << endl;

    // Cleanup: free all process objects.
    for (int i = 0; i < totalProcesses; i++) {
        free(processes[i]->keys);
        free(processes[i]->pt);
        free(processes[i]);
    }
    free(processes);
    return 0;
}
