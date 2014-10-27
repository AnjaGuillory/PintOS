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



int global_status;              /* Global status for exit function */

static struct lock Lock;

static void syscall_handler (struct intr_frame *);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int* getArgs(int* myEsp, int count);
pid_t exec (const char *cmd_line);
int checkPointer(const void * buffer);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&Lock);
  
}


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

  int * myEsp = f->esp;
  struct thread *cur = thread_current ();

  if(checkPointer(f->esp) == -1)
    exit(-1);

  if ((void *) f->esp >= PHYS_BASE)
    exit(-1);

  if (!is_user_vaddr (myEsp))
    exit(-1);

  uint32_t num = *myEsp;

  if (!(is_user_vaddr (myEsp + 1) && is_user_vaddr (myEsp + 2) && is_user_vaddr (myEsp + 3)))
    exit(-1); 

  if (num < SYS_HALT || num > SYS_CLOSE)
    exit(-1);

  int* args = palloc_get_page(0);
  
  if(list_empty(&cur->open_fd)) {
    struct filing *fil = palloc_get_page (0);

    fil->fd = 2;
    cur->position = 2;
    list_push_back(&cur->open_fd, &fil->elms);
    /* Creates a node with the first file descriptor open */  
  }

  /* SWITCHHHHHH */
  switch (num) {
    case SYS_HALT:
      // HALT
      halt();
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
      // f->eax = reads;
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

void exit (int status) {
  struct thread *cur = thread_current ();

  global_status = status;

  (cur->parent)->child_exit = status;

  char *str1, *token, *saveptr1;
  str1 = palloc_get_page (0);
  strlcpy (str1, cur->name, PGSIZE);
  token = strtok_r(str1, " ", &saveptr1);
  
  printf("%s: exit(%d)\n", token, status);
  palloc_free_page(str1);

  thread_exit();
}

void halt(void)
{
  /*Call wait on initial process*/
  shutdown_power_off ();
}


int write (int fd, const void *buffer, unsigned size) {
  /* Checks buffer for a bad pointer */
  if(checkPointer(buffer) == -1)
    exit(-1);

  int charWritten = 0;
  struct thread *cur = thread_current ();
  
  if (fd > 1){

    /* Checks if fd is within bounds of array */
    if(fd < 2 || fd > 127)
      return -1;

    /* Gets the file from the files array */
    struct file * fil = cur->files[fd];

    /* Checks if the file at fd was valid */
    if(fil == NULL)
      return -1;

    lock_acquire(&Lock);
    /* Writes to the file and puts number of written characters */
    charWritten = file_write (fil, (char *) buffer, size);

    lock_release(&Lock);
  }

  else if (fd == 1){
    
    if (buffer == NULL)
      exit(-1);

    if (size < 300) {
      putbuf((char *) buffer + charWritten, size);
      charWritten += size;
    }
    else {
      while (size > 300) {
      putbuf((char *)buffer + charWritten, 300);
      charWritten += 300;
      size -= 300;
      }

      /* calls putbuf on the rest of the buffer */
      putbuf((char *)buffer + charWritten, size);
      charWritten += size;
    }
  }

  return charWritten;
}

bool create (const char *file, unsigned initial_size) 
{

  lock_acquire(&Lock);

  if(checkPointer(file) == -1)
      exit(-1);
  
  int flag = filesys_create (file, initial_size);

  lock_release(&Lock);
  return flag;
}

int open (const char *file)  {


  // Needed to check for bad pointers (not working) 
  struct thread *cur = thread_current();

  if(checkPointer(file) == -1)
    exit(-1);

  lock_acquire(&Lock);
  /* Opens the file */
  struct file *openFile = filesys_open(file);
  
  lock_release(&Lock);


  /* Gets an open fd */
  struct filing *fil = list_entry(list_pop_front(&cur->open_fd), struct filing, elms);
  
  /* Allocate space for the index */
  cur->files[fil->fd] = palloc_get_page(0);

  /* Checks if the file system was able to open the file 
    and if the number of files has exceeded 128 */
  if(openFile == NULL || ((fil->fd > 127 || cur->position > 127) && list_empty(&cur->open_fd)) || fil->fd < 2 || cur->position < 2)
  {
    palloc_free_page(cur->files[fil->fd]);
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
  list_push_back(&cur->open_fd, &fil->elms);
  return fd;
}

int filesize (int fd) {
  /* Returns the length of the file */
  struct thread *cur = thread_current ();
  if(cur->files[fd] == NULL)
    exit(-1);

  int len = file_length (cur->files[fd]);
  
  return len;
}

unsigned tell (int fd) {
  struct thread *cur = thread_current ();
  /* Returns the current position in the file */
  int pos = file_tell (cur->files[fd]);

  return pos;
}

void seek (int fd, unsigned position) {
  struct thread *cur = thread_current ();
  /* Checks if the position to change to is within the file */
  if (cur->files[fd] == NULL)
    exit(-1);
  if(position > (unsigned) file_length (cur->files[fd]))
    exit(-1);

  /* Sets the file position to position */
  file_seek (cur->files[fd], position);

}

bool remove (const char *file) {
  lock_acquire(&Lock);

  /* Returns true if able to remove the file.
    Only prevents file from being opened again */
  int flag = filesys_remove (file);
  
  lock_release(&Lock);

  return flag;
}

void close (int fd) {
  struct thread *cur = thread_current ();
  /* Checks if fd is within the array */
  if(fd < 2 || fd > 127)
      exit(-1);

  if(cur->files[fd] == NULL)
    exit(-1);
  
  /* Closes the file */
  file_close (cur->files[fd]);

  /* Opens spot in array */
  cur->files[fd] = NULL;
  palloc_free_page(cur->files[fd]);

  struct filing *fil = palloc_get_page(0);
  fil->fd = fd;

  /* Adds freed fd to list of open fds */
  list_push_back(&cur->open_fd, &fil->elms);

  palloc_free_page(fil);
}

int read (int fd, void *buffer, unsigned size) 
{
  struct thread *cur = thread_current ();

  int charsRead = 0;

  /* Checks buffer for a bad pointer */
  if(checkPointer(buffer) == -1)
    exit(-1);
  
  if(fd == 0)
    charsRead = input_getc();

  else
  {
    /* Checks if fd is within bounds of array */
    if(fd < 2 || fd > 127)
      return -1;

    /* Gets the file from the files array */
    struct file * fil = cur->files[fd];

    /* Checks if the file at fd was valid */
    if(fil == NULL){
      exit(-1);
    }

    /* Writes to the file and puts number of written characters */
    charsRead = file_read (fil, (char *) buffer, size); 
  }
  return charsRead;
}

pid_t exec (const char *cmd_line) 
{
  if(checkPointer(cmd_line) == -1)
    return -1;
  
  struct thread *cur = thread_current ();
  pid_t result;
  
  lock_acquire(&Lock);
  result = process_execute((char *) cmd_line);
  /* If process execute didn't create a thread */
  if (result == TID_ERROR) {
    return -1;
  }
  sema_down(&cur->complete);
  lock_release(&Lock);

  struct list_elem *e;

  for (e = list_begin (&cur->children); e != list_end (&cur->children); e = list_next (e))
            {
              struct thread *j = list_entry (e, struct thread, child);
              if (j->tid == result) {
                if (j->load_flag == 1) {
                  return result;
                }
                else {
                    return -1;
                }
              }
            }
  return -1;
}

int wait (pid_t pid) {
  struct thread *cur = thread_current();
  int result = process_wait(pid);
  cur->parent->child_exit = result;

  return result;
}


int checkPointer(const void * buffer)
{
  uint32_t *activepd = active_pd ();
  if (buffer == NULL)
    exit(-1);

  else if(!is_kernel_vaddr(buffer)) {
    if(pagedir_get_page (activepd, buffer) == NULL|| lookup_page (activepd, buffer, 0) == NULL)
      exit(-1);
  }

  else if (is_kernel_vaddr(buffer))
    exit(-1);

  return 0;
}
