#ifndef VM_SWAP_H
#define VM_SWAP_H

struct swap_frame {
  int inUse;	/* Is this swap in use? */
  void * upage; /* Address of the process's page that needs swapping*/
  int process_status; /* Status of process */ 
}

#endif /* vm/swap.h */
