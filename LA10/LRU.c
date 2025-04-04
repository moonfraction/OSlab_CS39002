#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// System parameters
const int PAGE_SIZE = 4096;                         
const int TOTAL_MEMORY = 64 * 1024 * 1024;          
const int OS_MEMORY = 16 * 1024 * 1024;             
const int USER_MEMORY = TOTAL_MEMORY - OS_MEMORY;   
const int TOTAL_FRAMES = TOTAL_MEMORY / PAGE_SIZE;  
const int OS_FRAMES = OS_MEMORY / PAGE_SIZE;        
const int USER_FRAMES = USER_MEMORY / PAGE_SIZE;    
const int INTS_PER_PAGE = PAGE_SIZE / sizeof(int);  
const int PAGE_TABLE_ENTRIES = 2048;                
const int ESSENTIAL_PAGES = 10;                     
const int NFFMIN = 1000;                            

// Page table entry with age tracking for LRU
typedef struct {
    uint16_t entry;      // Structure: Valid(15) | Ref(14) | FrameNum(0-13)
    uint16_t history;    // Tracks reference history for LRU approximation
} PageTableEntry;

// Frame list entry for managing free frames
typedef struct {
    int frameNumber;     
    int lastOwner;       
    int lastPage;        
} FrameListEntry;

// Process control block
typedef struct {
    int pid;              
    int s;                
    int m;                
    int *keys;            
    int currentSearch;    
    PageTableEntry *pt;   
    // Performance metrics
    int pageAccesses;
    int pageFaults;
    int pageReplacements;
    int attemptCounts[4]; 
} Process;

// Bit definitions for page table entries
#define VALID_FLAG 0x8000    
#define REF_FLAG   0x4000    
#define FRAME_NUM_MASK 0x3FFF

// Page table entry manipulation
bool isValid(uint16_t entry) {
    return (entry & VALID_FLAG) ? true : false;
}

bool isReferenced(uint16_t entry) {
    return (entry & REF_FLAG) ? true : false;
}

int getFrame(uint16_t entry) {
    return (int)(entry & FRAME_NUM_MASK);
}

uint16_t makeEntry(int frame, bool referenced) {
    uint16_t result = VALID_FLAG;
    if (referenced) result |= REF_FLAG;
    return result | (frame & FRAME_NUM_MASK);
}

void setReferenced(uint16_t *entry) {
    *entry = (*entry) | REF_FLAG;
}

void clearReferenced(uint16_t *entry) {
    *entry = (*entry) & (~REF_FLAG);
}

void invalidate(uint16_t *entry) {
    *entry = 0;
}

// Global state
FrameListEntry *freeFrames;
Process **processes;
int NFF = 0;
int totalProcesses = 0;
int searchesPerProcess = 0;

// Global metrics
int totalPageAccesses = 0;
int totalPageFaults = 0;
int totalPageReplacements = 0;
int totalAttemptCounts[4] = {0};

// Helper function to allocate and initialize memory
void* safeAlloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Fatal error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    memset(ptr, 0, size);
    return ptr;
}

// Initialize process data structure
void initproc(Process *proc, int id, int size, int searches) {
    proc->pid = id;
    proc->s = size;
    proc->m = searches;
    proc->currentSearch = 0;
    
    // Allocate arrays
    proc->keys = (int*)safeAlloc(searches * sizeof(int));
    proc->pt = (PageTableEntry*)safeAlloc(PAGE_TABLE_ENTRIES * sizeof(PageTableEntry));
    
    // Reset statistics counters
    proc->pageAccesses = 0;
    proc->pageFaults = 0;
    proc->pageReplacements = 0;
    for (int i = 0; i < 4; i++) {
        proc->attemptCounts[i] = 0;
    }
}

void removeFreeFrameAt(int idx) {
    for (int i = idx; i < NFF - 1; i++) {
        freeFrames[i] = freeFrames[i + 1];
    }
    NFF--;
}

// Frame allocation from free list
bool allocateFrame(Process *proc, int vpage) {
    if (NFF <= 0) return false;
    
    int assignedFrame = freeFrames[0].frameNumber;
    
    // Remove from free list
    removeFreeFrameAt(0);
    
    // Update page table
    proc->pt[vpage].entry = makeEntry(assignedFrame, true);
    proc->pt[vpage].history = 0xFFFF;  // All bits set (recently used)
    
    return true;
}

