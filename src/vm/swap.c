#include <hash.h>
#include <string.h>
#include <syscall-nr.h>
#include <string.h>
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
#include "devices/block.h"

/* Swap table for the swap partition */
struct block * b = block_get_role (BLOCK_SWAP);
static swap_frame *swap_table[b->size];

// Call block get role with BLOCK_SWAP once
// whenever a page is evicted from frame,
// get an open sector (freelist) (keep our own mapping of this)
// call block_write to that sector, pass in page pointer
// when a process is terminated, then add the sector to the freelist
// when we want to put the page back in a frame, use block_read


/* Initialize swap table with empty swap blocks */
void swap_init ()
{
  int i;
  for(i = 0; i < b->size; i++)
  	swap_nullify (i);
}

bool swap_write (void *kpage){

	if (b == null)
		return false;

	block_sector_t sector_num = swap_get_free ();

	if (sector_num == -1)
		PANIC ("No free sectors!");

	else
	{
		block_write (b, sector_num, kpage);
		swap_table[sector_num]->inUse = 1;
		swap_table[sector_num]->kpage = kpage;
	}

	return true;

}

bool swap_read (void *kpage){

	block_sector_t sector_num = swap_find_sector (kpage);

	if (sector_num == -1)
		return false;

	else 
	{
		block_read (b, sector_num, kpage);
		swap_nullify (sector_num);
	}

	return true;
}

block_sector_t swap_get_free () {
	block_sector_t i;

	for (i = 0; i < b->size; i++){
		if (swap_table[i]->inUse == 0)
			return i;
	}

	return -1;
}
block_sector_t swap_find_sector (void * kpage){
	int i;
	for (i = 0; i < b->size; i++){
		if (swap_table[i]->kpage == kpage)
			return i;
	}

	return -1;
}

void swap_nullify (int index)
{
	swap_table[index]->inUse = 0;
	swap_table[index]->kpage = NULL;
}
