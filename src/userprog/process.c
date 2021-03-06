#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "kernel/list.h"
#include "threads/synch.h"

/*Driver: Anja*/

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, const char * command, void (**eip) (void), void **esp);

struct lock Lock;

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;

  strlcpy(fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  lock_init(&Lock);

  char *file_name = file_name_;

  char *str1, *token, *saveptr1;

  str1 = palloc_get_page (0);

  strlcpy (str1, file_name, PGSIZE);

  token = strtok_r(str1, " ", &saveptr1);

  //printf("FILENAME in start %s\n", file_name);
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (token, file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  else {
    struct thread *t = thread_current ();
    t->self = filesys_open(token);
    file_deny_write(t->self);
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{

  //printf("tid: %p\n", child_tid);

  /*Access parent of child*/
  struct thread *cur = thread_current ();
  struct thread *temp_child;
  //printf("name of current thread: %s\n", cur->name);
  
  if(!list_empty(&cur->children)) {
  
  struct list_elem *e;
    for (e = list_begin (&cur->children); e != list_end (&cur->children); e = list_next (e))
    {
      temp_child = list_entry (e, struct thread, child);
      if (temp_child->tid == child_tid)
      {
        break;
      }
    }
  }

  // List is empty, aka no children
  //printf("name it has after loop on %s\n", temp_child->name);
  
  //lock_acquire(&Lock);

  if (list_empty(&cur->children)) {
    //lock_release(&Lock);
    return -1;
  }

    // Child doesn't belong to this parent, 
    // aka 'child' made it out of the loop and was never
    // equal to child_tid
    ////printf("TEMP_CHILD %s\n", temp_child->name);
    if (temp_child != NULL && temp_child->tid != child_tid){
      //printf(" This child is not a direct child of the parent. Womp. \n");
      //lock_release(&Lock);
      return -1;
    }
    
    if (temp_child != NULL && temp_child->isWaited == 1)
    {
      ////printf("This child is already being waited on... \n");
      //lock_release(&Lock);
      return -1;
    }

    if (temp_child != NULL && temp_child->tid == child_tid){
      temp_child->isWaited = 1;
      sema_down(&temp_child->waiting);
      int exit_status = temp_child->child_exit;
      sema_up(&temp_child->exiting);
      return exit_status;
    }

  //palloc_free_page(&temp_child->waiting);
  return -1;
}


/* Free the current process's resources. */
void
process_exit (void)
{

  struct thread *cur = thread_current ();
  uint32_t *pd;
  
  file_close(cur->self);
  cur->self = NULL;

  //sema_up(&cur->waiting); //might need to move back to syscall_exit 
  /* Gets the name of the current thread */
  
  sema_up(&cur->waiting);
  if(cur->isWaited == 1)
  {
    sema_down(&cur->exiting);
  }

  int indx = 0;
  while (indx < 128)
    palloc_free_page(cur->files[indx]);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  struct thread *parent = cur->parent;
  struct list children_list = parent->children;

  struct list_elem *e;
  if (!list_empty (&children_list)) {
    for (e = list_begin (&children_list); e != list_end (&children_list);
               e = list_next (e))
            {
              struct thread *j = list_entry (e, struct thread, child);
              //printf("name of exiting thread %s\n", j->name);
              if (j->tid == cur->tid) {
                lock_acquire(&Lock);
                list_remove (e);
                lock_release(&Lock);
                break;
              }
            }

  }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

void the_stack(char * file_name, void **esp);
static bool setup_stack (char *file_name, void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, const char *command, void (**eip) (void), void **esp) 
{

  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;



  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  lock_acquire(&Lock);
  
  /* Open executable file. */
  file = filesys_open (file_name);
  
  lock_release(&Lock);
  
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack ((char *) command, esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;
  t->load_flag = success;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  
  //printf("about to sema up\n");
  sema_up(&(t->parent)->complete);
  thread_yield();
  //printf("im back from thread_yield() %s\n", t->name);
  
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (char *file_name, void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }

  the_stack(file_name, esp);

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}


void the_stack(char *file_name, void **esp)
{
  char *myEsp = (char *) *esp;
  char **token = palloc_get_page(PAL_ZERO);
  char **argv = palloc_get_page(PAL_ZERO);
  char *str1 = palloc_get_page (0);
  char *sptr1;
  int argc = 0;
  
  /* Make a copy of file_name */
  strlcpy (str1, file_name, PGSIZE);

  //printf("original esp %p\n", myEsp);

  /* Break file_name into tokens using
  * "/bin/ls -l foo bar" - > "/bin/ls", "-l", "foo", "bar"
  */

  int tk_indx = 0;
  for (;;str1 = NULL) {
    token[tk_indx] = strtok_r(str1, " ", &sptr1);

    /* Check if strtok_r() returns a null pointer */
    if (token[tk_indx] == NULL)
      break;

    tk_indx++;
  }

  argc = tk_indx; /* Set argc to the count of the token array (# of args) */
  int s = tk_indx-1;
  for (s; s >= 0; s--){
    myEsp -= strlen(token[s]) + 1;
    argv[s] = myEsp;
    memcpy(myEsp, token[s], strlen(token[s]) + 1);
    //printf("%p, argv[%d] '%s' char[%d]\n", myEsp, s, token[s], strlen(token[s]) +1);
  }

  //printf("length: %d\n", argc);

  /* Null Sentinel */
  argv[argc] = 0;

  /* Align to word size */
  int x = (unsigned int)myEsp % 4;
  //printf("%i\n", x);
  if (x != 0)
  {
    myEsp -= x;
    // memcpy(myEsp, &argv[argc], x);
  }

  /* Push the addresses of args onto the stack */
  int j;
  for (j = argc; j >= 0; j--) 
  {
    myEsp -= sizeof(char *);
    //printf("%p, argv[%d] '%p' char*\n", myEsp, j, argv[j]);
    memcpy(myEsp, &argv[j], sizeof(char *));
  }

  // Push argv
  char * tempEsp = myEsp;
  myEsp -= sizeof(char **);
  memcpy(myEsp, &tempEsp, sizeof(char **));

  // Push argc 
  myEsp -= sizeof(int);
  memcpy(myEsp, &argc, sizeof(int));

  // Push return address
  myEsp -= sizeof(char *);
  memcpy(myEsp, &argv[argc], sizeof(char *));

  /* Set esp back */
  *esp = myEsp;
 // hex_dump(*esp, *esp, PHYS_BASE-*esp, 1);
  // hex_dump(*esp, *esp, PHYS_BASE-*esp, 1);

  /* Free pages */
  palloc_free_page(argv);
  palloc_free_page(token);
  palloc_free_page(str1);

/*
- break filename into tokens
- "/bin/ls -l foo bar" - > "/bin/ls", "-l", "foo", "bar"
- store each token in list
- push the values in each token to the stack
- somewhere in here do word alignment 
  - (round the pointer to a multiple of 4 before first push)
- push a null sentinel
- push the address of each token from right to left
  - &bar, &foo, ...
- push argv (the pointer to the pointer of the token)
- push argc (the number of arguments - aka length of list)
- push return address (can be anything)
- set esp to myEsp
- call hexdump hex_dump(*esp, *esp, PHYS_BASE-*esp, 1)
- pintos --filesys-size=2 -p ../../examples/echo -a echo -- -f -q run 'echo x’
*/
}
