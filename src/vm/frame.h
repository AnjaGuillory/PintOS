#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/loader.h"

#define TABLE_SIZE (uint32_t) 1024	/* Size of frame table */
#define PG_SIZE (uint32_t) 4096 	/* Get rid of undeclared PG_SIZE error*/

struct frame_entry{			/* Entry struct to put in the frame atable array */
  void * addr;				/* Address that entry points to in MM */
  uint32_t frame_num;		/* Frame number */
  uint32_t offset;			/* Offset */
  bool isAllocated;			/* Indicate if already in use */
  int clockbit;	     		/* Clock bit for clock algorithm */
};

void frametable_init (void);
void frame_put (void *, size_t);
void frame_evict (void * , size_t);
int frame_find_kpage (void *);
void frame_clean(int);
void frame_null (struct frame_entry *);
#endif
