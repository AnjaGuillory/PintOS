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
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"

static struct frame_entry *frame_table[TABLE_SIZE];	/* Frame table */
static int frame_pointer;				/* Frame pointer for clock algorithm */
static struct lock Lock;				/* Lock synchronize frame table operations*/



void 
frametable_init ()
{
  /* Initialize Swap Table */
  swap_init ();

  lock_init(&Lock);
  frame_pointer = 0;

  unsigned int i;
  for(i = 0; i < TABLE_SIZE; i++) {
    frame_table[i] = (struct frame_entry *) malloc (sizeof (struct frame_entry));
    frame_null(frame_table[i]);
  }  
}

struct frame_entry *
getFrameEntry (){
  return frame_table[0];
}

/*Problem: We're not checking if the address we are pointing to 
 is actually available in memory, also we're not error checking
the address. Also, how do we need to indicate to the process where
each page ends so that is doesn't access another process' frame?*/

/*Is kpage a frame address or a vm address?*/
/*Most likely will need hash table to map memory */

/*This part MUST be synchronized */
void
frame_put (void * kpage){
  lock_acquire(&Lock);
  bool success = 0;

  unsigned int i;

  // printf("KPAGE: %p\n", kpage);

  for(i = 0; i < TABLE_SIZE; i++) {
    /*Reset success to 0 to record success for every page
      This way, if there is at least one failure, the allocation will fail*/
    success = 0;

    struct frame_entry *entry = frame_table[i];
    
    if(entry->isAllocated == 0) {
      entry->addr = kpage;
      entry->frame_num = (uint32_t) kpage & 0xFF400000;
      entry->offset = (uint32_t) kpage & 0x000FFFFF;
      entry->isAllocated = 1;

      entry->clockbit = 1;

      success = 1;
      break;
    }
  }

  if (success == 0) {
    /*Call the eviction policy*/
    lock_release(&Lock);
    frame_evict(kpage);
  }
  else
    lock_release(&Lock);
}

void frame_evict(void * kpage){
  // clock page algorithm
  // everytime we add a new frame, set clock bit to 1
  // when there are no more frames, go through looking for 0 clock bit,
  // until we find it, we set all clock bit == 1 to 0. `
  // when we find clockbit == 0, evict that frame,
  // replace the page in the frame, and set clock bit to 1,
  // then place the pointer after that frame
  lock_acquire(&Lock);
  //printf("WE'RE IN FRAME EVICT\n\n\n\n\n\n");
  //int accessed = hash_entry(thread_current()->page_table, page, page);

  unsigned int i;
  for (i = frame_pointer; i < TABLE_SIZE; i++){
    struct frame_entry *entry = frame_table[i];
    if (entry->clockbit == 0)
    {
      uint32_t * activepd = active_pd();

      if(entry->isStack == true || pagedir_is_dirty(activepd, entry->addr))
      {
        printf("going to swap write with %p\n", entry->addr);
        swap_write(entry->addr);
      }

      palloc_free_page (entry->addr);
      //frame_clean(i);
      lock_release (&Lock);
      frame_put(kpage);
      lock_acquire (&Lock);
      
      if (i == TABLE_SIZE - 1)
        frame_pointer = 0;
      else
        frame_pointer = i + 1;
      break;
    }
    else
      entry->clockbit = 0;
  }
  
  printf("exiting frame evict\n");
  lock_release(&Lock);

}

int frame_find_kpage (void * kpage){
  unsigned int i;
  for(i = 0; i < TABLE_SIZE; i++)
  {
    if (frame_table[i]->addr == kpage)
      return i;
  }

  return -1;
}

void frame_clean(int indx)
{
  frame_null(frame_table[indx]);
}

void frame_null (struct frame_entry *entry){
  entry->addr = NULL;
  entry->frame_num = 0;
  entry->offset = 0;
  entry->isAllocated = 0;
  //frame_entry->pagedIn = 0;
  entry->clockbit = 0;
  entry->isStack = 0;
}

void frame_stack (bool isStack, void *kpage) {

  if (isStack) {
  printf("in frame stack\n");
    frame_table[frame_find_kpage (kpage)]->isStack = isStack;
  }
}

