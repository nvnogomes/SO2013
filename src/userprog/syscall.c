#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"

static void syscall_handler (struct intr_frame *);
static int exec(const char*);
static int exit(int);
static int create(const char *, unsigned);
static int open(const char*);
static int halt();
static int close(int);
static int filesize(int);
static int write(int,const void *, unsigned);
static int seek(int, unsigned);
static int remove(const char *);
static int tell(int);
static int wait(int);
static int read(int, void*,unsigned);

static int get_next_fd();
static struct fdelem* get_of_fd(int);
static struct fdelem* get_tf_fd (int);


/**
 * add something to wait list
 */
static struct list pid_wait;


/**
 * add something file descriptors
 * fd 0 (STDIN_FILENO) is standard input,
 * fd 1 (STDOUT_FILENO) is standard output.
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

static struct list open_files;


/**
 * @brief syscall_init
 */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  next_fd = 2;
  list_init(&open_files);
  list_init(&pid_wait);
}

static void
syscall_handler (struct intr_frame *f)
{
  int ret;
  int *p = f->esp;

  if( !(is_user_vaddr(*(p+1)) && is_user_vaddr(*(p+2)) && is_user_vaddr(*(p+3))) )
    {
      exit(-1);
    }



  switch(*p)
    {
    case SYS_HALT:                   /* Halt the operating system. */
      ret = halt();
      break;
    case SYS_EXIT:                   /* Terminate this process. */
      ret = exit(*(p+1));
      break;
    case SYS_EXEC:                   /* Start another process. */
      ret = exec(*(p+1));
      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
      ret = wait(*(p+1));
      break;
    case SYS_CREATE:                 /* Create a file. */
      ret = create(*(p+1),*(p+2));
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      ret = remove(*(p+1));
      break;
    case SYS_OPEN:                   /* Open a file. */
      ret = open(*(p+1));
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      ret = filesize(*(p+1));
      break;
    case SYS_READ:                   /* Read from a file. */
      ret = read(*(p+1),*(p+2),*(p+3));
      break;
    case SYS_WRITE:                  /* Write to a file. */
      ret = write(*(p+1),*(p+2),*(p+3));
      break;
    case SYS_SEEK:                   /* Change position in a file. */
      ret = seek(*(p+1),*(p+2));
      break;
    case SYS_TELL:                   /* Report current position in a file. */
      ret = tell(*(p+1));
      break;
    case SYS_CLOSE:                  /* Close a file. */
      ret = close(*(p+1));
      break;
    }

  f->eax = ret;

}


static int
halt (void)
{
  shutdown_power_off();

  return 0;
}


static int
exit (int status)
{
  int ret = -1;
  struct thread *t;
  struct fdelem *fd;
  struct list_elem *l;

  t = thread_current ();
  // close files of the thread
  while (!list_empty (&t->files))
      {
        l = list_begin (&t->files);
      }

  t->status = status;
  thread_exit();

  return ret;
}


static int
exec(const char *file)
{
  int ret = -1;
  ret = process_execute(file);
  return ret;
}

static int
wait (int pid)
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
create (const char *file, unsigned initial_size)
{
  return filesys_create(file, initial_size) ? 0 : -1;
}

/**
 * Deletes the file called file. Returns true if successful, false otherwise.
 * A file may be removed regardless of whether it is open or closed, and
 * removing an open file does not close it.
 */
static int
remove (const char *file)
{
  struct file *fd = filesys_open(file);
  struct inode *fi = file_get_inode(fd);

  // file will be deleted after last reader closes the file
  inode_remove(fi);
  return filesys_remove(file);
}

/**
 * Opens the file called file. Returns a nonnegative integer handle
 * called a "file descriptor" (fd), or -1 if the file could not be opened.
 */
static int
open (const char *file)
{
  struct file *fd = filesys_open(file);
  if( fd == NULL )
    {
      return -1;
    }
  else
    {
      struct fdelem *fde;
      fde->fd = get_next_fd();
      fde->file = file;
      struct thread *t = thread_current();

      // add current file to open files and thread files
      list_push_back(&t->files, &fde->thread_elem);
//      list_push_back(&open_files, fde.of_elem);
      return &fde->fd;
    }
}

/**
 * @brief filesize
 * @param fd
 * @return the size, in bytes, of the file open as fd.
 */
static int
filesize (int fd)
{
  struct fdelem *fde = get_of_fd(fd);

  if( fde == NULL )
    {
      return -1;
    }
  else
    {
      return file_length(fde->file);
    }
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
read (int fd, void *buffer, unsigned length)
{

  if (fd == 0)
      {
        // TODO
      }
    else if (fd == 1)
    {
      return -1;
    }
  else
    {
      struct fdelem *fde = get_of_fd(fd);

      if( fde == NULL )
        {
          return -1;
        }
      else
        {
          return file_read(fde->file,buffer,length);
        }
    }
}

static int
write (int fd, const void *buffer, unsigned length)
{

  if( fd == 0 )
    {
      return -1;
    }
  else
    {
     struct fdelem *fde = get_of_fd(fd);

     if( fde == NULL )
       {
         return -1;
       }
     else
       {
         return file_write(fde->file, buffer, length);
       }
    }
}

/**
 * @brief seek These semantics are implemented in the file system and do not
 * require any special effort in system call implementation.
 * @param fd
 * @param position
 * @return
 */
static int
seek (int fd, unsigned position)
{
  if( fd == 0 )
    {
      return -1;
    }
  else
    {
     struct fdelem *fde = get_of_fd(fd);

     if( fde == NULL )
       {
         return -1;
       }
     else
       {
         file_seek(fde->file, position);
         return 0;
       }
    }
}

/**
 * @brief tell
 * @param fd
 * @return the position of the next byte to be read or written in open file fd,
 * expressed in bytes from the beginning of the file.
 */
static int
tell (int fd)
{
  if( fd == 0 )
    {
      return -1;
    }
  else
    {
     struct fdelem *fde = get_of_fd(fd);

     if( fde == NULL )
       {
         return -1;
       }
     else
       {
         return file_tell(fde->file);
       }
    }
}

/**
 * @brief close
 * @param fd
 * @return
 */
static int
close (int fd)
{
  int ret = -1;
  struct fdelem *fde = get_tf_fd(fd);

//  list_remove (fde->of_elem);
  list_remove (&fde->thread_elem);

  return ret;
}


/**
 * @brief get_next_fd
 * @return next available file descriptor
 */
static int
get_next_fd()
{
  return next_fd++;
}


/**
 * @brief get_of_fd
 * @param fd
 * @return struct fdelem
 */
static struct fdelem*
get_of_fd (int fd)
{
  struct list_elem *e;

  for (e = list_begin (&open_files); e != list_end (&open_files);
       e = list_next (e))
    {
      struct fdelem *fde = list_entry (e, struct fdelem, of_elem);
      if( fde->fd == fd )
        {
          return fde;
        }
    }

  return NULL;
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