// Allocate essential pages for a process
bool allocateEssentialPages(Process *proc) {
    for (int page = 0; page < ESSENTIAL_PAGES; page++) {
        if (!allocateFrame(proc, page)) {
            return false;  // Not enough memory
        }
    }
    return true;
}

// Return all frames back to free list when a process exits
void freeProcessFrames(Process *proc) {
    for (int page = 0; page < PAGE_TABLE_ENTRIES; page++) {
        if (isValid(proc->pt[page].entry)) {
            int frame = getFrame(proc->pt[page].entry);
            
            // Add to free list
            freeFrames[NFF].frameNumber = frame;
            freeFrames[NFF].lastOwner = proc->pid;
            freeFrames[NFF].lastPage = page;
            NFF++;
            
            // Mark as invalid in page table
            invalidate(&proc->pt[page].entry);
        }
    }
}

int findVictimPage(Process *proc) {
    int victim = -1;
    uint16_t lowestUsage = 0xFFFF;
    
    // page with lowest history value (least recently used)
    for (int page = ESSENTIAL_PAGES; page < PAGE_TABLE_ENTRIES; page++) {
        if (!isValid(proc->pt[page].entry)) continue;
        
        if (proc->pt[page].history < lowestUsage) {
            lowestUsage = proc->pt[page].history;
            victim = page;
        }
    }
    
    return victim;
}

// Helper to print attempt details in verbose mode
void p_Attempt(int attempt, int frame, int owner, int page) {
    #ifdef VERBOSE
    switch (attempt) {
        case 0:
            printf("        Attempt 1: Page found in free frame %d\n", frame);
            break;
        case 1:
            printf("        Attempt 2: Free frame %d owned by no process found\n", frame);
            break;
        case 2:
            printf("        Attempt 3: Own page %d found in free frame %d\n", 
                   page, frame);
            break;
        case 3:
            printf("        Attempt 4: Random frame %d owned by Process %d chosen\n",
                   frame, owner);
            break;
    }
    #endif
}

int findSuitableFrame(Process *proc, int vpage) {
    int frameIndex = -1;
    int attemptUsed = -1;
    
    // Attmept 1: frames that held the same page for this process
    for (int i = NFF-1; i >= 0; i--) {
        if (freeFrames[i].lastOwner == proc->pid && 
            freeFrames[i].lastPage == vpage) {
            frameIndex = i;
            attemptUsed = 0;
            p_Attempt(0, freeFrames[i].frameNumber, proc->pid, vpage);
            break;
        }
    }
    
    // Attmept 2: no previous owner
    if (frameIndex == -1) {
        for (int i = NFF - 1; i >= 0; i--) {
            if (freeFrames[i].lastOwner == -1) {
                frameIndex = i;
                attemptUsed = 1;
                p_Attempt(1, freeFrames[i].frameNumber, -1, -1);
                break;
            }
        }
    }
    
    // Attmept 3: Try frames owned by same process
    if (frameIndex == -1) {
        for (int i = NFF - 1; i >= 0; i--) {
            if (freeFrames[i].lastOwner == proc->pid) {
                frameIndex = i;
                attemptUsed = 2;
                p_Attempt(2, freeFrames[i].frameNumber, proc->pid, 
                                 freeFrames[i].lastPage);
                break;
            }
        }
    }
    
    // Attmept 4: Pick any random frame
    if (frameIndex == -1) {
        frameIndex = 0; // Randomly select the first frame
        attemptUsed = 3;
        p_Attempt(3, freeFrames[frameIndex].frameNumber, 
                         freeFrames[frameIndex].lastOwner,
                         freeFrames[frameIndex].lastPage);
    }
    
    // Update statistics
    proc->attemptCounts[attemptUsed]++;
    totalAttemptCounts[attemptUsed]++;
    
    return frameIndex;
}

void updatePageHistory(Process *proc) {
    for (int page = 0; page < PAGE_TABLE_ENTRIES; page++) {
        if (!isValid(proc->pt[page].entry)) continue;
        
        uint16_t wasReferenced = isReferenced(proc->pt[page].entry) ? 0x8000 : 0;
        proc->pt[page].history = (proc->pt[page].history >> 1) | wasReferenced;
        
        clearReferenced(&proc->pt[page].entry);
    }
}

