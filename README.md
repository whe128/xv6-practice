# XV6 Extended Features

This project extends the basic XV6 operating system with additional system calls, thread support, scheduler enhancements, and memory management features. It is intended for learning OS development and exploring low-level system programming concepts.

---

## Practice

### 1.

- `pipe` implementation
- `find` programe(all file and directory at the cwd)

### 2.

- `SYSCALL(halt)`
- `SYSCALL(time)`
- `SYSCALL(stime)`
- `SYSCALL(trace)`
- `SYSCALL(procinfo)`

### 3.

- `scheduler latency`
- `Round-Robin scheduler`
- `Lottery scheduler`
- `Hybrid scheduler`: delay = -1 first, then tickets based

### 4.

- `Relocate` user space processes, not start at 0x00
- `Page fault` hander implmentation in the trap.
- `mprotect` and `munprotect`: set write permission for the certain page.
- `mmap` and `munmap`: map the memory and directly operate memory to write/read files/devices.

### 5.

- `clone()`: syscall to create process-based thread (share page table and address space)
- `join()`: wait for the thread exit, ignore the process exit.
- `thread_create()`: thread library to call clone() to create thread. Malloc stack in the function.
- `thread_join()`: thread library to call joind() to release the tread and also free the allocated stack.


## Features

### 1. System Call Enhancements

- **`syscall` handling (`0x40 trap`)**
  Implemented system call dispatching via trap vector 0x40.
- **`procinfo` syscall**
  Query process information including PID, state, parent, and memory usage.
- **`time` syscall**
  Return system time for processes.

### 2. Thread Management

- **`clone` syscall**
  - Create threads by sharing the process page directory (`pgdir`).
  - Arguments (`arg1`, `arg2`) and return value are placed on the thread stack.
  - Allocates stack and updates shared memory size (`sz`) safely with locks.
- **`join` syscall**
  - Wait for a thread to exit.
  - Cleans up thread resources without freeing VM (unless it is a process).
- **Thread exit handling**
  - Closes files on thread exit.
  - Process exit (`exit`) also cleans up child threads.
- **Thread vs. Process wait logic**
  - `wait` skips threads but waits for processes to exit and frees VM.
  - `join` skips processes but waits for threads to exit.

### 3. Scheduler Enhancements

- **Round Robin**
  - Standard time-slice scheduling with latency tracking and next runnable selection.
- **Lottery Scheduler (Tickets)**
  - Random selection of runnable process based on ticket count.
- **Hybrid Scheduler**
  - Initially uses delay-1 scheduling, then switches to ticket-based lottery.

### 4. Memory Management

- **Page fault handling**
  - Added custom trap handler for page faults.
- **Permission management**
  - `protect`/`unprotect` syscall to change memory page permissions.
- **`mmap` support**
  - Allows dynamic mapping of memory regions.
- **Move entry address (`sz`)**
  - Adjusts user process entry addresses from `userBase` for threads and memory management.

### 5. Pipe Implementation

- Supports basic inter-process communication using pipes.

### 6. Debugging & Utilities

- **GDB support**
  - Debug kernel and user processes with breakpoints.
- **Poweroff syscall**
  - Gracefully shuts down the system.
- **Tracing syscall implementation**
  - Track system call execution for debugging.

---

## Usage

1. Build and run XV6 with your modified kernel:

```bash
make qemu
```
