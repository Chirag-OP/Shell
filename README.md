# My shell
A simple Unix shell implemented in C++ to understand how shells work internally, including process creation, command execution, pipes, I/O redirection, and basic shell built-ins

## Status
**currently building**

## Features

### Implemented
- Built-in Commands
- External Commands 
- Output Redirection
- Quotes Parsing
- Pipes

### Planned
- Command History
- Jobs
- KeyBoard Interrupts

## Design Decisions
### Why use `std::string` and `std::vector<std::string>` in the `Command` struct?

I use `std::string` and `std::vector<std::string>` because they automatically manage memory, so I don't have to manually allocate or free memory. This makes the code safer and avoids issues such as memory leaks and dangling pointers.
When executing external commands (e.g., with `execvp`), I convert the strings to C-style strings using `data()` (or `c_str()`) and build a `char*[]` argument array. This conversion does **not** allocate new memory. Instead, `data()` simply returns a pointer to the character buffer already owned by the corresponding `std::string`.
As a result, memory management is still handled entirely by the `std::string` objects, while the C API receives the `char*` pointers it requires.

## Technical Challenges Faced and their Solutions
### Input reading issue
**Problem:** 
i initally used `getline` to read user input which creates problem when implementing `signals`. Since when a background process ends OS sends a signal which interrupts the current running process so read returns -1 with `errno: EINTR`, but getline doesn't retry on seeing this error no, and treat it as `EOF` so gets stuck in reading empty input forever as soon as signal arrives.

**Initial Aprroach** 
Set `SA_RESTART` flag in signal. Now as soon as OS does some intruption it restarts the part which was intrupted, so getline gets restarted. But this causes a **problem** since getline restarted we wont see the ouput which should have been printed due to signal until we press enter.

**Solution** 
simply use `read` command and handle `errno:EINTR`. Also now we need to use `flush` with cout to print ouput since we are no longer using `cin` hence no automatic flushing happens.

### Auto Shell Stopping after Job control
**Problem** 
for job control shell gives the foreground process group the controlling terminal and retakes it back after foreground child process ends, but since after giving it the control terminal, shell itself is a background process when it tries to retake controlling terminal it recieves `SIGTTOU` flag and is stopped.

**Initial Aprroach** 
make the shell ignore the `SIGTTOU` flag using `signal(SIGTTOU, SIG_IGN)`. But now since child proceses inherit it, hence they also start ignoring this flag but we donot want that

**Solution** 
make the shell ignore the `SIGTTOU` flag using `signal(SIGTTOU, SIG_IGN)` and as soon as child is created make it accept this `SIGTTOU` flag using `signal(SIGTTOU, SIG_DFL)`
