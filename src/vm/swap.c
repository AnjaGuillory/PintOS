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

/* Swap table for the swap partition */
static struct swap_frame *swap_table[MAXARGS];

/* Initialize swap table with empty swap blocks */
bool swap_init ()
{
  int i;
  for(i = 0; i < MAXARGS; i++)
  {
    struct swap_frame *f;
    swap_table[i] = swap_nullify(f);
  }
}

/* nullify the components of the swap frame */
bool swap_nullify(struct swap_frame *f)
{
  f->inUse = 0; /* Not in use */
  f->upage = -1; /* No address set */
  process_status = -1; /* Process not terminated nor created */
}
