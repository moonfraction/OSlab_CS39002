
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
    vector<int> keys;                               // search keys (each search key is an index in A)
    int currentSearch;                              // index of next search to perform
    vector<uint16_t> pt;                            // page table of size PAGE_TABLE_ENTRIES (each entry is 16-bit)
    
    // Constructor initializes page table to 0 and currentSearch to 0.
    Process(int id, int size, int searches) : pid(id), s(size), m(searches), currentSearch(0) {
        pt.assign(PAGE_TABLE_ENTRIES, 0);
    }
};

// Global kernel data
queue<int> readyQ;                                  // holds process ids of active processes (round robin)
queue<int> swappedQ;                                // FIFO queue of swapped-out processes (store process id)
vector<int> freeFrames;                             // free frame list (store frame numbers)
vector<Process*> processes;                         // all processes indexed by pid

// Statistics
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
    if(freeFrames.empty()) return false;

    int frame = freeFrames.back();
    freeFrames.pop_back();
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
            freeFrames.push_back(frame);
            proc->pt[i] = 0;
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
            if (freeFrames.empty()) {
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
    cout << "+++ Swapping out process " << proc->pid 
        << " [" << activeProcesses << " active processes]" << endl;
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
    cout << "+++ Swapping in process " << proc->pid 
        << " [" << (activeProcesses+1) << " active processes]" << endl;
    return true;
}

//
// MAIN SIMULATION
//
int main() {
    ios::sync_with_stdio(false);
    // Initialize freeFrames list with USER_FRAMES available.
    // For simplicity, we use frame numbers 0 to USER_FRAMES-1.
    for (int i = 0; i < USER_FRAMES; i++) {
        freeFrames.push_back(i);
    }

    // Read input from "search.txt"
    ifstream fin("search.txt");
    if (!fin) {
        cerr << "Error: Cannot open input file 'search.txt'" << endl;
        return 1;
    }
    fin >> totalProcesses >> searchesPerProcess;
    
    // Create processes and initialize their page tables.
    processes.resize(totalProcesses, nullptr);
    for (int i = 0; i < totalProcesses; i++) {
        int s;
        fin >> s;
        Process *proc = new Process(i, s, searchesPerProcess);
        // Read m search keys.
        for (int j = 0; j < searchesPerProcess; j++) {
            int key;
            fin >> key;
            proc->keys.push_back(key);
        }
        // Allocate essential pages (pages 0 to ESSENTIAL_PAGES-1)
        if (!allocateEssentialPages(proc)) {
            cerr << "Error: Not enough free frames to allocate essential pages for process " << i << endl;
            return 1;
        }
        processes[i] = proc;
        readyQ.push(i);
        activeProcesses++;
    }
    fin.close();

    cout << "+++ Simulation data read from file" << endl;
    cout << "+++ Kernel data initialized" << endl;

#ifdef VERBOSE
    cout << "+++ Running in VERBOSE mode" << endl;
#endif

    int completedCount = 0;
    // Simulation loop: continue until all processes have finished
    while (completedCount < totalProcesses) {
        // If readyQ is empty but there are swapped-out processes waiting,
        // swap one in.
        if (readyQ.empty() && !swappedQ.empty()) {
            int pidToSwap = swappedQ.front();
            swappedQ.pop();
            if (!swapIn(processes[pidToSwap])) {
                cerr << "Error during swap-in." << endl;
                return 1;
            }
            activeProcesses++;
            readyQ.push(pidToSwap);
        }
        if (readyQ.empty())
            break; // safety check

        int pid = readyQ.front();
        readyQ.pop();
        Process *proc = processes[pid];

#ifdef VERBOSE
        cout << "Process " << pid << " performing search " << proc->currentSearch << endl;
#endif

        // If the process has finished all its searches, terminate it.
        if (proc->currentSearch >= proc->m) {
            freeProcessFrames(proc);
            activeProcesses--;
            completedCount++;
            // After termination, if there are swapped-out processes, swap one in.
            if (!swappedQ.empty()) {
                int swapPid = swappedQ.front();
                swappedQ.pop();
                if (!swapIn(processes[swapPid])) {
                    cerr << "Error during swap-in." << endl;
                    return 1;
                }
                activeProcesses++;
                readyQ.push(swapPid);
            }
            continue;
        }
        // Simulate one binary search for the current search key.
        int key = proc->keys[proc->currentSearch];
        bool success = simulateBinarySearch(proc, key);
        if (!success) {
            // Swap out the process because free frame list was empty.
            swapOut(proc);
            // Do not advance currentSearch; the search will be restarted after swap-in.
        } else {
            // Binary search finished successfully.
#ifdef VERBOSE
            cout << "Process " << pid << " completed search " << proc->currentSearch << endl;
#endif
            proc->currentSearch++; // move to next search
            // After a successful search, requeue the process if it has more searches.
            if (proc->currentSearch < proc->m) {
                readyQ.push(pid);
            } else {
                // Process finished: free its frames and update counters.
                freeProcessFrames(proc);
                activeProcesses--;
                completedCount++;
                // Check if a swapped-out process can be swapped in.
                if (!swappedQ.empty()) {
                    int swapPid = swappedQ.front();
                    swappedQ.pop();
                    if (!swapIn(processes[swapPid])) {
                        cerr << "Error during swap-in." << endl;
                        return 1;
                    }
                    activeProcesses++;
                    readyQ.push(swapPid);
                }
            }
        }
    }

    // Print final statistics.
    cout << "+++ Page access summary" << endl;
    cout << "Total number of page accesses = " << pageAccesses << endl;
    cout << "Total number of page faults = " << pageFaults << endl;
    cout << "Total number of swaps = " << swapCount << endl;
    cout << "Degree of multiprogramming = " << minActiveProcesses << endl;

    // Cleanup: free all process objects.
    for (size_t i = 0; i < processes.size(); i++) {
        delete processes[i];
    }
    return 0;
}
