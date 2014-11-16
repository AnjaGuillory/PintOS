#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"
#include "devices/block.h"
#include "filesys/off_t.h"

/* Where is the Page stored */
enum WherePage
  {
    PAGE_FILESYS,                   /* In the file system. */
    PAGE_SWAP,                      /* In Swap. */
  };

struct page
{
    struct hash_elem hash_elem;     /* Hash table element. */
    void * upage;                   /* User address. */
    void * kpage;                   /* Kernel addresss */
    enum WherePage page;            /* Where is the infor for the page stored */
    block_sector_t whereSwap;       /* Where in swap is it located */
    bool isStack;                   /* Is it a stack page */
    /* ...other members... */
    bool isZero;                    /* Should the page be zeroed */
    uint32_t read_bytes;            /* How many bytes need to be read */
    uint32_t zero_bytes;            /* Hown many bytes zeroed */
    struct file *file;              /* Which file, if any */
    off_t ofs;                      /* Offset in the file */
    bool writable;
};

void supplemental_init(void);   /* Initialize supplemental page table */
bool page_insert (void *, void *);  /* Insert page into supplemental page table */
bool page_update (struct page *, void *);   /* Update contents of page entry */
bool page_delete (void *);  /* Delete page entry */
struct page * page_lookup (void *, bool, struct thread *);   /* Look up a page entry */
unsigned page_hash (const struct hash_elem *, void *);  /* Hash a new page entry */
bool page_load (struct page *, void *);    /* Load page */
void page_destroy (void);   /* Destroy supplemental page table */
bool page_less (const struct hash_elem *, const struct hash_elem *,
           void *); /* Check if element precedes another element */


#endif /* vm/page.h */
