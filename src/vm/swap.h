#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"

#define SWAP_SIZE 8192

struct swap_frame {
  int inUse;	/* Is this swap in use? */
  void * kpage; /* Address of the evicted process's page */
  // int process_status; /* Status of process */ 
};


void swap_init (void);
bool swap_write (void *kpage);
bool swap_read (void *kpage);
block_sector_t swap_get_free (void);
block_sector_t swap_find_sector (void *kpage);
void swap_nullify (int index);


#endif /* vm/swap.h */
