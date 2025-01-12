Signal-Specific Behavior
|Signal	|Sent By Parent	|Handled By Child	|Default Behavior (If Unhandled)|
|---------|----------------|---------------------|------------------------------|
|`SIGUSR1`|	Custom logic	| Custom handler     |  Termination                                 |
|`SIGUSR2`|	Custom logic	| Custom handler     |  Termination                                 |
|`SIGTSTP`|	Suspend child	| Default handler    |	Suspends the process (same as `Ctrl+Z`).      |
|`SIGCONT`|	Resume child	| Default handler    |	Resumes execution of a suspended process.   |
|`SIGINT` | Terminate child	| Default handler    |	Terminates the process (same as `Ctrl+C`).    |


### Checking the status of out process

```sh
ps aux | grep a.out
```


- `ps aux`: This command lists all running processes with detailed information. The `a` option shows processes for all users, `u` provides a user-oriented format, and `x` includes processes without a controlling terminal.

- `grep a.out`: This filters the output to show only lines containing `a.out`, which is the name of the process you're interested in.

This command will display information about the `a.out` process, including its PID, CPU usage, memory usage, and more. If you need more specific details, you can adjust the `ps` options accordingly.