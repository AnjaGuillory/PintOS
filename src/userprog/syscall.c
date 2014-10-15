#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "lib/kernel/console.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  char* myEsp = f->esp;
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
  printf ("num: %d\n", num);

  myEsp += 4;
  int status;
  int fd;
  void *buffer;
  unsigned size;

  f->esp = myEsp;
  hex_dump(f->esp, f->esp, PHYS_BASE-f->esp, 1);
  // void *buffer;



  /* SWITCHHHHHH */
  switch (num) {
    case 0:
      // HALT
      break;
    case 1:
      // EXIT
      status = *myEsp;
      exit (status);
      break;
    case 2:
      // EXEC
      break;
    case 3:
      // WAIT
      break;
    case 4:
      // CREATE
      break;
    case 5:
      // REMOVE
      break;
    case 6:
      // OPEN
      break;
    case 7:
      // FILESIZE
      break;
    case 8:
      // READ
      break;
    case 9:
      // WRITE
      fd = *myEsp;
      myEsp += 4;
      buffer = *myEsp;
      myEsp += 4;
      size = *myEsp;
      int bytes = write (fd, buffer, size);
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
  thread_exit ();
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
  thread_exit();

  // Instead of returning status, we can set wait -> status pointer to the status
  // return status;
}

void halt(void)
{
  /*Call wait on initial process*/
  shutdown_power_off ();
}

int write (int fd, const void *buffer, unsigned size) {

  printf ("%d\n", fd);
  printf("%p\n", buffer);
  printf("%d\n", size);

  if (fd == 0){
    printf("dsnf");
  }

  else if (fd == 1){
    putbuf((char *)buffer, size);
    console_print_stats();
  }

  return 0;
}
