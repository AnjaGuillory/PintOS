#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
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

struct file *files[128];        /* An array of files */
int position;                   /* Keeps the position of the last element present in the array */

int global_status;              /* Global status for exit function */

static struct lock Lock;

static void syscall_handler (struct intr_frame *);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int* getArgs(int* myEsp, int count);
pid_t exec (const char *cmd_line);
int checkPointer(void * buffer);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&Lock);
}

int* getArgs(int * myEsp, int count) {
  int* args = palloc_get_page(0);

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
  /* Check if pointer is a user virtual address 
   * NEED: add check for unmapped territory
   */

  uint32_t *activepd = active_pd ();
  if (is_user_vaddr (f->esp)){
    if (lookup_page (activepd, f->esp, 0) == NULL){
      /* terminate the process and free its resources */
      // pagedir_destroy(activepd);
      thread_exit ();
    }
  }

  /* Check if pointer is a kernel virtual address (BAD) */
  else if (is_kernel_vaddr (f->esp)){
    /* terminate the process and free its resources 
    * don't need to call pagedir_destroy because it is called by process_exit()
    * which is called by thread_exit()
    */
    // pagedir_destroy(activepd); 
    thread_exit();
  }

  uint32_t num = *myEsp;
  //printf ("num: %d\n", num);

  int sizes;
  char *cmd_line;

  int* args = palloc_get_page(0);

  files[0] = palloc_get_page (0);
  struct filing *fil = palloc_get_page (0);
  
  /* File descriptors cannot be 0 or 1 */
  fil->fd = 2;
  position = 2;

  /* Creates a node with the first file descriptor open */
  list_push_back(&thread_current()->open_fd, &fil->elms);

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
      cmd_line = *(myEsp + 1);
      pid_t execs = exec (cmd_line);
      f->eax = execs;
      break;
    case SYS_WAIT:
      // WAIT
      args = getArgs (myEsp, 1);
      f->eax = wait (args[0]);
      break;
    case SYS_CREATE:
      // CREATE
      //file = *(myEsp + 1);
      //initial_size = *(myEsp + 2);
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
  
  //printf ("system call!\n");
  
  /* Get the system call number */
  /* Get any system call arguments */
  /* Switch statement checks the number and calls the right function */
  /* Implement each function */
  
  palloc_free_page(args);
  //thread_exit ();
}

void exit (int status) {
  struct thread *cur = thread_current ();

  global_status = status;
  /*printf("in exit the id of the one exiting %d\n", cur->tid);
  printf("exit status %d\n", status);
  printf("i'm upping %s\n", cur->name);*/
  sema_up(&cur->waiting);

  (cur->parent)->child_exit = status;
  
  printf("%s: exit(%d)\n", cur->name, status);
  /*char * stat = &status;
  //printf("\n");
  putbuf(cur->name, strlen(cur->name));
  putbuf(": exit(", 7);
  putbuf(&stat, 2);
  printf("\n");*/
  
  thread_exit();
}

void halt(void)
{
  /*Call wait on initial process*/
  shutdown_power_off ();
}


