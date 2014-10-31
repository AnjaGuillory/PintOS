#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/loader.h"

// #define TABLE_SIZE (uint32_t) init_ram_pages
 #define TABLE_SIZE (uint32_t) 1024

struct frame {
  void * addr;
  uint32_t frame_num;
  uint32_t offset;
  bool isAllocated;
  bool pagedIn;
  int clockbit;
  //struct hash_elem hash_elem; /* Hash table element. */
};

void frametable_init (void);
void frame_put (void *, size_t);
void frame_evict (void * , size_t);
int frame_find_kpage (void *);
void frame_clean(int);
void frame_null (struct frame *);

#endif /* vm/frame.h */
