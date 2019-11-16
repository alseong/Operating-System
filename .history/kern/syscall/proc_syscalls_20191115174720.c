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

#include <kern/fcntl.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>

#include "opt-A2.h"

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval) {
  // Step 1: Create process structure for child process
  struct proc *new_child = proc_create_runprogram(curproc->p_name);
  if (new_child == NULL) {
    panic("The new (child) process is null!");
    proc_destroy(new_child);
  }
  // Step 2: Create and copy address space
  lock_acquire(new_child->pLock); 
    int error = as_copy(curproc_getas(), &new_child->p_addrspace);
  lock_release(new_child->pLock);

  if (error != 0) {
    proc_destroy(new_child);
    panic("as_Copy: ENOMEM");
  }

  // Step 3: Assign PID to child process (DONE IN PROC.C) and create the parent/child relationship
  lock_acquire(curproc->pLock);
    new_child->parent = curproc;
  lock_release(curproc->pLock);

  // Step 4: Add to curproc's array of children
  lock_acquire(curproc->pLock);
    array_add(curproc->children, new_child, NULL);
  lock_release(curproc->pLock);

  // Step 5: Create Thread for child process
  struct trapframe* tf_copy = kmalloc(sizeof(struct trapframe)); // Trapframe of parent may change by the time child access it
  memcpy(tf_copy, tf, sizeof(struct trapframe));
  int err = thread_fork(new_child->p_name, new_child, (void *)&enter_forked_process, tf_copy, 0); //What is the last value used for?
  if (err) {
    kfree(tf_copy);
    proc_destroy(new_child);
    panic("Error: Issue with threadfork");
  }
  *retval =  new_child->PID;

  return(0);
}

#endif



  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */
//terminate the calling process
//passed parameter - if parent is alive, keep parameter around as parent can call waitpid on you
//freely delete etire processs only if parent is not alive / zombie
void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  //(void)exitcode;

  #if OPT_A2
  lock_acquire(p->pLock);
    p->zombie = true;
    p->exitCode = _MKWAIT_EXIT(exitcode);
  lock_release(p->pLock);

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
//1 is process whihc called waitpid my child
//if child and dead, grab exit code and status and merge to singlue int value -> return from waitpid
//child dead, grab exit code and done (destroy)

//if child is alive go to sleep -parent sleep on childs cv, child calls exit, child signal cv to wake pare up who can grab exit staus, fully delete you and done
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
  

  #if OPT_A2
  //find if parameter pid is a child of the current process
  lock_acquire(curproc->pLock);
  bool isChild = false;
  struct proc *child;
  for (unsigned int i = 0; i < array_num(curproc->children); i ++) {
    child = array_get(curproc->children, i);
    if (child->PID == pid ) {
      isChild = true;
      break;
    }
  }
  lock_release(curproc->pLock);
  if (isChild) {
    
      lock_acquire(child->pLock);
      while (!child->zombie) {
       cv_wait(child->p_cv, child->pLock);
      }
      lock_release(child->pLock);
      exitstatus = child->exitCode;
      lock_acquire(curproc->pLock);
        struct proc *child_temp;
        for (unsigned int i =0; i < array_num(curproc->children); i++) {
          child_temp = array_get(curproc->children, i);
          if (child_temp->PID == pid) {
            array_remove(curproc->children, i);
            break;
          }
        }
      lock_release(curproc->pLock);
      proc_destroy(child); 
  }
  else {
    panic("Parent does not have children with pid");
  }
  #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
int sys_execv(const char * progname, char ** args, int * retval) //NOT CONST
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
  (void) args;
  (void) retval;

//COPY PROGRAM NAME FROM USER SPACE TO KERNEL ADDRESS SPACE (PROG PATH)
  char * prog_alloc_kernel = kmalloc((strlen(progname) + 1) * sizeof(char));
  if (prog_alloc_kern == NULL) {
    panic("No program is allocated to kernel!");
  }
  int err = copyinstr((const_userptr_t) progname, (void *) prog_alloc_kernel, (strlen(progname) + 1) * sizeof(char));
  KASSERT(err == 0);
//COPY COUNT AND ALL THE ARGS FROM USER SPACE TO KERNELS ADD SPACE (ARRAY OF ARGS)


	/* Open the file. */
	result = vfs_open(prog_alloc_kern, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate(); //DOESNT ACTIVATE (CLEARS TLB)

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

//CREATE THE STACK (LIKE AS DEFINE STACK, but pass all argument we extracted 
//from calling program to new program 
//by populating the new program's stack with all the args from the old program's stack  ) ( CREATE A SEPARATE FUNCTION FOR )

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}
  
  kfree(prog_alloc_kern);

	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

}

#endif
