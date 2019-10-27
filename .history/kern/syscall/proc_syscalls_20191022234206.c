#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval) {
  // Step 1: Create process structure for child process
  struct proc *new_child = proc_create_runprogram(curproc->p_name); // perhaps we want a diff name for child
  if (new_child == NULL) {
    panic("The new (child) process is null!");
  }
  // Step 2: Create and copy address space
  int error = as_copy(curproc_getas(), &(new_child->p_addrspace));
  if (error != 0) {
    proc_destroy(new_child);
    panic("as_Copy: ENOMEM");
  }
  // Step 3: Assign PID to child process (DONE IN PROC.C) and create the parent/child relationship
  new_child->parent = curproc;

  // add to children_info array
  struct proc_info *child_info = kmalloc(sizeof(struct proc_info));
  child_info->proc_addr = child;
  child_info->exit_code = -1;
  child_info->pid = child->pid;
  array_add(curproc->children_info, child_info, NULL);

  // copy over address space
  
  if (err != 0) panic("copy address space errored");
  // probably don't need this function since we are directly copying address space into child
  // address space
  /* proc_setas(child->p_addrspace, child); */

  // create temp trapframe
  curproc->tf = kmalloc(sizeof(struct trapframe));
  KASSERT(curproc->tf != NULL);
  memcpy(curproc->tf, tf, sizeof(struct trapframe));

  // fork thread with temp trapframe
  thread_fork(child->p_name, child, (void *)&enter_forked_process, curproc->tf, 10);

  *retval =  child->pid;

  return(0);
}

#endif



  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

