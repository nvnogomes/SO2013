#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"

static void syscall_handler (struct intr_frame *);

/**
 * add something to wait list
 */

/**
 * add something file descriptors
 */



/**
 * @brief syscall_init
 */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int ret;
  int *op = f->esp;

  switch(op)
    {
    case SYS_HALT:                   /* Halt the operating system. */
      ret = halt();
      break;
    case SYS_EXIT:                   /* Terminate this process. */
      ret = exit(*p+1);
      break;
    case SYS_EXEC:                   /* Start another process. */
      ret = exec(*p+1);
      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
      ret = wait(*p+1);
      break;
    case SYS_CREATE:                 /* Create a file. */
      ret = create(*p+1,*p+2);
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      ret = remove(*p+1);
      break;
    case SYS_OPEN:                   /* Open a file. */
      ret = open(*p+1);
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      ret = filesize(*p+1);
      break;
    case SYS_READ:                   /* Read from a file. */
      ret = read(*p+1,*p+2,*p+3);
      break;
    case SYS_WRITE:                  /* Write to a file. */
      ret = write(*p+1,*p+2,*p+3);
      break;
    case SYS_SEEK:                   /* Change position in a file. */
      ret = seek(*p+1,*p+2);
      break;
    case SYS_TELL:                   /* Report current position in a file. */
      ret = tell(*p+1);
      break;
    case SYS_CLOSE:                  /* Close a file. */
      ret = close(*p+1);
      break;
    }

  f->eax = ret;

}

int
halt (void)
{
  shutdown_power_off();
}


int
exit (int status)
{

}


int
exec(const char *file)
{

}

int
wait (pid_t pid)
{

}

/**
 * Creates a new file called file initially initial_size bytes in size.
 * Returns true if successful, false otherwise. Creating a new file does not
 * open it: opening the new file is a separate operation which would require
 * a open system call.
 */
int
create (const char *file, unsigned initial_size)
{
  return filesys_create(file, initial_size);
}

/**
 * Deletes the file called file. Returns true if successful, false otherwise.
 * A file may be removed regardless of whether it is open or closed, and
 * removing an open file does not close it.
 */
int
remove (const char *file)
{
  struct file *fd = filesys_open(file);
  struct inode *fi = file_get_inode(fd);
  inode_remove(fi);
  return filesys_remove(file);
}

/**
 * Opens the file called file. Returns a nonnegative integer handle
 * called a "file descriptor" (fd), or -1 if the file could not be opened.
 */
int
open (const char *file)
{
  struct file *fd = filesys_open(file);
  if( fd == NULL )
    {
      return -1;
    }
  // FD to return ???
}

int
filesize (int fd)
{

}

int
read (int fd, void *buffer, unsigned length)
{

}

int
write (int fd, const void *buffer, unsigned length)
{

}

int
seek (int fd, unsigned position)
{

}

int
tell (int fd)
{

}

int
close (int fd)
{

}
