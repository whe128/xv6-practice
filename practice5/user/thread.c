#include "types.h"
#include "user.h"
#include "param.h"

struct thread_stack_info{
  int pid;
  void *malloced_stack;
};
// initialize a lock for malloc
lock_t malloc_lock;

struct thread_stack_info thread_stacks[NPROC];

void add_thread_stack_info(int pid, void *stack)
{
  int i;
  for(i=0; i<NPROC; ++i){
    if(thread_stacks[i].pid == 0){
      thread_stacks[i].pid = pid;
      thread_stacks[i].malloced_stack = stack;
      return;
    }
  }
}

void* extract_thread_stack_info(int pid)
{
  int i;
  for(i=0; i<NPROC; ++i){
    if(thread_stacks[i].pid == pid){
      void *stack = thread_stacks[i].malloced_stack;
      thread_stacks[i].pid = 0;
      thread_stacks[i].malloced_stack = 0;
      return stack;
    }
  }
  return 0;
}




int thread_create(void (*start_routine)(void*,void*), void *arg1, void*arg2)
{
  void* stack_buf;
  void* stack_aligned;
  int pid;

  lock_acquire(&malloc_lock);
  stack_buf = malloc(KSTACKSIZE * 2);
  lock_release(&malloc_lock);

  stack_aligned = (void *)(((uint)(stack_buf) + 4095) & ~0xFFF); // align to page size
  // printf(1, "thread_create: allocated stack at %p, aligned stack at %p\n", stack_buf, stack_aligned);
  pid = clone(start_routine, arg1, arg2, stack_aligned);
  if(pid < 0){
    lock_acquire(&malloc_lock);
    free(stack_buf);
    lock_release(&malloc_lock);
    return -1;
  }

  add_thread_stack_info(pid, stack_buf);
  // TODO: implement this for Assignment 4
  return pid;
}

int thread_join()
{
  void* stack;
  void* original_stack;

  int pid = join((void**)&stack);

  // no more child threads
  if(pid < 0)
    return -1;


  original_stack = extract_thread_stack_info(pid);
  // printf(1, "thread_join: freeing stack at%p  --  %p   for thread %d\n", stack, original_stack , pid);
  lock_acquire(&malloc_lock);
  free(original_stack);
  lock_release(&malloc_lock);
  return pid;
}
