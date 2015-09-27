/* BEGIN A3 SETUP */
/*
 * File handles and file tables.
 * New for ASST3
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/unistd.h>
#include <file.h>
#include <syscall.h>
#include <array.h>
#include <thread.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/fcntl.h>

/*** openfile functions ***/

/*
 * file_open
 * opens a file, places it in the filetable, sets RETFD to the file
 * descriptor. the pointer arguments must be kernel pointers.
 * NOTE -- the passed in filename must be a mutable string.
 * 
 * A3: As per the OS/161 man page for open(), you do not need 
 * to do anything with the "mode" argument.
 */
int
file_open(char *filename, int flags, int mode, int *retfd)
{

	struct vnode* vn;
	struct filedesc* fd;
	int result;

	fd = (struct filedesc *)kmalloc(sizeof(struct filedesc));
	if(fd == NULL){
		return -1;
	}

	result = vfs_open(filename, flags, mode,  &vn);
	if(result){
		return result;
	}
	fd->vn = vn;
	fd->offset = 0;
	fd->flags = flags & O_ACCMODE;
	*retfd = filetable_add(curthread->t_filetable, fd);
	if(*retfd == -1){
		vfs_close(vn);
		kfree(fd);
		return *retfd;
	}
	return 0;
}


/* 
 * file_close
 * Called when a process closes a file descriptor.  Think about how you plan
 * to handle fork, and what (if anything) is shared between parent/child after
 * fork.  Your design decisions will affect what you should do for close.
 */
int
file_close(int fd)
{
    int result;
    result = filetable_remove(curthread->t_filetable, fd);

    if(result == -1){
    	return EBADF;
    }

	return 0;
}

/*** filetable functions ***/

/* 
 * filetable_init
 * pretty straightforward -- allocate the space, set up 
 * first 3 file descriptors for stdin, stdout and stderr,
 * and initialize all other entries to NULL.
 * 
 * Should set curthread->t_filetable to point to the
 * newly-initialized filetable.
 * 
 * Should return non-zero error code on failure.  Currently
 * does nothing but returns success so that loading a user
 * program will succeed even if you haven't written the
 * filetable initialization yet.
 */

int
filetable_init(struct filetable* table)
{

	struct vnode* vn0;
	struct vnode* vn1;
	struct vnode* vn2;
	struct filedesc* fd0;
	struct filedesc* fd1;
	struct filedesc* fd2;
	int result0;
	int result1;
	int result2;


	// continue(attach STDIN, STDOUT, STDERR)
	fd0 = (struct filedesc *)kmalloc(sizeof(struct filedesc));

	if(fd0 == NULL){
		filetable_destroy(table);
		return 1;
	}
	char temp0[] = "con:";
	result0 = vfs_open(temp0, O_RDONLY, 0, &vn0);

	if(result0){
        kprintf("Unable to attach to stdin: %s\n", strerror(result0));
        vfs_close(vn0);
        filetable_destroy(table);
        return 1;
	}

	fd0->vn = vn0;
	fd0->dup_cnt = 0;
	fd0->flags = O_RDONLY;
	fd0->offset = 0;
	table->ftable[0] = fd0;

	fd1 = (struct filedesc *)kmalloc(sizeof(struct filedesc));

	if(fd1 == NULL){
		filetable_destroy(table);
		return 1;
	}
	temp0[3] = ':';
	result1 = vfs_open(temp0, O_WRONLY, 0, &vn1);

    if(result1){
    	kprintf("Unable to attach to stdout: %s\n", strerror(result1));
        vfs_close(vn1);
        filetable_destroy(table);
        return 1;
    }

	fd1->vn = vn1;
	fd1->dup_cnt = 0;
	fd1->flags = O_WRONLY;
	fd1->offset = 0;
	table->ftable[1] = fd1;

	fd2 = (struct filedesc *)kmalloc(sizeof(struct filedesc));

	if(fd2 == NULL){
		filetable_destroy(table);
		return 1;
	}
	temp0[3] = ':';
	result2 = vfs_open(temp0, O_WRONLY, 0, &vn2);

    if(result2){
    	kprintf("Unable to attach to stderr: %s\n", strerror(result2));
        vfs_close(vn2);
        filetable_destroy(table);
        return 1;
    }

	fd2->vn = vn2;
	fd2->dup_cnt = 0;
	fd2->flags = O_WRONLY;
	fd2->offset = 0;
	table->ftable[2] = fd2;

	return 0;
}


/*
 * filetable_destroy
 * closes the files in the file table, frees the table.
 * This should be called as part of cleaning up a process (after kill
 * or exit).
 */
void
filetable_destroy(struct filetable *ft)
{
	for(int i =0; i < __OPEN_MAX; i++){
		filetable_remove(ft, i);
	}
	kfree(ft);
}	

int filetable_add(struct filetable* table, struct filedesc* fd){

	KASSERT(table);
	KASSERT(fd);
	for(int i = 3; i < __OPEN_MAX; i++){
		if(table->ftable[i] == NULL){
			table->ftable[i] = fd;
			return i;
		}
	}
	return EMFILE;
}

int filetable_remove(struct filetable* table, int index){

	KASSERT(table);
	KASSERT(index);

	struct filedesc* fd = table->ftable[index];
	if(fd != NULL){
		vfs_close(fd->vn);
		kfree(fd);
		table->ftable[index] = NULL;
		return 0;
	}

	return -1;

}


/* 
 * You should add additional filetable utility functions here as needed
 * to support the system calls.  For example, given a file descriptor
 * you will want some sort of lookup function that will check if the fd is 
 * valid and return the associated vnode (and possibly other information like
 * the current file position) associated with that open file.
 */
struct filetable* filetable_create(){
	int result;
    struct filetable* ft;
    ft = (struct filetable*)kmalloc(sizeof(struct filetable));
    if(ft == NULL){
            return NULL;
    }
    result = filetable_init(ft);

    if(result != 0){
    	kfree(ft);
    	return NULL;
    }

    return ft;

}

struct filetable* filetable_copy(struct filetable* orig){
	struct filetable* copy;

	copy = filetable_create();

	if(copy == NULL){
		return NULL;
	}

	for(int i = 3; i < __OPEN_MAX; i++){
		if(orig->ftable[i] != NULL){
			copy->ftable[i] = orig->ftable[i];
		}
	}
	return copy;
}


/* END A3 SETUP */
