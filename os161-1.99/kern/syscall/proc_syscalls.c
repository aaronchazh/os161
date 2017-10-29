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
#include <array.h>
#include <mips/trapframe.h>
#include <synch.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval) {
  struct proc *child = proc_create_runprogram(curproc->p_name);
  if (child == NULL) {
    return ENPROC;
  }
  as_copy(curproc_getas(), &(child->p_addrspace));
  if (child->p_addrspace == NULL) {
    proc_destroy(child);
    return ENOMEM;
  }
  struct trapframe *ctf = kmalloc(sizeof(struct trapframe));
  if (ctf == NULL) {
    proc_destroy(child);
    return ENOMEM;
  }
  memcpy(ctf, tf, sizeof(struct trapframe));

  int fork_check = thread_fork(curthread->t_name, child, &enter_forked_process, ctf, 1);
  if (fork_check) {
    proc_destroy(child);
    kfree(ctf);
    ctf = NULL;
    return fork_check;
  }

  array_add(curproc->children, child, NULL);
  lock_acquire(child->child_lock);
  *retval = child->pid;

  return 0;
}
#endif

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

  #if OPT_A2
  for (int i = array_num(p->children) - 1; i >= 0; i--) {
    struct proc *child = array_get(p->children, i);
    lock_release(child->child_lock);
    array_remove(p->children, i);
  }
  #endif

  //DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

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

  #if OPT_A2
  p->exited = true;
  p->exit_code = _MKWAIT_EXIT(exitcode);
  cv_broadcast(p->process_cv, p->wait_lock);
  DEBUG(DB_SYSCALL,"to reuse: %d\n", p->pid);
  //reuse_pid(p);


  lock_acquire(p->child_lock);
  lock_release(p->child_lock);
  #endif

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
  #if OPT_A2
  *retval = curproc->pid;
  #endif
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
  #if OPT_A2
  struct proc *p = get_kprocess(pid);
  if (p == NULL) {
    return ESRCH;
  }
  if (p == curproc) {
    return ECHILD;
  }

  lock_acquire(p->wait_lock);
  while (!p->exited) {
    cv_wait(p->process_cv, p->wait_lock);
  }
  lock_release(p->wait_lock);

  exitstatus = p->exit_code;
  /* for now, just pretend the exitstatus is 0 */
  #else
  exitstatus = 0;
  #endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}