// Handle page fault during simulation
bool handlePageFault(Process *proc, int vpage) {
    proc->pageFaults++;
    totalPageFaults++;
    
    #ifdef VERBOSE
    printf("    Fault on Page %4d: ", vpage);
    #endif
    
    if (NFF > NFFMIN) { // free frames available
        if (!allocateFrame(proc, vpage)) {
            fprintf(stderr, "Error: Frame allocation failed despite sufficient free frames\n");
            return false;
        }
        #ifdef VERBOSE
        printf("Free frame %d found\n", getFrame(proc->pt[vpage].entry));
        #endif
        return true;
    }
    
    // else find victim page
    proc->pageReplacements++;
    totalPageReplacements++;
    
    int victimPage = findVictimPage(proc);
    if (victimPage < 0) {
        fprintf(stderr, "Error: No suitable victim page found\n");
        return false;
    }
    
    int victimFrame = getFrame(proc->pt[victimPage].entry);
    
    #ifdef VERBOSE
    printf("To replace Page %3d at Frame %d [history = %d]\n",
           victimPage, victimFrame, proc->pt[victimPage].history);
    #endif
    
    int frameIndex = findSuitableFrame(proc, vpage); // free frame for vpage
    int newFrame = freeFrames[frameIndex].frameNumber;
    
    // Update data structures
    removeFreeFrameAt(frameIndex);
    invalidate(&proc->pt[victimPage].entry);
    proc->pt[vpage].entry = makeEntry(newFrame, true);
    proc->pt[vpage].history = 0xFFFF;  // Mark as most recently used
    
    // Return victim frame to free list
    freeFrames[NFF].frameNumber = victimFrame;
    freeFrames[NFF].lastOwner = proc->pid;
    freeFrames[NFF].lastPage = victimPage;
    NFF++;
    
    return true;
}

// Simulate binary search with page replacement
bool binarySearch(Process *proc, int k) {
    int L = 0;
    int R = proc->s - 1;
    
    while (L < R) {
        int M = (L+R) / 2;
        
        int arrayOffset = M;
        int pageOffset = arrayOffset / INTS_PER_PAGE;
        int vpage = ESSENTIAL_PAGES + pageOffset; // virtual page number
        
        proc->pageAccesses++;
        totalPageAccesses++;
        
        // Check if page is in memory
        if (!isValid(proc->pt[vpage].entry)) {
            // Handle page fault
            if (!handlePageFault(proc, vpage)) {
                return false;
            }
        } else {
            // Page in memory, mark as referenced
            setReferenced(&proc->pt[vpage].entry);
        }
        
        // Continue binary search
        if (k <= M) {
            R = M;
        } else {
            L = M + 1;
        }
    }
    
    // Update page reference history after search completes
    updatePageHistory(proc);
    return true;
}

// Read input data from file
void readinput() {
    FILE *input;
    if ((input = fopen("search.txt", "r")) == NULL) {
        perror("Cannot open search.txt");
        exit(EXIT_FAILURE);
    }
    
    if (fscanf(input, "%d %d", &totalProcesses, &searchesPerProcess) != 2) {
        fprintf(stderr, "Error reading process count and search count\n");
        exit(EXIT_FAILURE);
    }
    
    processes = (Process**)safeAlloc(totalProcesses * sizeof(Process*));
    
    // Read each process data
    for (int i = 0; i < totalProcesses; i++) {
        int arraySize;
        if (fscanf(input, "%d", &arraySize) != 1) {
            fprintf(stderr, "Error reading array size for process %d\n", i);
            exit(EXIT_FAILURE);
        }
        
        Process *proc = (Process*)safeAlloc(sizeof(Process));
        initproc(proc, i, arraySize, searchesPerProcess);
        
        // Read search keys
        for (int j = 0; j < searchesPerProcess; j++) {
            if (fscanf(input, "%d", &proc->keys[j]) != 1) {
                fprintf(stderr, "Error reading search key %d for process %d\n", j, i);
                exit(EXIT_FAILURE);
            }
        }
        
        // Allocate essential pages
        if (!allocateEssentialPages(proc)) {
            fprintf(stderr, "Not enough memory for essential pages of process %d\n", i);
            exit(EXIT_FAILURE);
        }
        
        processes[i] = proc;
    }
    
    fclose(input);
}

