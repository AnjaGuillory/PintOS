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

static struct swap_frame * swap_table[8192];
struct block * b;



/* Initialize swap table with empty swap blocks */
void 
swap_init ()
{
	b = block_get_role (BLOCK_SWAP);
  unsigned int i;
  for(i = 0; i < b->size; i++)
  	swap_nullify (i);
}

bool 
swap_write (void *kpage)
{

	if (b == NULL)
		return false;

	block_sector_t sector_num = swap_get_free ();

	if (sector_num == b->size + 1)
		PANIC ("No free sectors!");

	else
	{
		block_write (b, sector_num, kpage);
		swap_table[sector_num]->inUse = 1;
		swap_table[sector_num]->kpage = kpage;

		struct page *p = page_lookup (kpage, 1);
		p->swappedIn = 1;
		p->whereSwap = sector_num;
	}

	return true;

}

bool 
swap_read (void *kpage)
{

	block_sector_t sector_num = swap_find_sector (kpage);

	if (sector_num == b->size + 1)
		return false;

	else 
	{
		block_read (b, sector_num, kpage);
		swap_nullify (sector_num);
	}

	return true;
}

block_sector_t 
swap_get_free () 
{
	unsigned int i;

	for (i = 0; i < b->size; i++){
		if (swap_table[i]->inUse == 0)
			return i;
	}

	return b->size + 1;
}

block_sector_t 
swap_find_sector (void * kpage)
{
	unsigned int i;
	for (i = 0; i < b->size; i++){
		if (swap_table[i]->kpage == kpage)
			return i;
	}

	return b->size + 1;
}

void 
swap_nullify (int index)
{
	swap_table[index]->inUse = 0;
	swap_table[index]->kpage = NULL;
}
