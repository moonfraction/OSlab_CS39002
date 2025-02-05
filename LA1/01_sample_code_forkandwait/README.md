# fork and wait


| Library       | Primary Purpose                                      | Example Use Case                                               |
|---------------|------------------------------------------------------|----------------------------------------------------------------|
| <sys/ipc.h>   | Interprocess Communication (IPC) mechanisms          | Setting up shared memory for parent-child communication.       |
| <sys/types.h> | System-specific data types for portability           | Using `pid_t` for process identifiers in `fork` or `waitpid`.        |
| <sys/wait.h>  | Process synchronization and termination management   | Waiting for a child process to complete using `waitpid` and analyzing its exit status using macros like `WIFEXITED`.         |
| <unistd.h>    | POSIX API for system-level process and file operations | Creating a child process with `fork`, or reading data with read.  |

<hr>

## **`fork.c`**

### `fork()`
- **`fork()`**: This system call is used to create a new process by duplicating the calling process. The newly created process, known as the child process, is an exact replica of the calling process, referred to as the parent process, until explicitly modified.
  - **Return Values**: 
    - Returns `0` to the child process.
    - Returns the PID of the child process to the parent process.
  - **Execution**:
    - The child process does not restart from the beginning of the program; it picks up execution from the point of the `fork()` call.
    - The parent process also continues execution immediately after the `fork()` call.
    - The parent and child processes can then follow different execution paths depending on the return value of `fork()`.
  - **Concurrency**: Both the child and parent processes can execute concurrently.

- **`getpid()`**: This function retrieves the process ID (PID) of the calling process. It is commonly used to generate unique temporary filenames.

- **`getppid()`**: This function returns the process ID of the parent of the calling process. The returned ID is either:
  - The ID of the process that created this process using `fork`.
  - If the creating process has terminated, the ID of the process to which this process has been reparented, which could be `init(1)` or a "subreaper" process defined via the `prctl(2)` `PR_SET_CHILD_SUBREAPER` operation.

<hr>

## **`fork2.c`**

### `wait()`

The `wait()` system call makes the parent process wait until any one of its child processes finishes execution.

- **Analysis**: You can analyze this value using macros like `WIFEXITED`, `WEXITSTATUS`, etc.
- **Return Value**:
  - Returns the PID of the terminated child process.
  - Returns `-1` if no child processes are left or on error.

### `waitpid()`

The `waitpid()` system call causes the parent process to wait for a child process with a specific PID to complete its execution.

- **Parameters**:
  - `pid`:
    - `> 0`: Waits for the child with the exact PID.
    - `-1`: Waits for any child process (same as `wait()`).
    - `0`: Waits for any child process in the same process group.
    - `< -1`: Waits for any child process in the group with a specific PGID (process group ID).
  - `status`: A pointer to an integer that will receive the exit status of the child process.
  - `options`: Flags to control the behavior of `waitpid()`.
    - `WNOHANG`: Non-blocking wait. Returns immediately if no child has exited.
    - `WUNTRACED`: Also waits for children that are stopped by a signal.
    - `WCONTINUED`: Waits for children that have been resumed after being stopped.

### Purpose of `WIFEXITED`

Before extracting the exit status using `WEXITSTATUS(status)`, ensure that the child process exited normally. If the process did not terminate via `exit()` or `return`, accessing its exit status using `WEXITSTATUS(status)` could result in undefined behavior.

### What `WEXITSTATUS(status)` Does

The `status` variable passed to `wait()` or `waitpid()` contains information about how the child process terminated. `WEXITSTATUS(status)` extracts the low-order 8 bits of the status, which represent the exit code passed by the child process to `exit()` or `return`.

### How `wait()` or `waitpid()` Works

When `wait()` or `waitpid()` is called, they:

- Block the parent process (if no child has terminated).
- Return the PID of the terminated child process.
- Fill the `status` variable with details about the child's termination.

### Other Related Macros

- **`WIFSIGNALED(status)`**: True if the child process was terminated by a signal.
- **`WIFSTOPPED(status)`**: True if the child process was stopped by a signal.
- **`WTERMSIG(status)`**: Gets the signal number if the process was terminated by a signal.
- **`WSTOPSIG(status)`**: Gets the signal number if the process was stopped by a signal.

> **What happens if the parent process terminates prematurely before its child processes finish?**

1. **Adoption by `init` Process**:
   - When a parent process terminates, its child processes become orphaned.
   - Orphaned processes are adopted by the `init` process (PID 1) or a similar system process, which is responsible for *reaping* zombie processes to ensure no resources are leaked.

2. **Child Process Continues Execution**:
   - The child process will continue to execute independently, unaffected by the termination of its parent process.
   - Once the child process completes execution, it will report its termination status to its new parent (`init`).

