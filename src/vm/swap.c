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

/*** Andrea drove here ***/

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


/*** Call block get role with BLOCK_SWAP once
* whenever a page is evicted from frame,
* get an open sector (freelist) (keep our own mapping of this)
* call block_write to that sector, pass in page pointer
* when a process is terminated, then add the sector to the freelist
* when we want to put the page back in a frame, use block_read ***/

/* Swap table for the swap partition */
static struct swap_frame * swap_table[SWAP_SIZE];

/* Declare a block */
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

/*** Andrea and Anja drove here ***/

/* Write evicted frame into swap space */
bool 
swap_write (void *kpage)
{
  /* Get a free block from partition */
  b = block_get_role (BLOCK_SWAP);

  /* If block is null, return as unsuccessful */
  if (b == NULL)
    return false;

  /* Get a free swap entry in swap table */
  block_sector_t sector_num = swap_get_free ();

  /* If searched all of swap table and no available entry found, panic */
  if (sector_num == SWAP_SIZE + 1)
    PANIC ("No free sectors!");
  else
  {
    /* Look up the page to write */
    struct page *p = page_lookup (kpage, 1);

    /* Update content information accordingly */
    p->page = PAGE_SWAP;
    p->whereSwap = sector_num;
    
    int page_size = 0;

    /* Write everything in frame to block */
    while (page_size < PGSIZE) {
      block_write (b, sector_num, kpage + page_size);
      
      swap_table[sector_num]->inUse = 1;
      swap_table[sector_num]->kpage = kpage;
      
      page_size += BLOCK_SECTOR_SIZE;
      sector_num++;

    }
  }

  /* On success, return true */
  return true;

}


/*** Dara drove here ***/

/* Read page from swap space */
bool 
swap_read (block_sector_t sector_num, void *kpage)
{
  /* Get block from swap space */
  b = block_get_role (BLOCK_SWAP);

  /* If null, return unsuccessful */
  if (b == NULL)
    return false;

  /*If cannot find sectory, return unsuccessful */
  if (sector_num == SWAP_SIZE + 1)
    return false;

  else 
  {
    /* Read from block and nullify its swap entry */
    int page_size = 0; 

    while (page_size < PGSIZE) {
      block_read (b, sector_num, (uint32_t) kpage + page_size);
      swap_nullify (sector_num);
      
      page_size += BLOCK_SECTOR_SIZE;
      sector_num++;
    }
  }

	return true;
}

/*** Anja drove here ***/
/* Find a free swap entry */
block_sector_t 
swap_get_free () 
{
	unsigned int i;

	for (i = 0; i < SWAP_SIZE; i++){
		if (swap_table[i]->inUse == 0)
			return i;
	}

  /* If cannot find free entry, return this default to show invalidity */ 
	return SWAP_SIZE + 1;
}

/*** Dara drove here ***/

/* Find sectory with this page */
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

/* Nullify contents of a no longer needed swap space */
void 
swap_nullify (block_sector_t index)
{
	swap_table[index]->inUse = 0;
	swap_table[index]->kpage = NULL;
}