// statistics for a single process
void printProcessStatistics(Process *proc) {
    // Calculate percentages
    float faultPercent = (proc->pageAccesses > 0) ? (proc->pageFaults * 100.0f) / proc->pageAccesses : 0;
    float replacePercent = (proc->pageAccesses > 0) ? (proc->pageReplacements * 100.0f) / proc->pageAccesses : 0;
    
    // Calculate attempt percentages
    float attemptPercent[4];
    for (int i = 0; i < 4; i++) {
        attemptPercent[i] = (proc->pageReplacements > 0) ? (proc->attemptCounts[i] * 100.0f) / proc->pageReplacements : 0;
    }
    
    printf("    %-3d       %d    %5d   (%5.2f%%)  %5d   (%5.2f%%)    %3d + %3d + %3d + %d "
           "(%4.2f%% + %4.2f%% + %4.2f%% + %4.2f%%)\n",
           proc->pid, proc->pageAccesses, proc->pageFaults, faultPercent,
           proc->pageReplacements, replacePercent,
           proc->attemptCounts[0], proc->attemptCounts[1], 
           proc->attemptCounts[2], proc->attemptCounts[3],
           attemptPercent[0], attemptPercent[1], attemptPercent[2], attemptPercent[3]);
}

// Print overall statistics
void printTotalStatistics() {
    float faultPercent = (totalPageAccesses > 0) ? (totalPageFaults * 100.0f) / totalPageAccesses : 0;
    float replacePercent = (totalPageAccesses > 0) ? (totalPageReplacements * 100.0f) / totalPageAccesses : 0;
    
    float attemptPercent[4];
    for (int i = 0; i < 4; i++) {
        attemptPercent[i] = (totalPageReplacements > 0) ? (totalAttemptCounts[i] * 100.0f) / totalPageReplacements : 0;
    }
    
    printf("\n    Total     %d    %5d (%5.2f%%)    %5d (%5.2f%%)    %3d + %3d + %3d + %d "
           "(%4.2f%% + %4.2f%% + %4.2f%% + %4.2f%%)\n",
           totalPageAccesses, totalPageFaults, faultPercent,
           totalPageReplacements, replacePercent,
           totalAttemptCounts[0], totalAttemptCounts[1], 
           totalAttemptCounts[2], totalAttemptCounts[3],
           attemptPercent[0], attemptPercent[1], attemptPercent[2], attemptPercent[3]);
}

// Clean up all allocated memory
void cleanup() {
    for (int i = 0; i < totalProcesses; i++) {
        free(processes[i]->keys);
        free(processes[i]->pt);
        free(processes[i]);
    }
    free(processes);
    free(freeFrames);
}

// Main execution function
int main() {
    srand((unsigned int)time(NULL) * getpid());
    
    // Set up free frame list
    freeFrames = (FrameListEntry*)safeAlloc(USER_FRAMES * sizeof(FrameListEntry));
    for (int i = 0; i < USER_FRAMES; i++) {
        freeFrames[i].frameNumber = i;
        freeFrames[i].lastOwner = -1;
        freeFrames[i].lastPage = -1;
    }
    NFF = USER_FRAMES;
    
    // Read and process input data
    readinput();

    /**** Round-robin execution of processes ****/
    int finishedProcesses = 0;
    int current = 0;
    
    while (finishedProcesses < totalProcesses) {
        Process *proc = processes[current];
        
        // Skip completed processes
        if (proc->currentSearch >= proc->m) {
            current = (current + 1) % totalProcesses;
            continue;
        }
        
        int searchKey = proc->keys[proc->currentSearch];
        
        #ifdef VERBOSE
        printf("+++ Process %d: Search %d\n", proc->pid, proc->currentSearch + 1);
        #endif
        
        if (binarySearch(proc, searchKey)) {
            proc->currentSearch++;
            
            // Check if process has completed all searches
            if (proc->currentSearch >= proc->m) {
                finishedProcesses++;
                freeProcessFrames(proc);
            }
        } else {
            fprintf(stderr, "Search operation failed for process %d\n", proc->pid);
            return EXIT_FAILURE;
        }
        
        current = (current + 1) % totalProcesses; // next process
    }
    
    // final statistics
    printf("+++ Page access summary\n");
    printf("    PID     Accesses        Faults         Replacements                        Attempts\n");
    
    for (int i = 0; i < totalProcesses; i++) {
        printProcessStatistics(processes[i]);
    }
    
    printTotalStatistics();
    
    // Free all allocated memory
    cleanup();
    
    return 0;
}