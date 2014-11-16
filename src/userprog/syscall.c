#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "lib/kernel/console.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/page.h"

/* Andrea drove here */

/* Struct that holds the file descriptor */
struct filing {
    int fd;
    struct list_elem elms;
};

/* An open file. */
struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

/* Anja drove here */

int global_status;              /* Global status for exit function */

static struct lock Lock;

static void syscall_handler (struct intr_frame *);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int* getArgs (int* myEsp, int count);
pid_t exec (const char *cmd_line);
int checkPointer (const void * buffer);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&Lock);
  
}

/* Dara drove here */

int* getArgs(int * myEsp, int count) {
  int* args[count];
  int i;
  for (i = 0; i < count; i++){
    args[i] = *(myEsp + i + 1);
  }
  return args;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* Andrea and Anja drove here */
  // printf("IN SYSCALL \n\n");
  int * myEsp = f->esp;
  struct thread *cur = thread_current ();

  cur->myEsp = f->esp;

  /* Checks if the pointer is valid */
  if(checkPointer (f->esp) == -1)
    exit (-1);

  uint32_t num = *myEsp;

  /* Checks if the arguments are valid pointers */
  if (!(is_user_vaddr (myEsp + 1) && is_user_vaddr (myEsp + 2) && is_user_vaddr (myEsp + 3)))
    exit (-1); 

  /* Make sure the syscall number is valid */
  if (num > SYS_CLOSE)
    exit (-1);

  int* args = (int *)malloc(sizeof(int));
  
  /* Dara and Anja drove here */

  /* Indicates the number for the first FD */
  if(list_empty(&cur->open_fd)) {
    struct filing *fil = (struct filing *)malloc(sizeof(struct filing));

    /* Sets the first FD to 2 because it can't be 0 or 1 */
    fil->fd = 2;
    cur->position = 2;

    /* Creates a node with the first file descriptor open */  
    list_push_back (&cur->open_fd, &fil->elms);
  }

  switch (num) {
    case SYS_HALT:
      // HALT
      halt ();
      break;
    case SYS_EXIT:
      // EXIT
      args = getArgs (myEsp, 1);
      exit (args[0]);
      f->eax = global_status;
      break;
    case SYS_EXEC:
      // EXEC
      args = getArgs (myEsp, 1);
      pid_t execs = exec (args[0]);
      f->eax = execs;
      break;
    case SYS_WAIT:
      // WAIT
      args = getArgs (myEsp, 1);
      f->eax = wait (args[0]);
      break;
    case SYS_CREATE:
      // CREATE
      args = getArgs (myEsp, 2);
      f->eax = create (args[0], args[1]);
      break;
    case SYS_REMOVE:
      // REMOVE
      args = getArgs (myEsp, 1);
      f->eax = remove (args[0]);
      break;
    case SYS_OPEN:
      // OPEN
      args = getArgs (myEsp, 1);
      f->eax = open (args[0]);
      break;
    case SYS_FILESIZE:
      // FILESIZE
      args = getArgs (myEsp, 1);
      f->eax = filesize (args[0]);
      break;
    case SYS_READ:
      // READ
      args = getArgs (myEsp, 3);

      f->eax = read (args[0], args[1], args[2]);
      //printf("back from read\n");
      break;
    case SYS_WRITE:
      // WRITE
      args = getArgs (myEsp, 3);
      f->eax = write (args[0], args[1], args[2]);
      break;
    case SYS_SEEK:
      // SEEK
      args = getArgs (myEsp, 2);
      seek (args[0], args[1]);
      break;
    case SYS_TELL:
      // TELL
      args = getArgs (myEsp, 1);
      f->eax = tell (args[0]);
      break;
    case SYS_CLOSE:
      // CLOSE
      args = getArgs (myEsp, 1);
      close (args[0]);
      break;
  } 
}

/* Dara drove here */
void exit (int status) {
  
  struct thread *cur = thread_current ();

  global_status = status;

  /* Gives the parent the child's exit status */
  cur->child_exit = status;

  char *token, *saveptr1;
  token = strtok_r (cur->name, " ", &saveptr1);
  

  printf ("%s: exit(%d)\n", token, cur->child_exit);
  
  /* Exits the thread */
  thread_exit ();
}

/* Anja drove here */
void halt (void)
{
  /*Call wait on initial process*/
  shutdown_power_off ();
}


