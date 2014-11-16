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
bool swap_write (void *kpage, struct thread *);
bool swap_read (block_sector_t sector_num, void *kpage);
block_sector_t swap_get_free (void);
block_sector_t swap_find_sector (void *kpage);
void swap_nullify (block_sector_t index);


#endif /* vm/swap.h */
