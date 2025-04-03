#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

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
const int NFFMIN = 1000;                            // minimum number of free frames

// Extended page table entry with history
typedef struct {
    uint16_t entry;      // MSB (bit 15)=valid, bit 14=reference, bits 0-13=frame number
    uint16_t history;    // History bits for approximate LRU
} PageTableEntry;

// Free frame list entry
typedef struct {
    int frameNumber;     // frame number
    int lastOwner;       // last process that owned this frame (-1 if none)
    int lastPage;        // page number in last owner (-1 if none)
} FrameListEntry;

// Process structure
typedef struct {
    int pid;              // process id
    int s;                // size of array A (number of integers)
    int m;                // number of searches to perform
    int *keys;            // search keys (each key is an index in A)
    int currentSearch;    // next search index to perform
    PageTableEntry *pt;   // page table (array of PAGE_TABLE_ENTRIES entries)
    // Statistics
    int pageAccesses;
    int pageFaults;
    int pageReplacements;
    int attemptCounts[4]; // Counts for replacement attempts 1-4
} Process;

// Bit mask macros for page table entry
#define VALID_BIT 0x8000    // Bit 15 (MSB)
#define REF_BIT   0x4000    // Bit 14
#define FRAME_MASK 0x3FFF   // Bits 0-13

// Functions to manipulate page table entries
bool isValid(uint16_t entry) {
    return (entry & VALID_BIT) != 0;
}

bool isReferenced(uint16_t entry) {
    return (entry & REF_BIT) != 0;
}

int getFrame(uint16_t entry) {
    return entry & FRAME_MASK;
}

uint16_t makeEntry(int frame, bool referenced) {
    return (uint16_t)((frame & FRAME_MASK) | VALID_BIT | (referenced ? REF_BIT : 0));
}

void setReferenced(uint16_t *entry) {
    *entry |= REF_BIT;
}

void clearReferenced(uint16_t *entry) {
    *entry &= ~REF_BIT;
}

void invalidate(uint16_t *entry) {
    *entry = 0;
}

// Global kernel data
FrameListEntry *freeFrames;  // free frame list (array of USER_FRAMES entries)
int NFF = 0;                 // current number of free frames
Process **processes;         // pointer array of processes
int totalProcesses = 0;      // n (total processes)
int searchesPerProcess = 0;  // m (searches per process)

// Global statistics
int totalPageAccesses = 0;
int totalPageFaults = 0;
int totalPageReplacements = 0;
int totalAttemptCounts[4] = {0, 0, 0, 0};

// Initialize a process structure
void initproc(Process *proc, int id, int size, int searches) {
    proc->pid = id;
    proc->s = size;
    proc->m = searches;
    proc->currentSearch = 0;
    proc->keys = (int *)malloc(searches * sizeof(int));
    proc->pt = (PageTableEntry *)malloc(PAGE_TABLE_ENTRIES * sizeof(PageTableEntry));
    
    if (proc->keys == NULL || proc->pt == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    
    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        proc->pt[i].entry = 0;
        proc->pt[i].history = 0;
    }
    
    proc->pageAccesses = 0;
    proc->pageFaults = 0;
    proc->pageReplacements = 0;
    memset(proc->attemptCounts, 0, sizeof(proc->attemptCounts));
}

// Allocate a free frame for a process from the head of the free frame list.
// This implementation shifts the array so that free frames are allocated in increasing order.
bool allocateFrame(Process *proc, int vpage) {
    if (NFF == 0) return false;
    
    // Remove from the head (index 0)
    int frame = freeFrames[0].frameNumber;
    // Shift the array left by one
    for (int i = 1; i < NFF; i++) {
        freeFrames[i - 1] = freeFrames[i];
    }
    NFF--;
    
    proc->pt[vpage].entry = makeEntry(frame, true);
    proc->pt[vpage].history = 0xFFFF;
    
    return true;
}

// Allocate essential pages for a process.
bool allocateEssentialPages(Process *proc) {
    for (int vp = 0; vp < ESSENTIAL_PAGES; vp++) {
        if (!allocateFrame(proc, vp))
            return false;
        proc->pt[vp].history = 0xFFFF;
    }
    return true;
}

// When a process exits, free all its frames and append them to the free frame list (using tail insertion).
void freeProcessFrames(Process *proc) {
    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        if (isValid(proc->pt[i].entry)) {
            int frame = getFrame(proc->pt[i].entry);
            freeFrames[NFF].frameNumber = frame;
            freeFrames[NFF].lastOwner = proc->pid;
            freeFrames[NFF].lastPage = i;
            NFF++;
            invalidate(&proc->pt[i].entry);
        }
    }
}

