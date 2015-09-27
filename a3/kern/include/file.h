/* BEGIN A3 SETUP */
/*
 * Declarations for file handle and file table management.
 * New for A3.
 */

#ifndef _FILE_H_
#define _FILE_H_

#include <kern/limits.h>
#include <array.h>

struct vnode;

/*
 * filetable struct
 * just an array, nice and simple.  
 * It is up to you to design what goes into the array.  The current
 * array of ints is just intended to make the compiler happy.
 */
struct filetable {
	struct filedesc* ftable[__OPEN_MAX]; /* dummy type */
};

struct filedesc{
	struct vnode* vn;
	off_t offset;
	int flags;
	int dup_cnt;
};

/* these all have an implicit arg of the curthread's filetable */
int filetable_init(struct filetable* table);
void filetable_destroy(struct filetable *ft);

/* opens a file (must be kernel pointers in the args) */
int file_open(char *filename, int flags, int mode, int *retfd);

/* closes a file */
int file_close(int fd);

/* A3: You should add additional functions that operate on
 * the filetable to help implement some of the filetable-related
 * system calls.
 */
struct filetable* filetable_create(void);
int filetable_add(struct filetable* table, struct filedesc* fd);
int filetable_remove(struct filetable* table, int index);
struct filetable* filetable_copy(struct filetable* orig);


#endif /* _FILE_H_ */

/* END A3 SETUP */
