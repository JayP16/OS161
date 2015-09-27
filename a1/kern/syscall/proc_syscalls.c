/*
 * Process-related syscalls.
 * New for ASST1.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <pid.h>
#include <machine/trapframe.h>
#include <syscall.h>
#include <signal.h>

/*
 * sys_fork
 * 
 * create a new process, which begins executing in md_forkentry().
 */


int
sys_fork(struct trapframe *tf, pid_t *retval)
{
	struct trapframe *ntf; /* new trapframe, copy of tf */
	int result;

	/*
	 * Copy the trapframe to the heap, because we might return to
	 * userlevel and make another syscall (changing the trapframe)
	 * before the child runs. The child will free the copy.
	 */
	ntf = kmalloc(sizeof(struct trapframe));
	if (ntf==NULL) {
		return ENOMEM;
	}
	*ntf = *tf; /* copy the trapframe */

	result = thread_fork(curthread->t_name, enter_forked_process, 
			     ntf, 0, retval);

	if (result) {
		kfree(ntf);
		return result;
	}

	return 0;
}

/*
 * sys_getpid
 * 
 * Get process id
 */
int
sys_getpid(pid_t *retval)
{	
	*retval = curthread->t_pid;
	return 0;
}

/*
 * sys_waitpid
 *
 *Wait for a process to exit
 */
 int
 sys_waitpid(pid_t pid, int *status, int options, pid_t *retval)
 {
 	int exitstatus;

 	exitstatus = pid_join(pid, status, options);
 	//Return exitstatus
 	*retval = exitstatus;
 	if (exitstatus < 0){
 		return -1;
 	}
 	return 0;
 }


/*
 * sys_kill
 *
 * Send signal to a process
 */
int 
sys_kill(pid_t pid, int sig, int *retval){
	
	/*	
	1	SIGHUP	terminate process
	2	SIGINT	terminate process
	9	SIGKILL	terminate process
	15	SIGTERM	terminate process
	17	SIGSTOP	stop process from executing until SIGCONT received
	19	SIGCONT	continue after SIGSTOP received
	28	SIGWINCH ignore signal
	29	SIGINFO	ignore signal
	*/

	//Set signal, and handle the signal in the pid_setsignal function.
	int result = pid_setsignal(pid, sig);
	*retval = result;

	// if the result getting back from the pid_setsignal is 0
	if (result != 0){
		return -1;
	}
	
	return 0;
}

