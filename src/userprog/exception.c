#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/vaddr.h"


/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      exit (-1); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      exit (-1);
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{

  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */
  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

   //printf("fault_addr %p\n", fault_addr);
  //printf("Esp is below PHYS_BASE: %d\n",f->esp < PHYS_BASE);
  //printf("Fault addr: %p\n", fault_addr);
  //printf("fault addr %p esp %p is kernel vaddr %d\n", fault_addr, f->esp, is_kernel_vaddr(fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  struct thread * blah = thread_current();
  struct page *p;
  /*printf("Name of current process: %s\n", blah->name);
  printf("User stack pointer: %p\n", blah->stack);
  printf("Kernel stack pointer?: %p\n", f->esp);*/
  /*if(f->esp > PHYS_BASE)
  {
    printf("haha ima bove PHYS_BASE");
    kill(f);
  }*/

    if (!user && not_present) {
     //  printf("hey %p %p %p\n\n\n\n\n\n\n\n\n", fault_addr, blah->myEsp, f->esp);
      fault_addr = (uint32_t) fault_addr & 0xFFFFF000;

      page_insert (fault_addr, NULL);
        /* Get the page to set its attributes */
        p = page_lookup (fault_addr, false, thread_current());
        
        /* If unsuccessful after insertion, check insertion method*/
        if (p == NULL) {
          printf("killed because of kernel address: %p, esp: %p\n");
          kill (f);
        }

        /* Set attributes */
        p->isStack = 1;
        p->writable = 1;
        p->isZero = true;
        p->read_bytes = PGSIZE;

        void * kpage = palloc_get_page(PAL_USER);
frame_stack (p->isStack, kpage);
if (page_load (p, kpage) == false) {
        kill (f);
      }
page_insert(fault_addr, kpage);
pagedir_set_page (active_pd(), fault_addr, kpage, p->writable);
//printf("returning\n");
return;
      
    }
  if (!not_present) {
    //printf("fault addr %p\n", fault_addr);
    //debug_backtrace();
    //PANIC ("i died");
  }

    if (!not_present) {
      //printf("killing\n");
      exit(-1);
    }

  /* Find how far below the fault addr is from f->esp*/
  int diff = fault_addr - f->esp;
  if (diff < 0)
    diff = diff * -1;
  bool pusha = (diff >= 4) && (diff <= 32);
   /*printf("f->esp: %d, fault_addr: %p pusha: %d\n", f->esp, fault_addr, pusha);
   printf("thread esp %p\n", thread_current()->myEsp);*/


  bool belowEsp = fault_addr < f->esp;
  bool equal = fault_addr == f->esp;
  //printf("fault addr %p\n", fault_addr);
        //printf("blah esp %p %p, fault addr %p\n", blah->myEsp, f->esp, fault_addr);
  fault_addr = (uint32_t) fault_addr & 0xFFFFF000;


  /* If it is the user and the page is not accessed */
  if (user && not_present) {

    /* Checks if buffer doesn't have anything in it */
    if (fault_addr == NULL) {   
      debug_backtrace();
      printf("killed because address is NULL\n");   
      kill(f);
    }
    else if (is_kernel_vaddr(fault_addr)) {
      //debug_backtrace();
      printf("killed because of kernel address: %p, esp: %p, kernel stack pointer %p\n", fault_addr, f->esp, blah->myEsp);
      kill (f);
    }

    /* If could not find page in page directory, search in supplemental page table */
    p = page_lookup (fault_addr, false, thread_current());
    //printf("p->upage %p\n", p->upage);

    /* If it is not in the supplemental page table*/
    if (p == NULL) {
      if (pusha || equal) {

        //printf("faulat addr < esp\n");
        /* Add to supplemental page table */
        page_insert (fault_addr, NULL);
        /* Get the page to set its attributes */
        p = page_lookup (fault_addr, false, thread_current());
        
        /* If unsuccessful after insertion, check insertion method*/
        if (p == NULL){
          printf("killed because page is NULL\n");          
          kill (f);
        }

        /* Set attributes */
        p->isStack = 1;
        p->writable = 1;
        p->isZero = true;
        p->read_bytes = PGSIZE;
        
      }
      /*else if(f->esp < PHYS_BASE)
      {
        printf("fault addr %p %p\n", fault_addr, f->esp);
        printf("asdfjiwofjiaefj %p %p\n", thread_current()->myEsp, f->esp);
      }*/
      /* If the address is not valid, kill process */
      //else if (fault_addr > f->esp && fault_addr > PHYS_BASE) {}
      else
      {
     //  printf("Are you here? \n");
        printf("killed because of bad stack growth\n");
        kill (f);
      }

    }

    /*  */
    if(p->kpage == NULL) {
      ////printf("hey\n");
      void * kpage = palloc_get_page(PAL_USER);

      //printf("kpage %p fault_addr %p\n", kpage, fault_addr);
      /* If run out of user pool space, panic!*/
      //if (kpage == NULL)
        //printf("heyasdjfklsjfwsiefojwed\n");
      /* Check if it is a stack page, and save this information*/
      frame_stack (p->isStack, kpage);
      // printf("p->writable %d \n", p->writable);

      /* Load the page to memory, if cannot, kill */
      if (page_load (p, kpage) == false) {
        printf("killed because can't load page \n");
        kill (f);
      }

      /* If load is successful, update supplemental in the insertion method */
      page_insert(fault_addr, kpage);
      /* Update the page directory with new page*/
      pagedir_set_page (active_pd(), fault_addr, kpage, p->writable);
      // printf("going to return\n");
      return;
    }
    

  }

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");

  printf("There is no crying in Pintos!\n");
  kill (f);

}

