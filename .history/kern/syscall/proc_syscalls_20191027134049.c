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
#include <kern/errno.h>
#include <mips/trapframe.h>
#include "opt-A2.h"

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval) {
  // Step 1: Create process structure for child process
  struct proc *new_child = proc_create_runprogram(curproc->p_name);
  if (new_child == NULL) {
    panic("The new (child) process is null!");
    proc_destroy(new_child);
    return ENOMEM;
  }
  // Step 2: Create and copy address space
  spinlock_acquire(&new_child->p_lock); 
    int error = as_copy(curproc_getas(), &new_child->p_addrspace);
  spinlock_release(&new_child->p_lock);

  if (error != 0) {
    proc_destroy(new_child);
    panic("as_Copy: ENOMEM");
    return ENOMEM;
  }

  // Step 3: Assign PID to child process (DONE IN PROC.C) and create the parent/child relationship
  spinlock_acquire(&curproc->p_lock);
    new_child->parent = curproc;
  spinlock_release(&curproc->p_lock);

  // Step 4: Add to curproc's array of children
  spinlock_acquire(&curproc->p_lock);
    array_add(curproc->children, new_child, NULL);
  spinlock_release(&curproc->p_lock);

  // Step 5: Create Thread for child process
  struct trapframe* tf_copy = kmalloc(sizeof(struct trapframe)); // Trapframe of parent may change by the time child access it
  DEBUG(DB_SYSCALL, "sys_fork: Created new trap frame\n");
  memcpy(tf_copy, tf, sizeof(struct trapframe));
  int err = thread_fork(new_child->p_name, new_child, (void *)&enter_forked_process, tf_copy, 0); //What is the last value used for?
  if (err) {
    kfree(tf_copy);
    proc_destroy(new_child);
    return ENOMEM;
  }

  DEBUG(DB_SYSCALL, "sys: fork created successfully\n");
  *retval =  new_child->PID;

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
  //(void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  #if OPT_A2
  //spinlock_acquire(&p->p_lock);
    p->zombie = true;
    p->exitCode = _MKWAIT_EXIT(exitcode);
  //spinlock_release(&p->p_lock);

  lock_acquire(p->pLock);
    cv_signal(p->p_cv, p->pLock);
  lock_release(p->pLock);
  #endif

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
  #if OPT_A2
  if (p->parent == NULL || p->parent->zombie){
    proc_destroy(p);
  }
  #endif
  
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
  #if OPT_A2
  *retval = curproc->PID;
  #endif
  //*retval = 1;
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
  //exitstatus = 0;

  #if A2_OPT
  //find if parameter pid is a child of the current process
  //lock_acquire(curproc->pLock);
  struct proc* child;
  bool isChild = false;
  for (int i = 0; i < array_num(curproc->children); i ++) {
    child = array_get(curproc->children, i);
    if (child->pid == pid ) {
      isChild = true;
      break;
    }
  }
  //lock_release(curproc->pLock);
  if (isChild) {
    lock_acquire(child->pLock);
     while (!child->zombie) {
       
       cv_wait(child->p_cv, child->pLock);
       
       
     }
     lock_release(child->pLock);
     exitstatus = child->exitcode;
    //  else {
    //    KASSERT(child->zombie);
    //    exitstatus = child->exitcode; //combine to single interger value??? HOW
    //  }
  }
  //remove from parent children list
  //proc_destroy(child); 
  else {
    panic("Parent does not have children with pid");
  }
  DEBUG(DB_SYSCALL, "waitpid A2 exit");
  #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

