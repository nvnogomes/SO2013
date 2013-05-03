#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"

// process_start(...)
#include "process.h"

// input_getc
#include "devices/input.h"

#define STDIN_FD  0   // standard input
#define STDOUT_FD 1   // standard output

#define SUCCESS 0
#define ERROR   1


static void syscall_handler (struct intr_frame *);
static int sysc_exec(const char*);
static int sysc_exit(int);
static int sysc_create(const char *, unsigned);
static int sysc_open(const char*);
static int sysc_halt(void);
static int sysc_close(int);
static int sysc_filesize(int);
static int sysc_write(int,const void *, unsigned);
static int sysc_seek(int, unsigned);
static int sysc_remove(const char *);
static int sysc_tell(int);
static int sysc_wait(int);
static int sysc_read(int, void*,unsigned);

static int get_next_fd(void);
static struct fdelem* get_tf_fd (int);


/**
 * add something to wait list
 */
static struct list pid_wait;


/**
 * add something file descriptors
 * starting in 2
 */
static int next_fd;

/*
 * structure to associate file to the process/thread
 * and to keep track of all open files
 */
struct fdelem {
  int fd;
  struct file *file;
  struct list_elem thread_elem;
  struct list_elem of_elem;
};


/**
 * @brief syscall_init
 */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  next_fd = 2;
  list_init(&pid_wait);
}

static void
syscall_handler (struct intr_frame *f)
{
  if( is_user_vaddr(f) == false  )
    {
      sysc_exit(1);
    }

  int ret = 1;
  int *p = f->esp;

  switch(*p)
    {
    case SYS_HALT:                   /* Halt the operating system. */
      ret = sysc_halt();
      break;
    case SYS_EXIT:                   /* Terminate this process. */
      ret = sysc_exit((int)*(p+1));
      break;
    case SYS_EXEC:                   /* Start another process. */
      ret = sysc_exec((const char*)*(p+1));
      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
      ret = sysc_wait((int)*(p+1));
      break;
    case SYS_CREATE:                 /* Create a file. */
      ret = sysc_create((const char*)*(p+1),(unsigned)*(p+2));
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      ret = sysc_remove((const char*)*(p+1));
      break;
    case SYS_OPEN:                   /* Open a file. */
      ret = sysc_open((const char*)*(p+1));
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      ret = sysc_filesize((int)*(p+1));
      break;
    case SYS_READ:                   /* Read from a file. */
      ret = sysc_read((int)*(p+1),(void*)*(p+2),(unsigned)*(p+3));
      break;
    case SYS_WRITE:                  /* Write to a file. */
      ret = sysc_write((int)*(p+1),(void*)*(p+2),(unsigned)*(p+3));
      break;
    case SYS_SEEK:                   /* Change position in a file. */
      ret = sysc_seek((int)*(p+1),(unsigned)*(p+2));
      break;
    case SYS_TELL:                   /* Report current position in a file. */
      ret = sysc_tell((int)*(p+1));
      break;
    case SYS_CLOSE:                  /* Close a file. */
      ret = sysc_close((int)*(p+1));
      break;
    }

  f->eax = ret;

}


static int
sysc_halt (void)
{
  shutdown_power_off();

  return SUCCESS;
}


static int
sysc_exit (int status)
{
  struct thread *t;
  struct list_elem *e;

  t = thread_current();
  // close files of the thread
  for (e = list_begin (&t->files); e != list_end (&t->files);
       e = list_next (e))
    {
      struct fdelem *fde = list_entry (e, struct fdelem, thread_elem);
      sysc_close(fde->fd);
    }


  t->status = status;
  thread_exit();

  return SUCCESS;
}


static int
sysc_exec(const char *file)
{

  if( is_user_vaddr(file)  )
    {
      return process_execute(file);
    }

  return ERROR;
}

static int
sysc_wait (int pid)
{
  return process_wait(pid);
}

/**
 * Creates a new file called file initially initial_size bytes in size.
 * Returns true if successful, false otherwise. Creating a new file does not
 * open it: opening the new file is a separate operation which would require
 * a open system call.
 */
static int
sysc_create (const char *file, unsigned initial_size)
{

  if( is_user_vaddr(file) )
    {
      return filesys_create(file, initial_size) ? 0 : 1;
    }

  return ERROR;
}

/**
 * Deletes the file called file. Returns true if successful, false otherwise.
 * A file may be removed regardless of whether it is open or closed, and
 * removing an open file does not close it.
 */
