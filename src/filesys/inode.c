#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

struct lock Lock;

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

  /* Driver: Anja */

  if (pos < inode->data.length)
  {
    /* Checks if the position in the file is in
      the inode data blocks, or one of the other 
      indirect blocks */
    if (pos < DIRECT_STOP)
      return inode->data.sectors[pos / BLOCK_SECTOR_SIZE];

    /* In indirect block */
    if (pos < INDIRECT_BLOCK_STOP)
    {
      /* Write from inode_disk sector to struct */
      struct firstIB *indirect;
      block_read (fs_device, inode->data.indirect, indirect);

      return indirect->sectors[pos / BLOCK_SECTOR_SIZE];
    }

    /* Driver: Anja and Dara */

    else
    {
      /* Write from inode_disk sector to struct */
      struct secondIB *double_indirect;
      block_read (fs_device, inode->data.double_indirect, double_indirect);

      /* Goes through the first level indirect blocks in the second level 
        and allocates sectors */
      int i;
      for (i = 0; i < 4; i++)
      {
        if (double_indirect->level[i] != NULL)
          return double_indirect->level[i]->sectors[pos / BLOCK_SECTOR_SIZE];
        else
          return -1; 
      }
    }
  }

  return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  lock_init (&Lock);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{

  /* Driver: Andrea */

  /* Create disk inode for file */
  struct inode_disk *disk_inode = NULL;
  /* Initialize first level indirect block */
  struct firstIB *indirect = NULL;
  /* Initialize second level indirect block */ 
  struct secondIB *double_indirect = NULL;
  /* Initialize marker of success*/
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      /* Receive number of sectors that need to be allocated */
      size_t sectors = bytes_to_sectors (length);

      /* Establish disk inode attributes*/
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      /* Create zero string filler */
      static char zeros[BLOCK_SECTOR_SIZE];
      
      /* Iteration variable for accessing disk inode sectors */
      int i = 0;
      /* Iteration variable for bookkeeping number of sectors allocated */
      int cnt = 0;

      /* Driver: Andrea and Dara */

      lock_acquire (&Lock);

      /* Iterate to allocate sectors */
      for (; sectors > 0; i++)
      {
        /* Allocate in direct blocks */
        if (cnt < 124)
        {
          /* Allocate one sector at a time */
          free_map_allocate (1, &disk_inode->sectors[i]);
          /* Fill sector with zeros */
          block_write (fs_device, disk_inode->sectors[i], zeros);
        }
        /* Allocate in indirect blocks */
        else if (cnt > 252)
        {
          /* Reset loop counter */
          i = 0;
          indirect = (struct firstIB *) malloc (sizeof (struct firstIB));
          free_map_allocate (1, &indirect->sectors[i]);
          block_write (fs_device, indirect->sectors[i], zeros);
        }

        /* Driver: Anja, Andrea */

        /* Allocate in double indirect blocks */
        else
        {
          /* Reset loop counter */
          i = 0;
          int f = 0;
          int size;
          double_indirect = (struct secondIB *) malloc (sizeof (struct secondIB));
          
          /* Goes through the first level indirect blocks in the second level 
            and allocates sectors */
          for (size = 0; f < 4 && sectors > 0; size++)
          {
            /* If indirect block is filled move one to next one */
            if (size > 127)
              f++;

            double_indirect->level[f] = (struct firstIB *) malloc (sizeof (struct firstIB));
            free_map_allocate (1, &double_indirect->level[f]->sectors[i]);

            block_write (fs_device, double_indirect->level[f]->sectors[i], zeros);
            cnt++;
            i++;
            sectors--;
          }
          //break;
        }

        /* Add to number of sectors allocated */
        cnt++;
        /* Decrease number of sectors to allocate */
        sectors--;
      }

      lock_release (&Lock);

      success = true;

      /* Write disk_inode to inode sector */
      block_write (fs_device, sector, disk_inode);

      /* Driver: Dara */

      /* Frees first and second level and writes back to inode_disk */
      if (indirect != NULL)
      {
        /* Allocate sector for indirect block */
        free_map_allocate (1, &disk_inode->indirect);
        block_write (fs_device, disk_inode->indirect, indirect);
        free (indirect);
      }
      if (double_indirect == NULL)
        double_indirect = (struct secondIB *) malloc (sizeof (struct secondIB));

      /* NULL out second level indirect blocks for comparing */
      int count;
      for (count = 0; count < 4; count++)
        double_indirect->level[count] = NULL;

      /* Allocate sector for double indirect block */
      free_map_allocate (1, &disk_inode->double_indirect);
      block_write (fs_device, disk_inode->double_indirect, double_indirect);
      
      free (double_indirect);
      free (disk_inode);
    }

  return success;
}
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;

  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
      {
        free_map_release (inode->sector, 1);

        /* Driver: Anja */

        lock_acquire (&Lock);

        int i;
        for (i = 0; i < inode->data.length / 512; i++)
        {
          /* Check if i is in direct blocks and free */
          if (i < DIRECT_STOP)
            free_map_release (inode->data.sectors[i],
                                bytes_to_sectors (inode->data.length));

          /* Check if i is in indirect block but not in direct or double indirect */
          if (i > DIRECT_STOP && i < INDIRECT_BLOCK_STOP)
          {
            /* Read indirect block into struct */
            struct firstIB *indirect;
            block_read (fs_device, inode->data.indirect, indirect);

            /* Free sectors in the indirect block */
            free_map_release (indirect->sectors[i],
                                bytes_to_sectors (inode->data.length));
          }

          /* Driver: Andrea and Dara */

          /* Otherwise, i is in the double indirect block */
          if (i > INDIRECT_BLOCK_STOP)
          {
            /* Read double indirect block into struct */
            struct secondIB *double_indirect;
            block_read (fs_device, inode->data.double_indirect, double_indirect);

            /* Goes through the first level indirect blocks in the second level 
              and frees the sectors */
            int f;
            for (f = 0; f < 4; f++)
            {
              if (double_indirect->level[i] != NULL)
              {
                free_map_release (double_indirect->level[f]->sectors[i],
                                  bytes_to_sectors (inode->data.length));
              }
              /* If no other arrays were initialized */
              else
                break; 
            }
          }
        }

        lock_release (&Lock);

      }
      
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {

      /*size_t grow = bytes_to_sectors (size);

      if (grow > 0)
      {

        block_sector_t start;
        static char zeros[BLOCK_SECTOR_SIZE];

        free_map_allocate (grow, &start);

        int i;
        for (i = 0; grow > 0; i++)
        {
          block_write (fs_device, start + i, zeros);
          grow--;
        }

        for (i = 0; i < 124; i++)
          if (inode->data.sectors[i] <= 0)
            block_write (fs_device, inode->data.sectors[i], start + i);
      }*/


      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Flag to indicate if another sector is needed */
      bool flag = false;

      /* Number of bytes to actually write into this sector. */
      int chunk_size;

      /* Driver: Dara and Anja */

      /* If fits in the available space */
      if (size <= min_left)
        chunk_size = size;
      else 
      {
        /* If size if greater than available size but still fits into the current sector */
        if (size > min_left && sector_left >= size)
        {
         /* Grow the size of the file by size */
         inode->data.length += size;
         /* Recompute the sector where the offset is located */
         sector_idx = byte_to_sector (inode, offset);
         chunk_size = size;        
        }
        else
        {
          /* Grow file by rest of sector */
          inode->data.length += sector_left;

          /* Available space in file */
          if ((inode->data.length - offset) <= BLOCK_SECTOR_SIZE)
            chunk_size = inode->data.length - offset;
          else
            chunk_size = sector_left;
          /* Recompute sector where offset is located */
          sector_idx = byte_to_sector (inode, offset);

          /* Set flag to true, need to allocate new sector */
          flag = true;
        }
      }

      if (chunk_size <= 0) {
        break;
      }

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;

      /* Driver: Andrea and Anja */

      if (flag)
      {
        lock_acquire (&Lock);

        /* Compute how many sectors are needed for the rest of size */
        int sector_cnt = size / 512;
        int rem = size % 512;
        if (rem != 0)
          sector_cnt += 1;

        /* Create new sector to hold allocated sector */
        block_sector_t new_inode_sector;
        
        /* Allocate sector */
        free_map_allocate (1, &new_inode_sector);

        lock_release (&Lock);
        
        /* Install sector(s) into inode_disk */
        inode_create(new_inode_sector, sector_cnt);
        inode->data.length += size;
        
        /* Reopen file */
        inode_open(inode->sector);

        /* Set index to allocated sector */
        sector_idx = new_inode_sector;
      }
    }
  
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
