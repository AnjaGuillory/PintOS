#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* Check if pointer is a user virtual address 
   * NEED: add check for unmapped territory
   */
  uint32_t *activepd = active_pd();
  if (is_user_vaddr(f->esp)){
    if (lookup_page(activepd, f->esp, 0) == NULL){

      /* terminate the process and free its resources */
      pagedir_destroy(activepd);
      thread_exit();
    }
  }

  /* Check if pointer is a kernel virtual address (BAD) */
  else if (is_kernel_vaddr(f->esp)){

    /* terminate the process and free its resources */
    pagedir_destroy(activepd);
    thread_exit();
  }

  char* myEsp = f->esp;

  uint32_t num = *myEsp;
  printf("num: %d\n", num);

  myEsp -= 4;
  int status;

  /* SWITCHHHHHH */
  switch(num) {
    case 0:
      // HALT
      break;
    case 1:
      // EXIT
      status = *myEsp;
      status = exit(status);
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

int exit (int status) {
  if(status == 0)
    thread_exit();
  else if(status == 1)
    thread_exit();
    //call error checker
  return status;
}

void halt()
{
  // Call wait on initial process
  shutdown_power_off();
} 
