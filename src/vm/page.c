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
  struct thread *cur = thread_current();
  struct page *p = (struct page *) malloc (sizeof (struct page));

  p->upage = upage;
  p->kpage = kpage;

  if (hash_insert (&(cur->page_table), &p->hash_elem) == NULL)
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

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct page *
page_lookup (void * addr, bool kpage)
{
  struct page *p = (struct page *) malloc (sizeof (struct page *));
  struct hash_elem *e;

  if(kpage) 
     /* For kpage */
    p->kpage = addr;
  else
    /* For upage */
    p->upage = addr;

  e = hash_find (&(thread_current()->page_table), &p->hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}
