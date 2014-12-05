#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define STOP 130

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
// struct inode_disk
//   {
//     //block_sector_t start;               /* First data sector. */
//     off_t length;                       /* File size in bytes. */
//     unsigned magic;                     /* Magic number. */
    
//     block_sector_t sectors[124];
//     block_sector_t indirect;
//     block_sector_t double_indirect;
//     //uint32_t unused[124];               /* Not used. */
//     //block_sector_t sectors[126];               /* Not used. */
//   };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

struct firstIB
  {
    block_sector_t sectors[128];
    //struct inode_disk *data[21];
    //struct inode_disk data;             /* Inode content */
    //block_sector_t sectors[21];              /* Disk location */
  };

struct secondIB
  {
    struct firstIB *level[4];           /* Array of first level indirect blocks */
    uint32_t unused[124];
    //block_sector_t sector;              /* Disk location, don't need? */
  };

// /* In-memory inode. */
// struct inode 
//   {
//     struct list_elem elem;              /* Element in inode list. */
//     block_sector_t sector;              /* Sector number of disk location. */
//     int open_cnt;                        Number of openers. 
//     bool removed;                       /* True if deleted, false otherwise. */
//     int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
//     struct inode_disk data;             /* Inode content. */
//   };


// /* Returns the total length of the file, 
//   takes into account the indirect blocks */
// static off_t
// total_length (const struct inode *inode)
// {
//   /* Checks if indirect block was not needed */

//   struct firstIB indirect;
//   block_read (fs_device, indirect, &inode->data.indirect);

//   if (indirect == NULL) {
//     //printf("total length %d\n", inode->data.length);
//     return inode->data.length;
//   }

//   /* Keep track of current sum */
//   off_t sum = 0;
//   sum += (inode->data.length + inode->firstLevel->data.length);

//   int i;
//   for (i = 0; i < 4; i++)
//   {
//     /* Checks if another indirect block was needed */
//     if (inode->secondLevel->level[i] == NULL)
//       return sum;
//     sum += inode->secondLevel->level[i]->data.length;
//   }

