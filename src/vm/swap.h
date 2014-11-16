#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"

#define SWAP_SIZE 8192

struct swap_frame {
  int inUse;	/* Is this swap in use? */
  void * kpage; /* Address of the evicted process's page */
};


void swap_init (void);	/* Initialize swap table */
bool swap_write (void *kpage, struct thread *);	/* Write evicted frame to swap block */
bool swap_read (block_sector_t sector_num, void *kpage);	/* Read frame from swap block */
block_sector_t swap_get_free (void);	/* Get a free swap entry from swap table */
block_sector_t swap_find_sector (void *kpage);	/* Find a sector within a block */
void swap_nullify (block_sector_t index);	/* Nullify entry when no longer in use */

#endif /* vm/swap.h */
