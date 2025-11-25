#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "procinfo.h"
#include "spinlock.h"

extern struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;
extern long xtime; // current time; defined in proc.c

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// A1 --
// shutdown the system
int sys_halt(void)
{
  outw(0x604, 0x2000);

  return 0; // never reached if shutdown successful
}
// -- A1

int sys_trace(void)
{
  int mask;
  if (argint(0, &mask) < 0)
  {
    return -1;
  }

  myproc()->tmask = mask;
  return 0;
}

void state_to_string(enum procstate state, char *str)
{
  switch (state)
  {
  case UNUSED:
    safestrcpy(str, "UN", 3);
    break;
  case EMBRYO:
    safestrcpy(str, "EM", 3);
    break;
  case SLEEPING:
    safestrcpy(str, "S", 3);
    break;
  case RUNNABLE:
    safestrcpy(str, "RE", 3);
    break;
  case RUNNING:
    safestrcpy(str, "RN", 3);
    break;
  case ZOMBIE:
    safestrcpy(str, "Z", 3);
    break;
  default:
    safestrcpy(str, "??", 3);
    break;
  }
}

int sys_procinfo(void)
{
  int n;
  struct procinfo_t *user_buf;
  int count = 0;

  // get max argument
  if (argint(0, &n) < 0)
    return -1;

  if (argptr(1, (char **)&user_buf, n * sizeof(struct procinfo_t)) < 0)
    return -1;

  if (n <= 0 || user_buf == 0)
    return -1;

  acquire(&ptable.lock);
  for (int i = 0; i < NPROC; i++)
  {
    // no enough space to store more processes
    if (count >= n)
    {
      break;
    }

    struct proc *p = &ptable.proc[i];

    if (p->state == UNUSED)
    {
      continue;
    }

    user_buf[count].pid = p->pid;
    safestrcpy(user_buf[count].name, p->name, sizeof(user_buf[count].name));
    state_to_string(p->state, user_buf[count].st);
    user_buf[count].start_time = p->start_time;
    user_buf[count].elapsed_time = xtime - p->start_time;

    count++;
  }
  release(&ptable.lock);

  return count;
}
