#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "procinfo.h"
#include "procstat.h"
#include "pageinfo.h"

extern pte_t *walkpgdir(pde_t *pgdir, const void *va, int alloc);
extern int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
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
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// A1 --
// shutdown the system
int
sys_halt(void)
{
  outw(0x604, 0x2000);

  return 0; // never reached if shutdown successful
}

// stub
int sys_trace(void)
{
  return -1;
}

// stub
int sys_procinfo(void)
{
  return -1;
}

// -- A1


// A2 --

int sys_setpriority(void)
{
  int pid, n;

  // get the first argument, which is the process id we want to change the priority of
  if(argint(0, &pid) < 0)
    return -1;

  // get the second argument, which is the new priority
  if(argint(1, &n) < 0)
    return -1;

  return set_priority(pid,n);
}


int sys_procstat(void)
{
  int n;
  char *arg;
  n = argptr(0, &arg, sizeof(struct procstat_t));
  if(n < 0)
    return -1;
  return getprocstat((struct procstat_t *) arg);
}

int sys_schedlatency(void)
{
  int n;

  // get the first argument, which is the trace mask
  if(argint(0, &n) < 0)
    return -1;

  return set_schedlatency(n);
}

// -- A2

// A3 --

int sys_mprotect(void)
{
  // TODO: implement this
  // read the arguments (addr, len) from the user stack
  char *addr;
  int len;
  int npages;

  if (argint(1, &len) < 0)
  {
    return -1;
  }

  if (argptr(0, &addr, sizeof(len * PGSIZE)) < 0)
  {
    return -1;
  }

  // len is zero or negative
  if (len <= 0)
  {
    return -1;
  }
  // align addr to page boundary
  if ((uint)addr % PGSIZE != 0)
  {
    return -1;
  }
  npages = PGROUNDUP(len) / PGSIZE;

  // not currently a part of the address space
  for (int i = 0; i < npages; i++)
  {
    pte_t *pte = walkpgdir(myproc()->pgdir, addr + i * PGSIZE, 1);
    // no page table entry
    if (pte == 0 || !(*pte & PTE_P))
    {
      return -1;
    }
  }

  // valid addr and len, now set the mprotection
  for (int i = 0; i < npages; i++)
  {
    // get the page table entry for the page
    pte_t *pte = walkpgdir(myproc()->pgdir, addr + i * PGSIZE, 1);
    // clear the PTE_U bit to set the page as read-only
    if (pte)
    {
      *pte &= ~PTE_W; // clear the write bit
    }
  }

  // V2P-> transfer virtual address to physical address
  lcr3(V2P(myproc()->pgdir)); // refresh the page table
  return 0;
}

int sys_munprotect(void)
{
  // TODO: implement this
  char *addr;
  int len;
  int npages;

  if (argint(1, &len) < 0)
  {
    return -1;
  }

  if (argptr(0, &addr, sizeof(len * PGSIZE)) < 0)
  {
    return -1;
  }

  // len is zero or negative
  if (len <= 0)
  {
    return -1;
  }
  // align addr to page boundary
  if ((uint)addr % PGSIZE != 0)
  {
    return -1;
  }
  npages = PGROUNDUP(len) / PGSIZE;

  // not currently a part of the address space
  for (int i = 0; i < npages; i++)
  {
    pte_t *pte = walkpgdir(myproc()->pgdir, addr + i * PGSIZE, 1);
    // no page table entry
    if (pte == 0 || !(*pte & PTE_P))
    {
      return -1;
    }
  }

  // valid addr and len, now set the mprotection
  for (int i = 0; i < npages; i++)
  {
    // get the page table entry for the page
    pte_t *pte = walkpgdir(myproc()->pgdir, addr + i * PGSIZE, 1);
    // clear the PTE_U bit to set the page as read-only
    if (pte)
    {
      *pte |= PTE_W; // clear the write bit
    }
  }

  return 0;
}

// query information about a virtual address
int sys_pageinfo(void)
{
  int n;
  char *s;
  if(argint(0, &n) < 0)
    return -1;
  if(argptr(1, &s, sizeof(struct pageinfo_t)) < 0)
    return -1;
  return pageinfo((void *)n, (void *)s);
}

int sys_mmap(void)
{
  void *addr;
  int len;
  int prot;

  void *start_addr;
  uint allocate_size;

  if (argint(0, (int *)&addr))
    return -1;
  if (argint(1, &len) < 0)
    return -1;
  if (argint(2, &prot) < 0)
    return -1;

  if (len <= 0)
    return -1;

  allocate_size = PGROUNDUP(len);

  start_addr = (void *)PGROUNDUP((uint)addr);

  if ((uint)start_addr < MMAPBASE)
    start_addr = (void *)(PGROUNDUP(MMAPBASE));

  while (1)
  {
    if ((uint)start_addr + allocate_size >= KERNBASE)
      goto not_find;

    int not_enough_space = 0;
    for (uint a = (uint)start_addr; a < (uint)start_addr + allocate_size; a += PGSIZE)
    {
      // get the page table entry, and check if mapped
      pte_t *pte = walkpgdir(myproc()->pgdir, (void *)a, 0);

      if (pte && (*pte & PTE_P))
      {
        //  already mapped, move to next page from the mapped page
        start_addr = (char *)a + PGSIZE;
        // break the for loop to start over
        not_enough_space = 1;
        break;
      }
    }
    if (not_enough_space)
      continue;

    // if we reach here, means all pages are free
    break;
  }

  // allocate pages
  uint a;
  for (a = (uint)start_addr; a < (uint)start_addr + allocate_size; a += PGSIZE)
  {
    char *mem = kalloc();

    // allocate failed, need to free previously allocated pages
    if (mem == 0)
    {
      // a
      goto allocation_map_failed;
    }
    memset(mem, 0, PGSIZE);
    int permission = PTE_U;
    // can write
    if (prot == 1)
      permission |= PTE_W;
    if (mappages(myproc()->pgdir, (char *)a, PGSIZE, V2P(mem), permission) < 0)
    {
      // free the last allocated page
      kfree(mem);
      // need to free previously allocated pages
      goto allocation_map_failed;
    }
  }

  myproc()->sz = ((uint)start_addr + allocate_size) > myproc()->sz ? (uint)start_addr + allocate_size : myproc()->sz;
  return (int)start_addr;

allocation_map_failed:
  // free previously allocated pages
  deallocuvm(myproc()->pgdir, a, (uint)start_addr);
  return -1;

not_find:
  // TODO: implement this
  return -1;
}

int sys_munmap(void)
{
  void *addr;
  int len;

  if (argptr(0, (char **)&addr, sizeof(void *)) < 0)
    return -1;

  if (argint(1, &len) < 0)
    return -1;
  if (len <= 0)
    return -1;
  if ((uint)addr % PGSIZE != 0)
    return -1;

  deallocuvm(myproc()->pgdir, (uint)addr + len, (uint)addr);

  if ((uint)addr + len == (uint)(myproc()->pgdir))
    myproc()->sz = (uint)addr;
  // TODO: implement this
  return 0;
}

// -- A3
