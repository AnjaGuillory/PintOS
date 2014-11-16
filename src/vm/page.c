#include "userprog/syscall.h"
#include <stdio.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdlib.h>
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
#include "userprog/pagedir.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "lib/kernel/hash.h"

struct lock Lock;

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->upage, sizeof p->upage);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->upage < b->upage;
}

/* Initialize hash table as supplemental page table */
void supplemental_init() {
  lock_init(&Lock);
  hash_init (&(thread_current()->page_table), page_hash, page_less, NULL);
}

/* Insert new page/mapping into supplemental page table */
bool page_insert (void *upage, void * kpage)
{
  struct page *p;

  /* If page was already inserted, then update with new mapping */
  if ((p = page_lookup (upage, false, thread_current())) != NULL)
  {
    page_update (p, kpage);
    return true;
  }

  /* Allocate space for new page */
  p = (struct page *) malloc (sizeof (struct page));

  /* Update contents */
  p->upage = upage;
  p->kpage = kpage;
  p->isStack = 0;

  /* Insert into hash table (supplemental page table) */
  if (hash_insert (&(thread_current()->page_table), &p->hash_elem) == NULL)
    return true;

  return false;
}

/* Update supplemental page table */
bool page_update (struct page *p, void *kpage) {
  
  p->kpage = kpage;

  if (hash_replace (&(thread_current()->page_table), &p->hash_elem) != NULL)
    return true;

  return false;
}

/* Delete a page currently no longer needed */
bool page_delete (void * kpage)
{
  struct page *p = page_lookup (kpage, true, thread_current());

  if (hash_delete (&(thread_current()->page_table), &p->hash_elem) == NULL)
    return false;

  return true;
}

/* Load page into memory */
bool
page_load (struct page *p, void *kpage)
{

  /* If read in from a file */
  if (p->page == PAGE_FILESYS) {

    /* Zero the whole page if isZero is true */
    if (p->isZero == true) {
      memset (kpage + p->zero_bytes, 0, PGSIZE);

    }
    else {

      /* Read from seeked file */
      if (p->read_bytes > 0 || p->zero_bytes > 0) {
        file_seek (p->file, p->ofs);

        /* Load this page. */
        if (file_read (p->file, kpage, p->read_bytes) != (int) p->read_bytes)
          {
            /* Free if invalid */
            palloc_free_page (kpage);
            return false; 
          }

        /* Read in bytes into page */
        memset (kpage + p->read_bytes, 0, p->zero_bytes);
      }
    }
  }

  /* Read from swap if it is a swapped page */
  else if (p->page == PAGE_SWAP) {
    bool readingIn = swap_read (p->whereSwap, kpage);
    if (readingIn == false)
      return false;
  }
  return true;
}

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct page *
page_lookup (void * addr, bool kpage, struct thread *t)
{
  struct hash_iterator i;

  /* Iterate throgh hash table to look for page at addr */
  hash_first (&i, &(t->page_table));

  while (hash_next (&i))
  {
    struct page *p = hash_entry (hash_cur (&i), struct page, hash_elem);
    
    if (kpage) {
      if (p->kpage == addr)
        return p;
    }
    else {
      if (p->upage == addr)
        return p;
    }
  }
  return NULL;
}

void 
page_destroy () 
{
  /* Destroy hash table upon termination */
  hash_destroy (&(thread_current()->page_table), &(page_hash));
}