//   return sum;
// }

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

  //off_t length = 0;

  if (pos < inode->data.length)
  {
    /* Checks if the position in the file is in
      the inode data blocks, or one of the other 
      indirect blocks */
    if (pos < 63488)
    {
      //printf("what is returned in byte_to_sector %d\n", inode->data.sectors[pos / BLOCK_SECTOR_SIZE]);
      //printf("inode->data.sectors 0 %d pos is %d\n", inode->data.sectors[0], pos);
      return inode->data.sectors[pos / BLOCK_SECTOR_SIZE];
    }

    /* If "pos" was not in the inode then add to length and compare */
    //length += inode->data.length + inode->firstLevel->data.length;

    /* In firstLevel indirect block */
    if (pos < 129024)
    {
      struct firstIB *indirect;
      block_read (fs_device, inode->data.indirect, indirect);

      return indirect->sectors[pos / BLOCK_SECTOR_SIZE];
    }
    else
    {
      /* In secondLevel indirect block */
      /*for (i = 0; i < 4; i++) {
        if (inode->secondLevel->level[i] != NULL) {
          length += inode->secondLevel->level[i]->data.length;
           
          if (pos < length)
            return inode->secondLevel->level[i]->data.start + pos / BLOCK_SECTOR_SIZE;
        }
        else
          return -1;
      }*/

      int i;
      struct secondIB *double_indirect;
      block_read (fs_device, inode->data.double_indirect, double_indirect);

      /* Goes through the first level indirect blocks in the second level 
        and allocates sectors */
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
}

void
initialize_array (block_sector_t arr[], int length)
{
  int i;
  for (i = 0; i < length; i++)
    arr[i] = STOP;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  //printf("in inode create with sector: %d %d\n", sector, length);
  //debug_backtrace();
  struct inode_disk *disk_inode = NULL;
  struct firstIB *indirect = NULL;
  struct secondIB *double_indirect = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      //printf("sectors need in inode create %d\n", sectors);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      static char zeros[BLOCK_SECTOR_SIZE];
      
      int i = 0;
      int cnt = 0;
      for (; sectors > 0; i++)
      {
        /* Allocate in direct blocks */
        if (cnt < 124)
        {
          //printf("in first if in create\n");
          free_map_allocate (1, &(disk_inode->sectors[i]));
          //printf("dies inode secotr %d i is %d\n", disk_inode->sectors[i], i);
          block_write (fs_device, disk_inode->sectors[i], zeros);
        }
        /* Allocate in indirect blocks */
        else if (cnt > 252)
        {
          i = 0;
          indirect = (struct firstIB *) malloc (sizeof (struct firstIB));
          free_map_allocate (1, &indirect->sectors[i]);
          block_write (fs_device, indirect->sectors[i], zeros);
        }
        /* Allocate in double indirect blocks */
        else
        {
          i = 0;
          int f = 0;
          int size;
          double_indirect = (struct secondIB *) malloc (sizeof (struct secondIB));
          /* Goes through the first level indirect blocks in the second level 
            and allocates sectors */
          for (size = 0; f < 4 && sectors > 0; size++)
          {
            if (size > 127)
              f++;

            double_indirect->level[f] = (struct firstIB *) malloc (sizeof (struct firstIB));
            free_map_allocate (1, &double_indirect->level[f]->sectors[i]);
            block_write (fs_device, double_indirect->level[f]->sectors[i], zeros);
            i++;
            sectors--;
          }
        }

        if (sectors <= 0)
          break;
        
        cnt++;
        sectors--;
      }
      success = true;

      //printf("im going to write the disk inode\n");
      block_write (fs_device, sector, disk_inode);
      //printf("sector in create %d\n", sector);

      /* Frees first and second level and writes back to inode_disk */
      if (indirect != NULL)
      {
        free_map_allocate (1, &disk_inode->indirect);
        block_write (fs_device, disk_inode->indirect, indirect);
        free (indirect);
      }
      if (double_indirect == NULL)
        double_indirect = (struct secondIB *) malloc (sizeof (struct secondIB));

      int count;
      for (count = 0; count < 4; count++)
        double_indirect->level[count] = NULL;

      free_map_allocate (1, &disk_inode->double_indirect);
      block_write (fs_device, disk_inode->double_indirect, double_indirect);
      
      free (double_indirect);
      //printf("length in create %d sector[0] %d\n", disk_inode->length, disk_inode->sectors[0]);
      free (disk_inode);
      
      //printf("length in create: %d\n\n\n\n\n\n\n", disk_inode->length);
    }

  return success;
}
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{

  //printf("in inode open with sector: %d \n", sector);

  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
          //printf("inode sector: %d\n", inode->sector);
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
  //inode->firstLevel = NULL;
  //printf("inode open sector %d\n", inode->sector);
  block_read (fs_device, inode->sector, &inode->data);

  printf("sector start open %d\n", inode->data.sectors[0]);
  //printf("length in open %d\n", inode->data.length);

  //printf("secotr start %d\n", inode->data.start);

    //printf("sector is 1, length = %d\n", inode->data.length);

  // size_t original = inode->data.length;

  // printf("inode original_length %d\n", original);
  // if (original > 100000)
  //   debug_backtrace();

  // int i;
  // for (i = 0; original > 63488; i++)
  // {
  //   /* Checks if first level indirect block has not been allocated */

  //   struct firstIB indirect;

  //   block_read (fs_device, indirect, &inode->data.indirect);
    
  //   if (indirect == NULL)
  //   {
  //     inode->firstLevel = (struct firstIB *) malloc (sizeof (struct firstIB));
  //     inode->firstLevel->sector = (inode->sector) + 11;
      
  //     /* Point firstLevel data blocks to zeroed disk block */
  //     inode->firstLevel->data = inode->data;
  //     inode->firstLevel->data.start = inode->firstLevel->sector;
      
  //     /* Change the length of original inode to reflect the other half
  //       being in the indirect block */
  //     inode->data.length = (BLOCK_SECTOR_SIZE*10);
  //     original -= (BLOCK_SECTOR_SIZE*10);

  //     /* Checks if file fills out the whole indirect block */
  //     if (original > (BLOCK_SECTOR_SIZE*1024))
  //     {
  //       inode->firstLevel->data.length = (BLOCK_SECTOR_SIZE*1024);
  //       original -= (BLOCK_SECTOR_SIZE*1024);
  //     }
  //     else {
  //       inode->firstLevel->data.length = original;
  //       original = 0;
  //     }
      
  //      Allocate secondLevel indirect in case it is needed 
  //     inode->secondLevel = (struct secondIB *) malloc (sizeof (struct secondIB));
  //     inode->secondLevel->level[0] = NULL;
  //     i--;
  //   }
  //   else
  //   {
  //     inode->secondLevel->level[i] = (struct firstIB *) malloc (sizeof (struct firstIB));

  //     /* Which sector wa the last to be determined */
  //     if (i == 0)
  //       inode->secondLevel->level[i]->sector = (inode->firstLevel->sector) + 1025;
  //     else
  //       inode->secondLevel->level[i]->sector = (inode->secondLevel->level[i-1]->sector) + 1025;

  //     /* Point indirect block to disk */
  //     inode->secondLevel->level[i]->data = inode->data;
  //     inode->secondLevel->level[i]->data.start = inode->secondLevel->level[i]->sector;

  //     /* Checks if file fills out the whole (current) indirect block */
  //     if (original > (BLOCK_SECTOR_SIZE*1024))
  //     {
  //       inode->secondLevel->level[i]->data.length = (BLOCK_SECTOR_SIZE*1024);
  //       original -= (BLOCK_SECTOR_SIZE*1024);
  //     }
  //     else
  //     {
  //       inode->secondLevel->level[i]->data.length = original;
  //       original = 0;
  //     } 

  //     /* Make sure the next indirect block is not allocated */
  //     if (i != 4)
  //       inode->secondLevel->level[i+1] = NULL;
  //   }
  // }
  
  //printf("inode length %d\n", inode->data.length);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{

  //printf("in inode reopen\n");
  if (inode != NULL)
    inode->open_cnt++;


  // size_t original = inode->data.length;

  // int i;
  // for (i = 0; original > 0; i++)
  // {
  //   /* Checks if first level indirect block has not been allocated */
  //   if (original < 5120)
  //   {
  //     // char zeros[original-inode->original_length];
  //     // block_write (fs_device, inode->sector, zeros);
  //     original = 0;

  //     //block_read (fs_device, inode->sector, &inode->data);
  //   }


  //   else if (inode->firstLevel == NULL)
  //   {
  //     inode->firstLevel = (struct firstIB *) malloc (sizeof (struct firstIB));
  //     inode->firstLevel->sector = (inode->sector) + 11;
      
  //     /* Point firstLevel data blocks to zeroed disk block */
  //     inode->firstLevel->data = inode->data;
  //     inode->firstLevel->data.start = inode->firstLevel->sector;
      
  //     /* Change the length of original inode to reflect the other half
  //       being in the indirect block */
  //     inode->data.length = (BLOCK_SECTOR_SIZE*10);
  //     original -= (BLOCK_SECTOR_SIZE*10);

  //     /* Checks if file fills out the whole indirect block */
  //     if (original > (BLOCK_SECTOR_SIZE*1024))
  //     {
  //       inode->firstLevel->data.length = (BLOCK_SECTOR_SIZE*1024);
  //       original -= (BLOCK_SECTOR_SIZE*1024);
  //     }
  //     else {
  //       inode->firstLevel->data.length = original;
  //       original = 0;
  //     }
      
  //     /* Allocate secondLevel indirect in case it is needed */
  //     inode->secondLevel = (struct secondIB *) malloc (sizeof (struct secondIB));
  //     inode->secondLevel->level[0] = NULL;
  //     i--;
  //   }
  //   else
  //   {
  //     inode->secondLevel->level[i] = (struct firstIB *) malloc (sizeof (struct firstIB));

  //     /* Which sector wa the last to be determined */
  //     if (i == 0)
  //       inode->secondLevel->level[i]->sector = (inode->firstLevel->sector) + 1025;
  //     else
  //       inode->secondLevel->level[i]->sector = (inode->secondLevel->level[i-1]->sector) + 1025;

  //     /* Point indirect block to disk */
  //     inode->secondLevel->level[i]->data = inode->data;
  //     inode->secondLevel->level[i]->data.start = inode->secondLevel->level[i]->sector;

  //     /* Checks if file fills out the whole (current) indirect block */
  //     if (original > (BLOCK_SECTOR_SIZE*1024))
  //     {
  //       inode->secondLevel->level[i]->data.length = (BLOCK_SECTOR_SIZE*1024);
  //       original -= (BLOCK_SECTOR_SIZE*1024);
  //     }
  //     else
  //     {
  //       inode->secondLevel->level[i]->data.length = original;
  //       original = 0;
  //     } 

  //     /* Make sure the next indirect block is not allocated */
  //     if (i != 4)
  //       inode->secondLevel->level[i+1] = NULL;
  //   }
  // }
  
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
  //printf("I got to inode close~!\n");
  //printf("in inode_close() with length %d\n", total_length(inode));
  // inode->data.length = 2134;
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /*uint8_t bounce = NULL;
  block_sector_t i = inode->data.start;
  for (; i < inode->data.length; i++){
    block_write (fs_device, i, bounce);
  }*/
  //block_write (fs_device, inode->sector, inode);

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
      {
        free_map_release (inode->sector, 1);

        int i;
        for (i = 0; i < inode->data.length / 512; i++)
        {
          if (i < 63488)
            free_map_release (inode->data.sectors[i],
                                bytes_to_sectors (inode->data.length));

          if (i > 63488 && i < 129024)
          {

            struct firstIB *indirect;
            block_read (fs_device, inode->data.indirect, indirect);

            free_map_release (indirect->sectors[i],
                                bytes_to_sectors (inode->data.length));
          }
          if (i > 129024)
          {
            //int i;
            struct secondIB *double_indirect;
            block_read (fs_device, inode->data.double_indirect, double_indirect);

            /* Goes through the first level indirect blocks in the second level 
              and allocates sectors */
            int f;
            for (f = 0; f < 4; f++)
            {
              if (double_indirect->level[i] != NULL)
              {
                free_map_release (double_indirect->level[f]->sectors[i],
                                  bytes_to_sectors (inode->data.length));
              }
              else
                break; 
            }
          }
        }
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
  //debug_backtrace();
  //printf("inode write at start sector is %d offset is %d size is %d and length is %d\n", inode->data.sectors[0], offset, size, inode->data.length);
  //printf("offset %d\n", offset);
  //printf("start sector in write %d\n", inode->data.sectors[0]);
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {

      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      //printf("sector_left %d inode_left %d\n", sector_left, inode_left);
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      //printf("size: %d min_left: %d sector_left: %d offset: %d inode length: %d\n", size, min_left, sector_left, offset, inode->data.length);
      bool flag = false;
      /* Number of bytes to actually write into this sector. */

      // printf ("trying to write with size %d & minleft = %d\n", size, min_left);
      int chunk_size;
      if (size <= min_left){
        chunk_size = size;
         //printf("min left %d\n", min_left);
      }
      else {
        if (size > min_left && sector_left >= size)
        {
          //printf("in else in if %d\n", flag);
         inode->data.length += size;
         sector_idx = byte_to_sector (inode, offset);
         //printf("data and sector %d %d\n", inode->data.length, sector_idx);
         chunk_size = size;        
        }
        else
        {

          inode->data.length += sector_left;
          // printf("inode length: %d offset %d\n", inode_length(inode), offset);
          if ((inode->data.length - offset) <= 512)
            chunk_size = inode->data.length - offset;
          else
            chunk_size = sector_left;
          sector_idx = byte_to_sector (inode, offset);
          // printf("chunk_size: %d\n", chunk_size);
          flag = true;
        }
      }

      // int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0) {
        //// printf("hey\n\n\n");
        break;
      }

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          //printf("im writing 512 to sector %d\n", sector_idx);
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
             // printf("sector_ofs: %d\n", sector_ofs);
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
          // printf("after block write in the else\n");
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;

      //printf("size and offset %d %d\n", size, offset);

      if (flag)
      {
           //printf("size in flag: %d\n", size);
          int sector_cnt = size / 512;
          int rem = size % 512;
          if (rem != 0)
            sector_cnt += 1;

          // printf("sector count in flag: %d\n", sector_cnt);
          block_sector_t new_inode_sector;
          free_map_allocate (1, &new_inode_sector);
          inode_create(new_inode_sector, sector_cnt);


          inode->data.length += size;
          inode_open(inode->sector);
          sector_idx = byte_to_sector (inode, offset);
          // const void *new_buffer = (void *) malloc (size);
          //memcpy (new_buffer, buffer, 6);

          // printf("after open\n");
          //inode_write_at(inode, buffer, size, offset);
          // printf("sector_idx after %d\n", sector_idx);
          //size -= chunk_size;
          //offset += chunk_size;
          //bytes_written += chunk_size;
      }
      //printf("sector index in inode_write_at() %d\n", sector_idx);
      
      // size -= chunk_size;
      
      
      //printf("size before loop exit: %d\n", size);

    }
  free (bounce);

  printf("file size %d\n", inode->data.length);
  // printf("bytes written %d\n", bytes_written);
  //printf("returning\n");
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

/*struct inode *
next_Block (struct inode *inode)
{
  if (inode->firstLevel == NULL)
  {
    inode->firstLevel = (struct firstIB *) malloc (sizeof (struct firstIB));
    
    inode->secondLevel = (struct secondIB *) malloc (sizeof (struct secondIB));
    inode->secondLevel->level[0] = NULL;
    
    return inode->firstLevel;
  }
  else
  {
    int i;
    for (i = 0; i < 4; i++)
    {
      if (inode->secondLevel->level[i] == NULL) {
        inode->secondLevel->level[i] = (struct firstIB *) malloc (sizeof (struct firstIB));
        
        if (i != 3)
          inode->secondLevel->level[i] = NULL;

        return inode->secondLevel->level[i];
      }
    }
  }

  return NULL;
}*/