int write (int fd, const void *buffer, unsigned size) {
  
  /* Checks buffer for a bad pointer */
  if(checkPointer (buffer) == -1)
    exit (-1);

  int charWritten = 0;
  
  /* Anja drove here */
  struct page *p = page_lookup(buffer, false, thread_current());
  struct frame_entry *f = frame_getEntry(p->kpage);
  f->notevictable = 1;

  /* Check if we need to write to a file */
  if (fd > 1){
    struct thread *cur = thread_current ();

    /* Checks if fd is within bounds of array */
    if (fd < 2 || fd > 127)
      return -1;

    /* Gets the file from the files array */
    struct file * fil = cur->files[fd];

    /* Checks if the file at FD was valid */
    if (fil == NULL)
      return -1;

    
    /* Writes to the file and puts number of written characters */
    charWritten = file_write (fil, (char *) buffer, size);
    
  }

  /* Andrea drove here */

  /* Checks if we need to write to console */
  else if (fd == 1){
    /* Checks if buffer is valid */
    if (buffer == NULL)
      exit (-1);

    if (size < 300) {
      putbuf ((char *) buffer + charWritten, size);
      charWritten += size;
    }
    else {
      /* Breaks up buffer */
      while (size > 300) {
      putbuf ((char *)buffer + charWritten, 300);
      charWritten += 300;
      size -= 300;
      }

      putbuf ((char *)buffer + charWritten, size);
      charWritten += size;
    }
  }

  /* Number of chars written */
  f->notevictable = 0;
  return charWritten;
}

/* Dara drove here */

bool create (const char *file, unsigned initial_size) 
{
  /* Lock for mutual exclusion */
  lock_acquire(&Lock);

  /* Checks if file is valid */
  if(checkPointer(file) == -1)
      exit(-1);
  
  int flag = filesys_create (file, initial_size);
  lock_release(&Lock);
  return flag;
}

int open (const char *file)  {
  /* Dara and Andrea drove here */
  struct thread *cur = thread_current();

  if(checkPointer (file) == -1)
  {
    exit(-1);
  }

  lock_acquire (&Lock);
  struct page *p = page_lookup(file, false, thread_current());
  struct frame_entry *f = frame_getEntry(p->kpage);
  f->notevictable = 1;
  
  /* Opens the file */
  struct file *openFile = filesys_open (file);
  
  lock_release (&Lock);

  /* Gets an open fd */
  struct filing *fil = list_entry (list_pop_front (&cur->open_fd), struct filing, elms);
  
  /* Allocate space for the index */
  cur->files[fil->fd] = (struct file *)malloc(sizeof(struct file));

  /* Checks if the file system was able to open the file 
    and if the number of files has exceeded 128 */
  if (openFile == NULL || ((fil->fd > 127 || cur->position > 127) && list_empty(&cur->open_fd)) || fil->fd < 2 || cur->position < 2)
  {
    free(cur->files[fil->fd]);
    return -1;
  }

  /* Puts the file into the array */
  cur->files[fil->fd] = openFile;

  /* Sets the file descriptor to the open fd */
  int fd = fil->fd;

  /* If extending past the current bound, increase bound */
  if (fd == cur->position) {
    cur->position += 1;
    fil->fd = cur->position;  
  }

  /* Adds the next index to the list of open fds for the thread */
  list_push_back (&cur->open_fd, &fil->elms);

  f->notevictable = 0;
  return fd;
}

/* Anja drove here */

int filesize (int fd) {
  struct thread *cur = thread_current ();

  if (cur->files[fd] == NULL)
    exit(-1);

  /* Returns the length of the file */
  int len = file_length (cur->files[fd]);
  
  return len;
}

/* Anja and Dara drove here */

unsigned tell (int fd) {
  struct thread *cur = thread_current ();

  /* Returns the current position in the file */
  int pos = file_tell (cur->files[fd]);

  return pos;
}

/* Andrea drove here */

void seek (int fd, unsigned position) {
  struct thread *cur = thread_current ();
  
  /* Checks if the file is valid */
  if (cur->files[fd] == NULL)
    exit(-1);

  /* Checks if the position to change to is within the file */
  if(position > (unsigned) file_length (cur->files[fd]))
    exit(-1);

  /* Sets the file position to position */
  file_seek (cur->files[fd], position);
}