static int
sysc_remove (const char *file)
{
  if( is_user_vaddr(file) )
    {
      struct file *fd = filesys_open(file);
      struct inode *fi = file_get_inode(fd);

      // file will be deleted after last reader closes the file
      inode_remove(fi);
      return filesys_remove(file);

    }

  return ERROR;
}

/**
 * Opens the file called file. Returns a nonnegative integer handle
 * called a "file descriptor" (fd), or -1 if the file could not be opened.
 */
static int
sysc_open (const char *file)
{
  int ret = -ERROR;
  if( is_user_vaddr(file) )
    {
      struct file *fd = filesys_open(file);
      if( fd != NULL )
        {
          struct fdelem *fde;
          fde = (struct fdelem *)malloc (sizeof (struct fdelem));
          fde->fd = get_next_fd();
          fde->file = fd;
          struct thread *t = thread_current();

          // add current file to open files and thread files
          list_push_back(&t->files, &fde->thread_elem);
          ret = (int)&fde->fd;
        }
    }

  return ret;
}

/**
 * @brief filesize
 * @param fd
 * @return the size, in bytes, of the file open as fd.
 */
static int
sysc_filesize (int fd)
{
  struct fdelem *fde = get_tf_fd(fd);

  if( fde != NULL )
    {
      return file_length(fde->file);
    }

  return ERROR;
}

/**
 * @brief read
 * the number of bytes actually read (0 at end of file),
 * or -1 if the file could not be read (due to a condition other than end
 * of file). Fd 0 reads from the keyboard using input_getc().
 * @param fd
 * @param buffer
 * @param length
 * @return
 */
static int
sysc_read (int fd, void *buffer, unsigned length)
{

  if (fd == STDIN_FD)
      {
      unsigned int i;
      for (i = 0; i != length; ++i)
        {
           *(char*)(buffer + i) = input_getc ();
        }
        return length;
      }
  else
    {
      struct fdelem *fde = get_tf_fd(fd);
      if (fde != NULL && fd == STDOUT_FD)
        {
          return file_read(fde->file,buffer,length);
        }
    }

  return ERROR;
}

/**
 * @brief sysc_write
 * @param fd
 * @param buffer
 * @param length
 * @return
 */
static int
sysc_write (int fd, const void *buffer, unsigned length)
{

  if( is_user_vaddr (buffer) && is_user_vaddr (buffer + length) )
    {
      struct fdelem *fde = get_tf_fd(fd);

      if( fde != NULL )
        {
          return file_write(fde->file, buffer, length);
        }
    }

  return ERROR;
}

/**
 * @brief seek These semantics are implemented in the file system and do not
 * require any special effort in system call implementation.
 * @param fd
 * @param position
 * @return
 */
static int
sysc_seek (int fd, unsigned position)
{
   struct fdelem *fde = get_tf_fd(fd);

   if( fde != NULL )
     {
       file_seek(fde->file, position);
       return SUCCESS;
     }

   return ERROR;
}

/**
 * @brief tell
 * @param fd
 * @return the position of the next byte to be read or written in open file fd,
 * expressed in bytes from the beginning of the file.
 */
static int
sysc_tell (int fd)
{
  struct fdelem *fde = get_tf_fd(fd);

  if( fde != NULL )
    {
      return file_tell(fde->file);
    }

  return ERROR;
}

/**
 * @brief close
 * @param fd
 * @return
 */
static int
sysc_close (int fd)
{
  struct fdelem *fde = get_tf_fd(fd);
  if( fde != NULL)
    {
      //  list_remove (fde->of_elem);
      list_remove (&fde->thread_elem);

      return SUCCESS;
    }

  return ERROR;
}


/**
 * @brief get_next_fd
 * @return next available file descriptor
 */
static int
get_next_fd(void)
{
  return next_fd++;
}



/**
 * @brief get_tf_fd
 * @param fd
 * @return struct fdelem
 */
static struct fdelem*
get_tf_fd (int fd)
{
  struct list_elem *e;
  struct thread *t = thread_current();

  for (e = list_begin (&t->files); e != list_end (&t->files);
       e = list_next (e))
    {
      struct fdelem *fde = list_entry (e, struct fdelem, thread_elem);
      if( fde->fd == fd )
        {
          return fde;
        }
    }

  return NULL;
}
