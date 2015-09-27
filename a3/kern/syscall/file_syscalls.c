/* BEGIN A3 SETUP */
/* This file existed for A1 and A2, but has been completely replaced for A3.
 * We have kept the dumb versions of sys_read and sys_write to support early
 * testing, but they should be replaced with proper implementations that 
 * use your open file table to find the correct vnode given a file descriptor
 * number.  All the "dumb console I/O" code should be deleted.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <copyinout.h>
#include <synch.h>
#include <file.h>
#include <kern/seek.h>

/* This special-case for the console vnode should go away with a proper
 * open file table implementation.
 */

struct vnode *cons_vnode=NULL;

void dumb_consoleIO_bootstrap()
{
  int result;
  char path[5];

  /* The path passed to vfs_open must be mutable.
   * vfs_open may modify it.
   */

  strcpy(path, "con:");
  result = vfs_open(path, O_RDWR, 0, &cons_vnode);

  if (result) {
    /* Tough one... if there's no console, there's not
     * much point printing a warning...
     * but maybe the bootstrap was just called in the wrong place
     */
    kprintf("Warning: could not initialize console vnode\n");
    kprintf("User programs will not be able to read/write\n");
    cons_vnode = NULL;
  }
}

/*
 * mk_useruio
 * sets up the uio for a USERSPACE transfer. 
 */
static
void
mk_useruio(struct iovec *iov, struct uio *u, userptr_t buf, 
	   size_t len, off_t offset, enum uio_rw rw)
{

	iov->iov_ubase = buf;
	iov->iov_len = len;
	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = offset;
	u->uio_resid = len;
	u->uio_segflg = UIO_USERSPACE;
	u->uio_rw = rw;
	u->uio_space = curthread->t_addrspace;
}

/*
 * sys_open
 * just copies in the filename, then passes work to file_open.
 * You have to write file_open.
 * 
 */
int
sys_open(userptr_t filename, int flags, int mode, int *retval)
{
	char fname[__PATH_MAX];
	int result;

	result = copyinstr(filename, fname, sizeof(fname), NULL);
	if (result) {
		return result;
	}

	result =  file_open(fname, flags, mode, retval);
	return result;
}

/* 
 * sys_close
 * You have to write file_close.
 */
int
sys_close(int fd)
{
	if (fd < 0 || fd >__OPEN_MAX) {
	  return EBADF;
	}

	return file_close(fd);
}

/* 
 * sys_dup2
 * Duplicates an existing file descriptor. 
 * Nonzero on error.
 */
int
sys_dup2(int oldfd, int newfd, int *retval)
{
		if (oldfd < 0 || oldfd >__OPEN_MAX) {
			return EBADF;
		}
		if (newfd < 0 || newfd >__OPEN_MAX) {
					return EBADF;
		}

        struct filedesc* old;
        struct filedesc* new;

        old = curthread->t_filetable->ftable[oldfd];
        if(old == NULL){
        	return EBADF;
        }

        new = curthread->t_filetable->ftable[newfd];
        if(new != NULL){
        	sys_close(newfd);
        }

        new  = kmalloc(sizeof(struct filedesc));
        new->vn = old->vn;
        new->offset = old->offset;
        new->flags = old->flags;

        curthread->t_filetable->ftable[newfd] = new;
        (void)*retval;

        return 0;

}

/*
 * sys_read
 * calls VOP_READ.
 * 
 * A3: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
	struct uio user_uio;
	struct iovec user_iov;
	int result;
	//int offset = 0;

	/* better be a valid file descriptor */
	/* Right now, only stdin (0), stdout (1) and stderr (2)
	 * are supported, and they can't be redirected to a file
	 */
	if (fd < 0 || fd >__OPEN_MAX) {
	  return EBADF;
	}

	struct filedesc* file_desc;

	file_desc = curthread->t_filetable->ftable[fd];

	if(file_desc == NULL || file_desc->vn->vn_ops == NULL){
		return EBADF;
	}

	if((file_desc->flags) != O_RDONLY && (file_desc->flags) != O_RDWR){
		return EBADF;
	}


	/* set up a uio with the buffer, its size, and the current offset */
	mk_useruio(&user_iov, &user_uio, buf, size, file_desc->offset, UIO_READ);

	/* does the read */
	result = VOP_READ(file_desc->vn, &user_uio);
	if (result) {
		return result;
	}
	file_desc->offset = user_uio.uio_offset;

	/*
	 * The amount read is the size of the buffer originally, minus
	 * how much is left in it.
	 */
	*retval = size - user_uio.uio_resid;

	return 0;
}

/*
 * sys_write
 * calls VOP_WRITE.
 *
 * A3: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */

