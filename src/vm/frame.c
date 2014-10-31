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
static int frame_pointer;
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
frame_put (void * kpage, size_t page_cnt){
  bool success = 0;

  int i;
  for(i = 0; i < TABLE_SIZE; i++) {
    struct frame *frame = frame_table[i];
    if(frame->isAllocated == 0) {
      frame->addr = kpage;
      frame->frame_num = (uint32_t) paddr & 0xFF400000;
      frame->offset = (uint32_t) paddr & 0x000FFFFF;
      frame->isAllocated = 1;
      frame->pagedIn = 1;
      frame->clockbit = 1;

      success = 1;
      break;
    }
  }

  if (success == 0) {
    // call the eviction policy
    frame_evict(kpage, page_cnt);
    PANIC ("RAN OUT OF FRAME PAGES");
  }
  //hash_insert(frame);
}

void frame_evict(void * kpage, size_t page_cnt){
  // clock page algorithm
  // everytime we add a new frame, set clock bit to 1
  // when there are no more frames, go through looking for 0 clock bit,
  // until we find it, we set all clock bit == 1 to 0. 
  // when we find clockbit == 0, evict that frame,
  // replace the page in the frame, and set clock bit to 1,
  // then place the pointer after that frame

  int i;
  for (i = 0; i < TABLE_SIZE; i++){
    struct frame *frame = frame_table[i];

    if (frame->clockbit == 0)
    {
      frame_clean(i);
      frame_put(kpage, page_cnt);
      if (i == TABLE_SIZE - 1)
        frame_pointer = 0;
      else
        frame_pointer = i + 1;
      break;
    }
    else
      frame->clockbit = 0;
  }


}


int frame_find_kpage (void * kpage){
  int i;
  for(i = 0; i < TABLE_SIZE; i++)
  {
    if (frame_table[i]->pageAddr == paddr)
      return i;
  }
}

void frame_clean(int indx)
{
  frame_null(frame_table[indx]);
}

void frame_null (struct frame *frame){
  frame->addr = NULL;
  frame->frame_num = 0;
  frame->offset = 0;
  frame->isAllocated = 0;
  frame->pagedIn = 0;
  frame->clockbit = 0;
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


