#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/loader.h"

#define TABLE_SIZE (uint32_t) 1024	/* Size of frame table */
#define PG_SIZE (uint32_t) 4096 	/* Get rid of undeclared PG_SIZE error*/

struct frame_entry{			/* Entry struct to put in the frame atable array */
  void * addr;				/* Address that entry points to in MM */
  bool isAllocated;			/* Indicate if already in use */
  int clockbit;	     		/* Clock bit for clock algorithm */
  bool isStack;             /* Is this a stack page? */
  struct thread *owner;	    /* thread for frame */
  bool notevictable; 			/* sets whether the frame can be evicted or not */
};

void frametable_init (void);	/* Initialize frame table */
void frame_put (void *);		/* Put page into an available frame */
void frame_evict (void);		/* Evict a currently unnecessary frame */
struct frame_entry *frame_getEntry (void *); /* Finds the frame entry that maps to kpage */
int frame_find_kpage (void *);	/* Find a frame */
void frame_clean(int);			/* Clean an entry */
void frame_null (struct frame_entry *);	/* Nullify contents of entry for new usage */
void frame_stack (bool , void *);	/* Check if stack frame */
void free_frame (void *, struct thread *);  /* Free the frame, page, and swap when evicted */

#endif
