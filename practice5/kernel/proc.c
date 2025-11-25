#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "procinfo.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->tmask = 0; // initialise tmask
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
      // allocate only one page for kernel stack; on
      // failure of kalloc of stack
      p->state = UNUSED;
    return 0;
  }
  // set the sp pointer at the top of the stack
  // kstack is the start address of the stack
  // sp is the end address of the stack
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  // top size of stack is for trap frame
  sp -= sizeof *p->tf; //*(p->tf)
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  // next, put 4byte for return address of trapret
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  // next, put size of context structure
  sp -= sizeof *p->context;                  //*(p->context)
  p->context = (struct context *)sp;         // set the context pointer
  memset(p->context, 0, sizeof *p->context); // clear the context
  p->context->eip = (uint)forkret;           //

  // top to down -> trapfram-> trapret -> context
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }

  // sychronize the size for threads sharing the same pgdir
  for(struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pgdir == curproc->pgdir && p != curproc) {
      p->sz = sz;
    }
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    // failure of copyuvm
    kfree(np->kstack);  // one page
    np->kstack = 0;     // clear the pointer
    np->state = UNUSED; // set the state to UNUSED
    return -1;
  }

  // success
  np->sz = curproc->sz;   // size of process memory
  np->parent = curproc;   // parent process
  *np->tf = *curproc->tf; // copy trap frame


  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  // copy the open files
  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);

  // copy the current working directory
  np->cwd = idup(curproc->cwd);

  // copy the process name
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  // copy trace mask from parent
  np->tmask = curproc->tmask;

  // new process pid
  pid = np->pid;

  // set the process table
  acquire(&ptable.lock);

  np->state = RUNNABLE; // set the state to RUNNABLE

  // release the process table lock
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  if (curproc->ustack == 0)
  {
    // Close all open files.
    for (fd = 0; fd < NOFILE; fd++)
    {
      if (curproc->ofile[fd])
      {
        fileclose(curproc->ofile[fd]);
        curproc->ofile[fd] = 0;
      }
    }

    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;
  }

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      if(p->ustack != 0)
        p->killed = 1; // kill threads

      p->parent = initproc;

      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;

  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      // init proc can clean all thread and processe children
      // other processes can only clean their own processe children
      if(p->ustack != 0 && curproc != initproc)
        continue; // skip threads

      havekids = 1;
      if(p->state == ZOMBIE){

          // Found one.
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;

          if(p->ustack){
            // thread
            p->ustack = 0;
          } else {
            // process
            freevm(p->pgdir);
          }

          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          p->state = UNUSED;
          release(&ptable.lock);
          return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.


// Round robin scheduler
void
rr_scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  int norunnable=1;
  c->proc = 0;

  for(;;){
    // Enable interrupts on this processor.
    sti();
    norunnable=1;
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      norunnable=0;
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

    if(norunnable) hlt(); // put cpu in a sleep state, if no runnable processes in the queue (contribution by Daniel Herald)
  }
}

void scheduler(void)
{
  rr_scheduler();
  __builtin_unreachable(); // this is to tell the compiler that this function does not return
}


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


int join(void **stack)
{
  struct proc *p;
  int havekids, pid;
  acquire(&ptable.lock);
  for (;;)
  {
    havekids = 0;

    // traverse whole process table looking for child threads
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      // no child, no ustack , not same address space
      if (p->parent != myproc() || p->ustack == 0 || p->pgdir != myproc()->pgdir)
        continue;

      havekids = 1;

      // if the child thread exit, it will become zombie, because main process does not wait for it
      if (p->state == ZOMBIE)
      {
        pid = p->pid;
        *stack = p->ustack; // return the user stack address
        kfree(p->kstack);   // free kernel stack
        p->ustack = 0;      // clear ustack pointer
        p->kstack = 0;
        p->pid = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        p->parent = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    if (!havekids || myproc()->killed)
    {
      *stack = 0;
      release(&ptable.lock);
      return -1;
    }
    sleep(myproc(), &ptable.lock); // wait for child thread to exit
  }
  // TODO: implement this for Assignment 4
}

int clone(void (*fcn)(void *, void *), void *arg1, void *arg2, void *stack)
{
  // top to down -> trapfram-> trapret -> context
  struct proc *curproc = myproc();
  struct proc *np;
  char *user_sp;
  // check the stack pointer alignment
  if (stack == 0 || ((uint)stack % PGSIZE) != 0)
    return -1;

  // allocate a new process
  if ((np = allocproc()) == 0)
  {
    // failure of copyuvm
    kfree(np->kstack);  // one page
    np->kstack = 0;     // clear the pointer
    np->state = UNUSED; // set the state to UNUSED
    return -1;
  }
  acquire(&ptable.lock);
  // page table of the new process(share)
  np->pgdir = curproc->pgdir; // share the page table
  np->sz = curproc->sz;       // size of process memory
  *np->tf = *curproc->tf;     // copy trap frame
  np->parent = curproc;       // parent process
  np->ustack = stack;         // user stack for threads
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;


  // first set at top of stack
  user_sp = (char *)stack + PGSIZE;

  // arg2
  user_sp -= 4;
  *(uint *)user_sp = (uint)arg2;

  // arg1
  user_sp -= 4;
  *(uint *)user_sp = (uint)arg1;

  // fake
  user_sp -= 4;
  *(uint *)user_sp = (uint)0xffffffff;

  np->tf->esp = (uint)user_sp;
  np->tf->eip = (uint)fcn; // set to function address

  // set open files and current working directory
  for (int i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  // copy trace mask from parent
  np->tmask = curproc->tmask;
  np->state = RUNNABLE;

  release(&ptable.lock);
  // TODO: implement this for Assignment 4
  return np->pid;
}
