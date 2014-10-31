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
void frame_put (void * paddr, size_t page_cnt);

#endif /* vm/frame.h */
