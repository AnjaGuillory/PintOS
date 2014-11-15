#include <hash.h>
#include <string.h>
#include <syscall-nr.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
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
#include "devices/block.h"

/* Swap table for the swap partition */

/* A block device. */
struct block
  {
    struct list_elem list_elem;         /* Element in all_blocks. */

    char name[16];                      /* Block device name. */
    enum block_type type;                /* Type of block device. */
    block_sector_t size;                 /* Size in sectors. */

    const struct block_operations *ops;  /* Driver operations. */
    void *aux;                          /* Extra data owned by driver. */

    unsigned long long read_cnt;        /* Number of sectors read. */
    unsigned long long write_cnt;       /* Number of sectors written. */
  };


// Call block get role with BLOCK_SWAP once
// whenever a page is evicted from frame,
// get an open sector (freelist) (keep our own mapping of this)
// call block_write to that sector, pass in page pointer
// when a process is terminated, then add the sector to the freelist
// when we want to put the page back in a frame, use block_read

static struct swap_frame * swap_table[SWAP_SIZE];
struct block * b;



/* Initialize swap table with empty swap blocks */
void 
swap_init ()
{
  unsigned int i;
  for(i = 0; i < SWAP_SIZE; i++) {
    swap_table[i] = (struct swap_frame *) malloc (sizeof (struct swap_frame));
    swap_nullify (i);
  }
}

bool 
swap_write (void *kpage)
{
  printf("in swap write\n");

  b = block_get_role (BLOCK_SWAP);

	if (b == NULL)
		return false;

	block_sector_t sector_num = swap_get_free ();

	if (sector_num == SWAP_SIZE + 1)
		PANIC ("No free sectors!");
	else
	{
		struct page *p = page_lookup (kpage, 1);
    printf("putting into swap: %p    %d\n", p->upage, sector_num);

    p->page = PAGE_SWAP;
    //p->kpage = NULL;
    p->whereSwap = sector_num;
    int page_size = 4096;

    while (page_size > 0) {
      block_write (b, sector_num, kpage);
      page_size -= BLOCK_SECTOR_SIZE;
    swap_table[sector_num]->inUse = 1;
    swap_table[sector_num]->kpage = kpage;
      sector_num++;
    }

    hex_dump (kpage, kpage, 1024, true);

	}

	return true;

}

bool 
swap_read (void *kpage)
{
  b = block_get_role (BLOCK_SWAP);

  if (b == NULL)
    return false;

	block_sector_t sector_num = swap_find_sector (kpage);
  printf("block sector returned %d, blcok return from p->whereSwap \n", sector_num);//, page_lookup(kpage, true)->whereSwap);

  //hex_dump (kpage, kpage, 64, true);

  if (sector_num == SWAP_SIZE + 1)
    return false;

  else 
  {
    int page_size = 4096;

    while (page_size > 0) {
      block_read (b, sector_num, kpage);
      page_size -= BLOCK_SECTOR_SIZE;
		swap_nullify (sector_num);
      sector_num++;
    }
  

  hex_dump (kpage, kpage, 1024, true);
	}

	return true;
}

block_sector_t 
swap_get_free () 
{
	unsigned int i;

	for (i = 0; i < SWAP_SIZE; i++){
		if (swap_table[i]->inUse == 0)
			return i;
	}

	return SWAP_SIZE + 1;
}

block_sector_t 
swap_find_sector (void * kpage)
{
	unsigned int i;
	for (i = 0; i < SWAP_SIZE; i++){
		if (swap_table[i]->kpage == kpage)
			return i;
	}

	return SWAP_SIZE + 1;
}

void 
swap_nullify (block_sector_t index)
{
	swap_table[index]->inUse = 0;
	swap_table[index]->kpage = NULL;
}