// Find the victim page for replacement (non-essential pages) using approximate LRU.
int findVictimPage(Process *proc) {
    int victimPage = -1;
    uint16_t minHistory = 0xFFFF;
    for (int i = ESSENTIAL_PAGES; i < PAGE_TABLE_ENTRIES; i++) {
        if (isValid(proc->pt[i].entry) && proc->pt[i].history < minHistory) {
            minHistory = proc->pt[i].history;
            victimPage = i;
        }
    }
    return victimPage;
}

// Find a suitable free frame using four strategies.
int findSuitableFrame(Process *proc, int vpage) {
    int frameIndex = -1;
    int attemptUsed = -1;
    
    // Attempt 1: frame with same owner and page.
    for (int i = 0; i < NFF; i++) {
        if (freeFrames[i].lastOwner == proc->pid && freeFrames[i].lastPage == vpage) {
            frameIndex = i;
            attemptUsed = 0;
            break;
        }
    }
    
    // Attempt 2: frame with no owner.
    if (frameIndex == -1) {
        for (int i = 0; i < NFF; i++) {
            if (freeFrames[i].lastOwner == -1) {
                frameIndex = i;
                attemptUsed = 1;
                break;
            }
        }
    }
    
    // Attempt 3: frame with same owner (different page).
    if (frameIndex == -1) {
        for (int i = 0; i < NFF; i++) {
            if (freeFrames[i].lastOwner == proc->pid && freeFrames[i].lastPage != vpage) {
                frameIndex = i;
                attemptUsed = 2;
                break;
            }
        }
    }
    
    // Attempt 4: pick a random free frame.
    if (frameIndex == -1) {
        frameIndex = rand() % NFF;
        attemptUsed = 3;
    }
    
    proc->attemptCounts[attemptUsed]++;
    totalAttemptCounts[attemptUsed]++;
    return frameIndex;
}

// Shift freeFrames array to remove the entry at index idx.
void removeFreeFrameAt(int idx) {
    for (int i = idx + 1; i < NFF; i++) {
        freeFrames[i - 1] = freeFrames[i];
    }
    NFF--;
}

// Update history for each valid page after a binary search.
void updatePageHistory(Process *proc) {
    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        if (isValid(proc->pt[i].entry)) {
            proc->pt[i].history = (proc->pt[i].history >> 1) | 
                                  (isReferenced(proc->pt[i].entry) ? 0x8000 : 0);
            clearReferenced(&proc->pt[i].entry);
        }
    }
}

// Simulate binary search with page replacement.
bool simulateBinarySearch(Process *proc, int k) {
    int L = 0, R = proc->s - 1;
    while (L < R) {
        int M = (L + R) / 2;
        int offset = M / INTS_PER_PAGE;
        int vpage = ESSENTIAL_PAGES + offset;
        proc->pageAccesses++;
        totalPageAccesses++;
        
        if (!isValid(proc->pt[vpage].entry)) {
            proc->pageFaults++;
            #ifdef VERBOSE
                printf("    Fault on Page %4d: ", vpage);
            #endif
            totalPageFaults++;
            
            if (NFF > NFFMIN) {
                if (!allocateFrame(proc, vpage)) {
                    fprintf(stderr, "Error: Failed to allocate frame with sufficient free frames\n");
                    return false;
                }
                #ifdef VERBOSE
                    printf("Free frame %d found\n", getFrame(proc->pt[vpage].entry));
                #endif
            } else {
                proc->pageReplacements++;
                totalPageReplacements++;
                int victimPage = findVictimPage(proc);
                if (victimPage == -1) {
                    fprintf(stderr, "Error: Could not find a victim page for replacement\n");
                    return false;
                }
                int victimFrame = getFrame(proc->pt[victimPage].entry);
                #ifdef VERBOSE
                    printf("To replace Page %4d at Frame %d [history = %d]\n",
                        victimPage, victimFrame, proc->pt[victimPage].history);
                #endif
                int frameIndex = findSuitableFrame(proc, vpage);
                int newFrame = freeFrames[frameIndex].frameNumber;
                removeFreeFrameAt(frameIndex);
                invalidate(&proc->pt[victimPage].entry);
                proc->pt[vpage].entry = makeEntry(newFrame, true);
                proc->pt[vpage].history = 0xFFFF;
                freeFrames[NFF].frameNumber = victimFrame;
                freeFrames[NFF].lastOwner = proc->pid;
                freeFrames[NFF].lastPage = victimPage;
                NFF++;
            }
        } else {
            setReferenced(&proc->pt[vpage].entry);
        }
        
        if (k <= M)
            R = M;
        else
            L = M + 1;
    }
    
    updatePageHistory(proc);
    return true;
}

