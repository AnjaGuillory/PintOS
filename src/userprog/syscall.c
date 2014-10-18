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

/* Struct that holds the file descriptor */
struct filing {
    int fd;
    struct list_elem elms;
};

/* An array of files */
struct file *files[128];
int global_status;

static void syscall_handler (struct intr_frame *);
bool create (const char *file, unsigned initial_size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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

  // myEsp += 4;
  int status;
  int fd;
  void *buffer;
  unsigned size;
  const char *file;
  unsigned initial_size;
  const char *file_sys;
  bool creation = 0;

  files[0] = palloc_get_page(0);
  struct filing *fil = palloc_get_page(0);
  
  /* File descriptors cannot be 0 or 1 */
  fil->fd = 2;

  /* Creates a node with the first file descriptor open */
  list_push_back(&thread_current()->open_fd, &fil->elms);

  /* SWITCHHHHHH */
  switch (num) {
    case 0:
      // HALT
      halt();
      break;
    case 1:
      // EXIT
      status = *(myEsp + 1);
      exit (status);
      //int exit_status = exit (status);
      f->eax = global_status;
      break;
    case 2:
      // EXEC
      break;
    case 3:
      // WAIT
      break;
    case 4:
      // CREATE
      file = *(myEsp + 1);
      initial_size = *(myEsp + 2);
      creation = create(file, initial_size);
      f->eax = creation;
      break;
    case 5:
      // REMOVE
      break;
    case 6:
      // OPEN
        file_sys = *(myEsp + 1);
      if(creation)
      {
        int file_des = open(file_sys);
        f->eax = file_des;
      } else {
        f->eax = -1;
      }
      break;
    case 7:
      // FILESIZE
      break;
    case 8:
      // READ
      break;
    case 9:
      // WRITE
      fd = *(myEsp + 1);
      buffer = *(myEsp + 2);
      size =  *(myEsp + 3);
      int bytes = write (fd, buffer, size);
      f->eax = bytes;
      break;
    case 10:
      // SEEK
      break;
    case 11:
      // TELL
      break;
    case 12:
      // CLOSE
      break;
  } 
  
  printf ("system call!\n");
  
  /* Get the system call number */
  /* Get any system call arguments */
  /* Switch statement checks the number and calls the right function */
  /* Implement each function */
  //thread_exit ();
}

void exit (int status) {
  struct thread *cur = thread_current ();
  struct thread *parent = cur->parent;
  struct list children_list = parent->children;

  struct list_elem *e;
  if (!list_empty (&children_list)) {
    for (e = list_begin (&children_list); e != list_end (&children_list);
               e = list_next (e))
            {
              struct thread *j = list_entry (e, struct thread, child);
              if (j->tid == cur->tid) {
                list_remove (e);
                break;
              }
            }

  }
  global_status = status;
  thread_exit();

  printf("hello");
  // Instead of returning status, we can set wait -> status pointer to the status
  //return status;
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
    
    uint32_t *activepd = active_pd ();
    if (is_kernel_vaddr (buffer) || lookup_page (activepd, buffer, 0) == NULL || buffer == NULL) {
      exit (1);
    }

    /* Gets the file from the files array */
    struct file * fil = files[fd];

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
  if(file == NULL)
    exit(-1);
    
  return filesys_create (file, initial_size);
}

int open (const char *file)  {
  
  /* Opens the file */
  struct file *openFile = filesys_open(file);

  struct thread *cur = thread_current();

  struct filing *fil = list_entry(list_pop_front(&cur->open_fd), struct filing, elms);
  
  /* Allocate space for the index */
  files[fil->fd] = palloc_get_page(0);

  /* Checks if the file system was able to open the file 
      and if the number of files has exceeded 128 */
  if(openFile == NULL || fil->fd > 127)
  {
    palloc_free_page(files[fil->fd]);
    return -1;
  }

  /* Sets the file descriptor to a open fd */
  files[fil->fd] = openFile;

  int fd = fil->fd;
  fil->fd++;
  //fil->fd = list_size(&(cur->open_files)) + 2;

  /* Adds the next index to the list of open fds for the thread */
  list_push_back(&(cur->open_fd), &fil->elms);

  return fd;
}
