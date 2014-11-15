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

void supplemental_init() {
  hash_init (&(thread_current()->page_table), page_hash, page_less, NULL);
}

bool page_insert (void *upage, void * kpage)
{
  struct page *p;

  if ((p = page_lookup (upage, false)) != NULL)
  {
    page_update (p, kpage);
    return true;
  }

  p = (struct page *) malloc (sizeof (struct page));

  p->upage = upage;
  p->kpage = kpage;
  p->isStack = 0;

  if (hash_insert (&(thread_current()->page_table), &p->hash_elem) == NULL)
    return true;

  return false;
}

bool page_update (struct page *p, void *kpage) {
  
  p->kpage = kpage;

  if (hash_replace (&(thread_current()->page_table), &p->hash_elem) != NULL)
    return true;

  return false;
}

bool page_delete (void * kpage)
{
  struct page *p = page_lookup (kpage, true);

  if (hash_delete (&(thread_current()->page_table), &p->hash_elem) == NULL)
    return false;

  return true;
}

bool
page_load (struct page *p, void *kpage)
{
  if (p->page == PAGE_FILESYS) {
    if (p->isZero == true) {
      memset (kpage + p->zero_bytes, 0, PGSIZE);

     // p->zero_bytes -= PGSIZE;
    }
    else {
      if (p->read_bytes > 0 || p->zero_bytes > 0) {
        file_seek (p->file, p->ofs);

        /* Load this page. */
        if (file_read (p->file, kpage, p->read_bytes) != (int) p->read_bytes)
          {
            palloc_free_page (kpage);
            return false; 
          }
          //printf("p->zerobytes %p\n", kpage);
        memset (kpage + p->read_bytes, 0, p->zero_bytes);
      }
    }
  }
  else if (p->page == PAGE_SWAP) {
    //printf("We see that the page is a swapped page\n");
    bool readingIn = swap_read (p->whereSwap, kpage);
    if (readingIn == false)
      return false;
    //printf("Reading in a swapped page\n");
  }
  return true;
}

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct page *
page_lookup (void * addr, bool kpage)
{
  struct hash_iterator i;

  hash_first (&i, &(thread_current()->page_table));
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
  hash_destroy (&(thread_current()->page_table), &(page_hash));
}

// void 
// page_find () {
//   struct page *p = (struct page *) malloc (sizeof (struct page *));
//   struct hash_elem *e;

//   if(kpage) 
//      /* For kpage */
//     p->kpage = addr;
//   else
//     /* For upage */
//     p->upage = addr;

//   e = hash_find (&(thread_current()->page_table), &p->hash_elem);
//   return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
// }