/* Anja drove here */
bool remove (const char *file) {
  lock_acquire(&Lock);
  struct page *p = page_lookup(file, false, thread_current());
  struct frame_entry *f = frame_getEntry(p->kpage);
  f->notevictable = 1;

  /* Returns true if able to remove the file.
    Only prevents file from being opened again */
  int flag = filesys_remove (file);
  
  f->notevictable = 0;
  lock_release (&Lock);
  return flag;
}

/* Andrea and Anja drove here */

void close (int fd) {
  struct thread *cur = thread_current ();
  
  /* Checks if fd is within the array */
  if (fd < 2 || fd > 127)
      exit(-1);

  /* Checks if the file is valid */
  if (cur->files[fd] == NULL)
    exit (-1);
  
  /* Closes the file */
  file_close (cur->files[fd]);

  /* Opens spot in array */
  cur->files[fd] = NULL;
  free (cur->files[fd]);

  struct filing *fil = (struct filing*)malloc(sizeof(struct filing));
  fil->fd = fd;

  /* Adds freed fd to list of open fds */
  list_push_back (&cur->open_fd, &fil->elms);

  free (fil);
}

int read (int fd, void *buffer, unsigned size) 
{

  /* Andrea drove here */

  int charsRead = 0;

  /* Checks buffer for a bad pointer */
  int check = checkPointer (buffer);

  struct page *p = page_lookup(buffer, false, thread_current());
  struct frame_entry *f = frame_getEntry(p->kpage);
  f->notevictable = 1;
  /* Checks if getting input */
  if (fd == 0) {
    
    charsRead = input_getc ();
  }

  /* Dara drove here */
  else
  {
    struct thread *cur = thread_current ();
    
    /* Checks if fd is within bounds of array */
    if(fd < 2 || fd > 127) {
      return -1;
    }

    /* Gets the file from the files array */
    struct file * fil = cur->files[fd];

    /* Checks if the file at fd was valid */
    if(fil == NULL){
      exit(-1);
    }

    /* Writes to the file and puts number of written characters */
    charsRead = file_read (fil, (char *) buffer, size); 
  }

  /* Return number of chars read */
  f->notevictable = 0;
  return charsRead;
}

pid_t exec (const char *cmd_line) 
{
  /* Dara and Andrea drove here */

  /* Checks if cmd_line is valid */
  if(checkPointer (cmd_line) == -1)
    return -1;

  struct page *p = page_lookup(cmd_line, false, thread_current());
  struct frame_entry *f = frame_getEntry(p->kpage);
  f->notevictable = 1;
  
  struct thread *cur = thread_current ();
  pid_t result;
  
  lock_acquire (&Lock);
  /* Creates thread to run */
  result = process_execute ((char *) cmd_line);

  /* If process execute didn't create a thread */
  if (result == TID_ERROR) {
    return -1;
  }
  f->notevictable = 0;
  /* Parent waits until the child indicates if it has loaded */
  sema_down (&cur->complete);
  lock_release (&Lock);

  /* Anja drove here */

  struct list_elem *e;

  /* Go through children until finds correct child */
  for (e = list_begin (&cur->children); e != list_end (&cur->children); e = list_next (e))
  {
    struct thread *j = list_entry (e, struct thread, child);
    if (j->tid == result) {
      /* Check if the child loaded */
      if (j->load_flag == 1) {
        //printf("result %d\n", result);
        return result;
      }
      else {
        //printf("hey\n");
          return -1;
      }
    }
  }
  return -1;
}

/* Dara drove here */

int wait (pid_t pid) {
  struct thread *cur = thread_current();

  /* Wait for the child process */
  int result = process_wait (pid);

  /* Gives the parent the exit status of the child */
  //cur->parent->child_exit = result;

  return result;
}

/* Anja and Dara drove here */

int checkPointer (const void * buffer)
{

  uint32_t *activepd = active_pd ();

  /* Checks if buffer doesn't have anything in it */
  if (buffer == NULL)
    exit(-1);

  /* Checks if pointer is in user space */
  if (!is_kernel_vaddr (buffer)) {
    /* Checks if the pointer is mapped */
    if(lookup_page (activepd, buffer, 0) == NULL) {
      exit (-1);
    }
  }

  /* Checks if the pointer is in kernel address space */
  else if (is_kernel_vaddr (buffer)){
    exit (-1);
  }

  /* Return successful otherwise */
  return 0;
}