3. **Zombie Processes**:
   - If the child terminates before being adopted by `init`, it becomes a zombie process temporarily because no parent is there to reap it.
   - The `init` process will eventually clean up the zombie process.

### Why Is This Behavior Important?

- **Orphan Process Handling**:
  - The operating system ensures that child processes are not left unmanaged when their parent terminates.

- **Avoid Resource Leaks**:
  - `init` ensures that orphaned child processes are properly cleaned up (reaped) after termination.

- **System Stability**:
  - This mechanism prevents issues like zombie processes accumulating due to unhandled child processes.

**Unintended Behavior**:
- If the parent terminates prematurely and the child continues running, the child process might produce unintended results if it relies on shared resources (e.g., pipes, files).

> "Reaping" a process refers to the act of the parent process collecting the exit status of a terminated child process. When a child process finishes execution, it transitions to the zombie state until its parent explicitly acknowledges (or reaps) its termination using system calls like `wait()` or `waitpid()`

<hr>

## **`forkarray.c`**
### Memory Duplication and Management During fork()

1. **Memory Duplication During fork()**:
   - When a process calls `fork()`, the operating system creates a new child process.
   - The entire memory space (stack, heap, and data segment) of the parent process is duplicated for the child.
   - The parent and child processes now have separate copies of the memory, even though the initial content and addresses of these copies are the same.

2. **Virtual Memory and Address Space**:
   - Both the parent and child processes have their own virtual memory spaces.
   - The addresses in A and B (e.g., 0x16b22b120 and 0x60000155c2d0) are the same in both processes, but these addresses point to different physical memory locations due to copy-on-write.

3. **Copy-On-Write Mechanism**:
   - The operating system uses a technique called copy-on-write (COW) to optimize memory usage:
     - Initially, the parent and child processes share the same physical memory pages (marked as read-only).
     - When either process attempts to modify the shared memory, the OS creates a new copy of the memory page for the process making the modification.
     - This ensures that changes in one process do not affect the other.

### Why Is This Behavior Important?

- **Efficient Memory Usage**:
  - Copy-on-write allows the system to avoid unnecessary duplication of memory pages, saving physical memory resources.
  
- **Process Isolation**:
  - Each process operates in its own virtual memory space, ensuring that one process's changes do not interfere with another's memory.

- **Performance Optimization**:
  - By delaying the actual copying of memory pages until they are modified, the system can improve performance and reduce overhead.

<hr>

## **`execlp.c`**
```c++
int execlp(const char *file, const char *arg, ..., (char *) NULL);
```

- **`execlp()`**: This function is used to execute a program specified by its name. It **replaces** the current process with the specified program.
  - If the function is successful, it does not return because the current process is replaced by the new program.
  - If it fails (e.g., the file doesn't exist), it returns `-1`, and `errno` is set to indicate the error.

- **Arguments**:
  - `file`: The name of the file to execute. This can be the name of an executable in the system's PATH or a full path to the executable.
  - `arg`: The first argument passed to the program. By convention, this is the name of the program (i.e., the same as file).
  - `...`: Additional arguments to pass to the new program. Terminate this list with `(char *) NULL`.

- **Reason for Same Arguments**:
  - The first argument, `file`, specifies the executable to run.
  - The second argument, `argv[0]`, tells the new program its own name. While you can customize it, it is generally preferred to stick to the convention.

- **`NULL` at the End**:
  - The `NULL` at the end of the arguments in the `execlp` function (and other `exec` family functions) is necessary to indicate the end of the arguments list.

> **Error Handling with `execlp`**:
>  - If `execlp` fails, it returns `-1`, and `errno` is set. Use `perror` or `strerror(errno)` to get a descriptive error message.   

<hr>

## **`execvp.c`**

```c++
int execvp(const char *file, char *const argv[]);
```

Differences Between execlp and execvp
| Feature            | execlp                                      | execvp                                      |
|--------------------|---------------------------------------------|---------------------------------------------|
| Arguments          | Passed as individual parameters (varargs).  | Passed as an array of strings (`argv[]`).     |
| Flexibility        | Requires explicit listing of all arguments. | Easier to use with dynamically generated arguments. |
| Use Case           | Best for a small, fixed number of arguments.| Best when arguments are generated programmatically or dynamic. |
| Syntax Complexity  | Requires adding `(char *)NULL` at the end.    | More compact and intuitive for argument arrays. |

<hr>

**Key Differences Between Exec Variants**:
| Function | Searches PATH? | Takes Command Arguments | Takes Environment Variables |
|----------|----------------|-------------------------|-----------------------------|
| execl    | No             | Yes (variable args)     | No                          |
| execlp   | Yes            | Yes (variable args)     | No                          |
| execv    | No             | Yes (array)             | No                          |
| execvp   | Yes            | Yes (array)             | No                          |
| execve   | No             | Yes (array)             | Yes                         |