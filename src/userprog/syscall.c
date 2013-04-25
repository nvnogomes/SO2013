#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "lib/user/syscall.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}


void
halt (void)
{
  shutdown_power_off();
}


void
exit (int status)
{

}


pid_t
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
bool
create (const char *file, unsigned initial_size)
{
  return filesys_create(file, initial_size);
}

/**
 * Deletes the file called file. Returns true if successful, false otherwise.
 * A file may be removed regardless of whether it is open or closed, and
 * removing an open file does not close it.
 */
bool
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

void
seek (int fd, unsigned position)
{

}

unsigned
tell (int fd)
{

}

void
close (int fd)
{

}