void readinput(){
    FILE *fin = fopen("search.txt", "r");
    if (!fin) {
        perror("search.txt");
        exit(1);
    }
    
    fscanf(fin, "%d %d", &totalProcesses, &searchesPerProcess);
    
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
        
        for (int j = 0; j < searchesPerProcess; j++) {
            fscanf(fin, "%d", &proc->keys[j]);
        }
        
        if (!allocateEssentialPages(proc)) {
            fprintf(stderr, "Error: Not enough free frames for essential pages of process %d\n", i);
            exit(1);
        }
        
        processes[i] = proc;
    }
    fclose(fin);
}

void printProcessStatistics(Process *proc) {
    printf("    %-3d       %d    %5d   (%5.2f%%)  %5d   (%5.2f%%)    %3d + %3d + %3d + %3d (%4.2f%% + %4.2f%% + %4.2f%% + %4.2f%%)\n",
           proc->pid,
           proc->pageAccesses,
           proc->pageFaults,
           (proc->pageFaults * 100.0) / proc->pageAccesses,
           proc->pageReplacements,
           (proc->pageReplacements * 100.0) / proc->pageAccesses,
           proc->attemptCounts[0],
           proc->attemptCounts[1],
           proc->attemptCounts[2],
           proc->attemptCounts[3],
           (proc->pageReplacements > 0 ? (proc->attemptCounts[0] * 100.0) / proc->pageReplacements : 0),
           (proc->pageReplacements > 0 ? (proc->attemptCounts[1] * 100.0) / proc->pageReplacements : 0),
           (proc->pageReplacements > 0 ? (proc->attemptCounts[2] * 100.0) / proc->pageReplacements : 0),
           (proc->pageReplacements > 0 ? (proc->attemptCounts[3] * 100.0) / proc->pageReplacements : 0));
}

void printTotalStatistics() {
    printf("\n    Total     %d    %5d (%5.2f%%)    %5d (%5.2f%%)    %3d + %3d + %3d + %3d (%4.2f%% + %4.2f%% + %4.2f%% + %4.2f%%)\n",
           totalPageAccesses,
           totalPageFaults,
           (totalPageFaults * 100.0) / totalPageAccesses,
           totalPageReplacements,
           (totalPageReplacements * 100.0) / totalPageAccesses,
           totalAttemptCounts[0],
           totalAttemptCounts[1],
           totalAttemptCounts[2],
           totalAttemptCounts[3],
           (totalPageReplacements > 0 ? (totalAttemptCounts[0] * 100.0) / totalPageReplacements : 0),
           (totalPageReplacements > 0 ? (totalAttemptCounts[1] * 100.0) / totalPageReplacements : 0),
           (totalPageReplacements > 0 ? (totalAttemptCounts[2] * 100.0) / totalPageReplacements : 0),
           (totalPageReplacements > 0 ? (totalAttemptCounts[3] * 100.0) / totalPageReplacements : 0));
}

int main() {
    srand(time(NULL));
    
    freeFrames = (FrameListEntry *)malloc(USER_FRAMES * sizeof(FrameListEntry));
    if (freeFrames == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    
    // Initialize free frame list with frame numbers starting at 1280
    for (int i = 0; i < USER_FRAMES; i++) {
        freeFrames[i].frameNumber = i;
        freeFrames[i].lastOwner = -1;
        freeFrames[i].lastPage = -1;
    }
    NFF = USER_FRAMES;
    
    readinput();

    int completedProcesses = 0;
    int currentPid = 0;
    
    while (completedProcesses < totalProcesses) {
        Process *proc = processes[currentPid];
        if (proc->currentSearch >= proc->m) {
            currentPid = (currentPid + 1) % totalProcesses;
            continue;
        }
        
        int key = proc->keys[proc->currentSearch];
        
        #ifdef VERBOSE
        printf("+++ Process %d: Search %d\n", proc->pid, proc->currentSearch + 1);
        #endif
        
        bool success = simulateBinarySearch(proc, key);
        
        if (success) {
            proc->currentSearch++;
            if (proc->currentSearch >= proc->m) {
                completedProcesses++;
                freeProcessFrames(proc);
            }
        } else {
            fprintf(stderr, "Error: Binary search failed for process %d\n", proc->pid);
            return 1;
        }
        
        currentPid = (currentPid + 1) % totalProcesses;
    }
    
    printf("+++ Page access summary\n");
    printf("    PID     Accesses        Faults         Replacements                        Attempts\n");
    
    for (int i = 0; i < totalProcesses; i++) {
        printProcessStatistics(processes[i]);
    }
    
    printTotalStatistics();
    
    for (int i = 0; i < totalProcesses; i++) {
        free(processes[i]->keys);
        free(processes[i]->pt);
        free(processes[i]);
    }
    free(processes);
    free(freeFrames);
    
    return 0;
}