int
sys_write(int fd, userptr_t buf, size_t len, int *retval) 
{
        struct uio user_uio;
        struct iovec user_iov;
        int result;

        /* Right now, only stdin (0), stdout (1) and stderr (2)
         * are supported, and they can't be redirected to a file
         */
        if (fd < 0 || fd > __OPEN_MAX) {
          return EBADF;
        }


    	struct filedesc* file_desc;

    	file_desc = curthread->t_filetable->ftable[fd];

    	if(file_desc == NULL){
    		return EBADF;
    	}

    	if((file_desc->flags) != O_WRONLY && (file_desc->flags) != O_RDWR){
    		return EBADF;
    	}


        /* set up a uio with the buffer, its size, and the current offset */
        mk_useruio(&user_iov, &user_uio, buf, len, file_desc->offset, UIO_WRITE);

        /* does the write */
        result = VOP_WRITE(file_desc->vn, &user_uio);
        if (result) {
                return result;
        }

        file_desc->offset = user_uio.uio_offset;
        /*
         * the amount written is the size of the buffer originally,
         * minus how much is left in it.
         */
        *retval = len - user_uio.uio_resid;

        return 0;
}

/*
 * sys_lseek
 * Repositions offset of the open file assosciated with
 * fd to offset based on whence.
 * SEEK_SET: the offset is set to offset bytes.
 * SEEK_CUR: the offset is set to its current location plus offset bytes.
 * SEEK_END: the offset is set to the size of the file plus offset.
 */
int
sys_lseek(int fd, off_t offset, int whence, off_t *retval)
{

    	off_t ofs;
    	struct stat tmp;
    	int result;

    	if(fd < 0 || fd > __OPEN_MAX){
    		*retval = -1;
    		return EBADF;
    	}
    	if(curthread->t_filetable == NULL){
    		*retval = -1;
    		return EBADF;
    	}
    	struct filedesc* f_desc = curthread->t_filetable->ftable[fd];

    	if(f_desc == NULL || f_desc->vn->vn_ops == NULL){
    		return EBADF;
    	}

    	//actual seek occurs
    	switch(whence) {
    	case SEEK_SET:
    		ofs = offset;
    		break;

    	case SEEK_CUR:
    		ofs = f_desc->offset + offset;
    		break;

    	case SEEK_END:
           result = VOP_STAT(f_desc->vn, &tmp);
           if(result){
        	   return result;
           }
           ofs = tmp.st_size + offset;
           break;

    	default:
    		return EINVAL;
    	}

    	if(offset < 0) {
    		return EINVAL;
    	}

    	result = VOP_TRYSEEK(f_desc->vn, ofs);
    	if(result){
    		return result;
    	}
    	// All done, update offset
    	*retval = f_desc->offset = ofs;
    	return 0;
}


/* really not "file" calls, per se, but might as well put it here */

/*
 * sys_chdir
 * Changes working to directory to specified path.
 */
int
sys_chdir(userptr_t path)
{
		char path_buffer[__NAME_MAX];
		int result;

		result = copyinstr(path, path_buffer, __NAME_MAX, NULL);
		if(result){
			return result;
		}
		return vfs_chdir(path_buffer);
}

/*
 * sys___getcwd
 * 
 */
int
sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{

		if(buf == NULL){
			return EFAULT;
		}

        struct iovec iov;
        struct uio readuio;
        char *name = (char*)kmalloc(buflen);
        uio_kinit(&iov, &readuio, name, buflen-1, 0, UIO_READ);

        int result = vfs_getcwd(&readuio);
        if(result)
        {
        *retval = -1;
        return result;
        }
        //null terminate
        name[buflen-1-readuio.uio_resid] = 0;
        size_t size;
        copyoutstr((const void *)name, buf, buflen, &size);
        *retval = buflen-readuio.uio_resid;
        kfree(name);
        return 0;
}

/*
 * sys_fstat
 */
int
sys_fstat(int fd, userptr_t statptr)
{
		if(fd < 3 || fd > __OPEN_MAX){
			return EBADF;
		}
		int result;
		struct stat *s_buffer = (struct stat *) statptr;
		struct filedesc* f_desc = curthread->t_filetable->ftable[fd];
		if(f_desc == NULL || f_desc->vn->vn_ops == NULL){
			return EBADF;
		}
		result = VOP_STAT(f_desc->vn, s_buffer);
		if(result){
			return ENOENT;
		}

	return 0;
}

/*
 * sys_getdirentry
 */
int
sys_getdirentry(int fd, userptr_t buf, size_t buflen, int *retval)
{

		if(fd < 3 || fd > __OPEN_MAX){
			*retval = -1;
			return EBADF;
		}

		struct uio *u;
		u = (struct uio *)kmalloc(sizeof(struct uio));
		if(u == NULL){
			return -1;
		}
		struct iovec *iov;
		iov = (struct iovec *)kmalloc(sizeof(struct iovec));
		if(iov == NULL){
			return -1;
		}

        uio_kinit(iov, u, buf, buflen, 0, UIO_READ);



		struct filedesc* f_desc = curthread->t_filetable->ftable[fd];
		if(f_desc == NULL || f_desc->vn->vn_ops == NULL) {
			return EBADF;
		}
        *retval = VOP_GETDIRENTRY(f_desc->vn, u);

        if(*retval){
        	*retval = -1;
        	return ENOENT;
        }
        f_desc->offset += u->uio_offset;

        return 0;
}

/* END A3 SETUP */




