#ifndef VM_SWAP_H
#define VM_SWAP_H

struct swap_frame {
  int inUse;	/* Is this swap in use? */
  void * kpage; /* Address of the evicted process's page */
  // int process_status; /* Status of process */ 
};

#endif /* vm/swap.h */
