#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"

struct page
{
    struct hash_elem hash_elem;     /* Hash table element. */
    void * upage;                   /* User address. */
    void * kpage;                   /* Kernel addresss */
    bool swappedIn;                 /* Indicate if in swap space */
    void * whereSwap;               /* Where in swap is it located */
    char * infoPage;                /* Where is the infor for the page stored */
    int indexFrame;                 /* Which frame is it using */
    bool isStack;                   /* Is it a stack page */
    /* ...other members... */
};

void supplemental_init(void);
bool page_insert (void *, void *);
bool page_delete (void *);
struct page * page_lookup (void *, bool);
unsigned page_hash (const struct hash_elem *, void *);
bool page_less (const struct hash_elem *, const struct hash_elem *,
           void *);


#endif /* vm/page.h */
