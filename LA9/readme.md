
Simulation of demand paging with swapping (no page replacement) for an embedded system.

## Memory Parameters:
- **Total memory**: 64 MB, **Page size**: 4 KB → **Total frames** = 16384.
- **OS reserves** 16 MB, so **available user frames** = 48 MB / 4 KB = 12288.

## Process Details:
- Each process has a **page table** of **2048 entries** (unsigned short, 16-bit).
- **Bit 15** (most significant) is the **valid/invalid flag**.
- **Lower 15 bits** store the **frame number**.
- **First 10 pages** (virtual pages 0-9) hold the **essential segments**.
- **Additional data segment (array A)** starts at **virtual page 10**.

## Simulation:
- Each process is **initially loaded** with its **10 essential pages**.
- Each process performs **m binary searches** over its **array A**.
- For a binary search:
    - Simulate **A[i] = i** and check the page containing **A[M]**.
    - If the page is **not loaded** (invalid entry), a **page fault** occurs.
    - If a **free frame** is available, load the page (**update page table** and **remove frame** from free list).
    - If **no free frame** is available, the process is **swapped out**:
        - **All frames** allocated to the process are **freed** (both essential & A pages).
        - The process is placed in a **FIFO swapped-out queue**.
        - Its **current search is abandoned** (to restart later upon swap-in).
- When a process **finishes all m searches**, it **quits and frees its frames**.
- After termination, if **swapped-out processes** are waiting, **one process is swapped in**.

## Kernel Maintains:
- **Ready queue (FIFO)** of active processes.
- **Free frames list**.
- **Swapped-out process queue**.
- **Global counters**:
    - Page accesses.
    - Page faults.
    - Number of swaps.
- **Minimum number of active processes observed when memory was full** (degree of multiprogramming).

## Input Format (from "search.txt"):
- **First line**: `n m`    *(n: number of processes, m: searches per process)*
- **Next n lines**, each containing: `s k0 k1 ... km`
    - `s` is the **size of array A** (number of 32-bit integers).
    - Each `k` is an **index in A**.

## Compilation:
```sh
g++ -Wall -o runsearch demandpaging.cpp
```
- Optionally, add `-DVERBOSE` for **verbose mode**.