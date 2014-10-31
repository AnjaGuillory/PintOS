#include "userprog/syscall.h"
#include <stdio.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdlib.h>
#include <hash.h>
#include <string.h>
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

//struct hash frametable;
static struct frame *frame_table[TABLE_SIZE];
static struct lock Lock;


void 
frametable_init (){
  unsigned int i;
  for(i = 0; i < TABLE_SIZE; i++) {
    frame_table[i] = (struct frame *) malloc (sizeof (struct frame));
    frame_null();
  }  
}

void 
frame_put (void * paddr, size_t page_cnt){
  bool success = 0;

  unsigned int i;
  for(i = 0; i < TABLE_SIZE; i++) {
    struct frame *frame = frame_table[i];
    if(frame->isAllocated == 0) {
      frame->pageAddr = paddr;
      frame->frame_num = (uint32_t) paddr & 0xFF400000;
      frame->offset = (uint32_t) paddr & 0x000FFFFF;
      frame->isAllocated = 1;
      frame->pagedIn = 1;

      success = 1;
      break;
    }
  }

  if (success == 0) {
    // call the eviction policy
    frame_evict();
    PANIC ("RAN OUT OF FRAME PAGES");
  }
  //hash_insert(frame);
}

void frame_evict(){
  // clock page algorithm
}

void frame_clean(void * paddr)
{
  unsigned int i;
  for(i = 0; i < TABLE_SIZE; i++)
  {
    if (frame_table[i]->pageAddr == paddr)
    {
    frame_null();
    break;
    }
  }
}

void frame_null (){
  frame->pageAddr = NULL;
  frame->frame_num = 0;
  frame->offset = 0;
  frame->isAllocated = 0;
  frame->pagedIn = 0;
}

  
/* Returns a hash value for page p. */
// unsigned
// frame_hash (const struct hash_elem *p_, void *aux UNUSED)
// {
//   const struct page *p = hash_entry (p_, struct page, hash_elem);
//   return hash_bytes (&p->addr, sizeof p->addr);
// }

// /* Returns true if page a precedes page b. */
// bool
// frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
//            void *aux UNUSED)
// {
//   const struct page *a = hash_entry (a_, struct page, hash_elem);
//   const struct page *b = hash_entry (b_, struct page, hash_elem);

//   return a->addr < b->addr;
// }



// /* Returns the page containing the given virtual address,
//    or a null pointer if no such page exists. */
// struct page *
// frame_lookup (const void *address)
// {
//   struct page p;
//   struct hash_elem *e;

//   p.addr = address;
//   e = hash_find (&pages, &p.hash_elem);
//   return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
// }


