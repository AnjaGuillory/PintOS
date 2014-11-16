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

/*** Dara drove here ***/

static struct frame_entry *frame_table[TABLE_SIZE];	/* Frame table */
static int frame_pointer;				/* Frame pointer for clock algorithm */
static struct lock Lock;				/* Lock synchronize frame table operations*/



void 
frametable_init ()
{
  /* Initialize Swap Table */
  swap_init ();

  /* Initialize mutex */
  lock_init(&Lock);
  /* Create entries for each index of the frame table */
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

/*** Dara and Andrea drove here ***/

/* Find a frame for a page */
/*This part MUST be synchronized */
void
frame_put (void * kpage){
  /* Acquire lock before entering */
  lock_acquire(&Lock);

  /* Boolean to determine if successful */
  bool success = 0;

  unsigned int i;

  /* If the page could not be found, return */
  if (frame_find_kpage(kpage) != -1)
    return;

  /* Search through table for an available frame */
  for(i = 0; i < TABLE_SIZE; i++) {

    /*Reset success to 0 to record success for every page
      This way, if there is at least one failure, the allocation will fail*/
    success = 0;

    struct frame_entry *entry = frame_table[i];
    
    /* If the entry is free, get it */
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

  /* If could not find an available entry, 
  * release lock and evict, then place new page into 
  * newly freed frame */
  if (success == 0) {

    /*Call the eviction policy*/
    lock_release(&Lock);
    frame_evict();
    frame_put(kpage);
  }
  else {
    lock_release(&Lock);
  }
}

/*** Andrea and Anja drove here ***/
void frame_evict(){
  /*** clock page algorithm
  * everytime we add a new frame, set clock bit to 1
  * when there are no more frames, go through looking for 0 clock bit,
  * until we find it, we set all clock bit == 1 to 0.  
  * when we find clockbit == 0, evict that frame, 
  * replace the page in the frame, and set clock bit to 1, 
  * then place the pointer after that frame ***/
  lock_acquire(&Lock);

  unsigned int i;
  for (i = frame_pointer; i < TABLE_SIZE; i++){
    struct frame_entry *entry = frame_table[i];
    if (entry->clockbit == 0 && entry->isAllocated == 1)
    {
      uint32_t * activepd = active_pd();


      if(entry->isStack == true || pagedir_is_dirty(activepd, entry->addr))
      {
        swap_write(entry->addr);
      }

      palloc_free_page (entry->addr);

      
      if (i == TABLE_SIZE - 1)
        frame_pointer = 0;
      else
        frame_pointer = i + 1;
      break;
    }
    else if (entry->isAllocated == 0) {
      frame_pointer = 0;
      i = frame_pointer;
    }
    else {
      entry->clockbit = 0;
    }
  }
  
  lock_release(&Lock);

}

/*** Dara drove here ***/
int frame_find_kpage (void * kpage){
  /* Find a given frame */
  unsigned int i;
  for(i = 0; i < TABLE_SIZE; i++)
  {
    if (frame_table[i]->addr == kpage)
      return i;
  }

  return -1;
}

/* Clean out the frame entry */
void frame_clean(int indx)
{
  frame_null(frame_table[indx]);
}

/* Nullify entry to renew it for availability */
void frame_null (struct frame_entry *entry){
  entry->addr = NULL;
  entry->frame_num = 0;
  entry->offset = 0;
  entry->isAllocated = 0;
  entry->clockbit = 0;
  entry->isStack = 0;
}

/* Check if frame is a stack frame, and handle appropriately */
void frame_stack (bool isStack, void *kpage) {

  if (isStack) {
    frame_table[frame_find_kpage (kpage)]->isStack = isStack;
  }
}