int write (int fd, const void *buffer, unsigned size) {

  //printf ("%d\n", fd);
  //printf("%p\n", buffer);
  //printf("%d\n", size);
  int charWritten = 0;
  
  if (fd > 1){
    
    /* Checks buffer for a bad pointer */
    uint32_t *activepd = active_pd ();
    if (!is_kernel_vaddr (buffer)) {
      if(pagedir_get_page (activepd, buffer) == NULL|| lookup_page (activepd, buffer, 0) == NULL || buffer == NULL)
      exit(-1);
    }
    else if(is_kernel_vaddr (buffer))
      exit(-1);

    /* Checks if fd is within bounds of array */
    if(fd < 2 || fd > 127)
      return -1;

    /* Gets the file from the files array */
    struct file * fil = files[fd];

    /* Checks if the file at fd was valid */
    if(fil == NULL){
      return -1;
    }

    /* Writes to the file and puts number of written characters */
    charWritten = file_write (fil, (char *) buffer, size);

    //return charWritten;
  }

  else if (fd == 1){
    

    if (size < 300) {
      putbuf((char *) buffer + charWritten, size);
      charWritten += size;
    }
    else {
      while (size > 300) {
      putbuf((char *)buffer + charWritten, 300);
      charWritten += 300;
      size -= 300;
      console_print_stats();
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

  uint32_t *activepd = active_pd ();
    if (!is_kernel_vaddr (file)) {
      if(pagedir_get_page (activepd, file) == NULL|| lookup_page (activepd, file, 0) == NULL || file == NULL)
      exit(-1);
    }
    else if(is_kernel_vaddr (file))
      exit(-1);
  
  int flag = filesys_create (file, initial_size);

  lock_release(&Lock);

  return flag;
}

int open (const char *file)  {

  lock_acquire(&Lock);

  // Needed to check for bad pointers (not working) 
 /*if(checkPointer(file) == -1)
 {
  
 }
  printf("Check pointer failed")
  exit(-1); */


  /* Opens the file */
  struct file *openFile = filesys_open(file);


  struct thread *cur = thread_current();

  /* Gets an open fd */
  struct filing *fil = list_entry(list_pop_front(&cur->open_fd), struct filing, elms);
  
  /* Allocate space for the index */
  files[fil->fd] = palloc_get_page(0);

  /* Checks if the file system was able to open the file 
      and if the number of files has exceeded 128 */
  if(openFile == NULL || fil->fd > 127 || position > 127)
  {
    palloc_free_page(files[fil->fd]);
    return -1;
  }

  /* Puts the file into the array */
  files[fil->fd] = openFile;

  /* Sets the file descriptor to the open fd */
  int fd = fil->fd;

  /* If extending past the current bound, increase bound */
  if (fd == position)
    fil->fd = ++position;
  //fil->fd++;

  /* Adds the next index to the list of open fds for the thread */
  list_push_back(&(cur->open_fd), &fil->elms);
  
  lock_release(&Lock);

  return fd;
}

int filesize (int fd) {
  /* Returns the length of the file */
  int len = file_length (files[fd]);
  
  return len;
}

unsigned tell (int fd) {
  /* Returns the current position in the file */
  int pos = file_tell (files[fd]);

  return pos;
}

void seek (int fd, unsigned position) {
  /* Checks if the position to change to is within the file */
  if(position > (unsigned) file_length (files[fd]))
    exit(-1);

  /* Sets the file position to position */
  return file_seek (files[fd], position);
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
  /* Checks if fd is within the array */
  if(fd < 2 || fd > 127)
      exit(-1);
  
  /* Closes the file */
  file_close (files[fd]);

  /* Opens spot in array */
  files[fd] = NULL;

  struct thread *cur = thread_current();
  struct filing *fil = palloc_get_page(0);
  fil->fd = fd;

  /* Adds freed fd to list of open fds */
  list_push_back(&cur->open_fd, &fil->elms);

  palloc_free_page(fil);
}

int read (int fd, void *buffer, unsigned size) 
{
  int charsRead = 0;
  
  if(fd == 0)
  {
    charsRead = input_getc();
  }
  else
  {
    /* Checks buffer for a bad pointer */
    uint32_t *activepd = active_pd ();
    if (!is_kernel_vaddr (buffer)) {
      if(pagedir_get_page (activepd, buffer) == NULL|| lookup_page (activepd, buffer, 0) == NULL || buffer == NULL)
      exit(-1);
    }
    else if(is_kernel_vaddr (buffer))
      exit(-1);

    /* Checks if fd is within bounds of array */
    if(fd < 2 || fd > 127)
      return -1;

    /* Gets the file from the files array */
    struct file * fil = files[fd];

    /* Checks if the file at fd was valid */
    if(fil == NULL){
      return -1;
    }

    /* Writes to the file and puts number of written characters */
    charsRead = file_read (fil, (char *) buffer, size); 
    //printf("chars %d\n", charsRead);

    //return charWritten;
  }

  return charsRead;
}

pid_t exec (const char *cmd_line) 
{
  if(cmd_line == NULL)
    return -1;
  
  struct thread *cur = thread_current ();
  pid_t result;
  
  lock_acquire(&Lock);
  result = process_execute((char *) cmd_line);
  lock_release(&Lock);
  
  /* If process execute didn't create a thread */
  if (result == TID_ERROR)
    return -1;

  sema_down(&cur->complete);

  struct list_elem *e;

  for (e = list_begin (&cur->children); e != list_end (&cur->children); e = list_next (e))
            {
              struct thread *j = list_entry (e, struct thread, child);
              if (j->tid == result) {
                if (j->load_flag == 1) {
                  return result;
                }
                else {
                    list_remove(e);
                    return -1;
                }
              }
            }

  return -1;
}

int wait (pid_t pid) {
  struct thread *cur = thread_current();
  int result = process_wait(pid);
  cur->child_exit = result;
  printf("RETURN STATUS: %d\n", result);
  if (result == -1)
    exit(-1);
  else
    return result;
}


int checkPointer(void * buffer)
{
    uint32_t *activepd = active_pd ();
    if (!is_kernel_vaddr (buffer)) {
      if(pagedir_get_page (activepd, buffer) == NULL|| lookup_page (activepd, buffer, 0) == NULL || buffer == NULL)
        return -1;
    }
    else if(is_kernel_vaddr (buffer))
        return -1;
    else
      return 0;
}