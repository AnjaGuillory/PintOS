#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "lib/kernel/list.h"

#define STOP 130
#define DIRECT_STOP 63488
#define INDIRECT_BLOCK_STOP 129024

/* Driver: Andrea and Dara */

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    
    block_sector_t sectors[124];        /* Disk location, direct sectors */  
    block_sector_t indirect;            /* Sector for indirect block */
    block_sector_t double_indirect;     /* Sector for double indirect blocks */
  };

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    int flag_bool_dir;                  /* Indicates if directory inode */
  };

/* Driver: Anja */

/* Struct that represents first level indirect block */
struct firstIB
  {
    block_sector_t sectors[128];         /* Disk location */         
  };

/* Struct that represents second level indirect block */
struct secondIB
  {
    struct firstIB *level[4];           /* Array of first level indirect blocks */
    uint32_t unused[124];               /* Not used */
  };


struct bitmap;

void inode_init (void);
bool inode_create (block_sector_t, off_t);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

#endif /* filesys/inode.h */
