#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
/* NEW */
#include "filesys/cache.h"
/* /NEW */

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* NEW */
static uint8_t empty_sector[BLOCK_SECTOR_SIZE];
/* /NEW */

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors(off_t size)
{
  return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector(const struct inode *inode, off_t pos)
{
  ASSERT(inode != NULL);
  if (pos < inode->data.length)
  {
    /* NEW */
    size_t sectors = bytes_to_sectors(pos);
    off_t index = pos / BLOCK_SECTOR_SIZE;
    if (index < DIRECT_BLOCK_NUM)
      return inode->data.direct[index];

    sectors -= DIRECT_BLOCK_NUM;
    block_sector_t sector;
    if (sectors <= INDIRECT_BLOCK_NUM * BLOCK_POINTER_NUM)
    {
      block_sector_t *indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
      if (!indirect_block)
        return -1;

      index = sectors / BLOCK_POINTER_NUM;
      off_t offset = ((pos - DIRECT_BLOCK_NUM * BLOCK_SECTOR_SIZE) / BLOCK_SECTOR_SIZE) % BLOCK_POINTER_NUM;
      cache_read(inode->data.indirect[index], indirect_block);
      sector = indirect_block[offset];
      free(indirect_block);

      return sector;
    }
    /* /NEW */
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init(void)
{
  list_init(&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
/* NEW */
bool inode_create(block_sector_t sector, off_t length, bool is_dir)
/* /NEW */
{
  struct inode_disk *disk_inode = NULL;
  /* NEW */
  bool success = true;
  /* /NEW */

  ASSERT(length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc(1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    /* NEW */
    disk_inode->is_dir = is_dir;
    success = inode_disk_init(disk_inode, length);
  }

  if (success)
    cache_write(sector, disk_inode);

  free(disk_inode);
  /* /NEW */
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
       e = list_next(e))
  {
    inode = list_entry(e, struct inode, elem);
    if (inode->sector == sector)
    {
      inode_reopen(inode);
      return inode;
    }
  }

  /* Allocate memory. */
  inode = malloc(sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front(&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  /* NEW */
  cache_read(inode->sector, &inode->data);
  /* /NEW */
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    /* Remove from inode list and release lock. */
    list_remove(&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      /* NEW */
      size_t sectors = bytes_to_sectors(inode->data.length);
      size_t blocks = sectors < DIRECT_BLOCK_NUM ? sectors : DIRECT_BLOCK_NUM;
      for (int i = 0; i < blocks; ++i)
        inode_block_clear(&inode->data.direct[i], 1, 0);

      if (sectors <= DIRECT_BLOCK_NUM)
        goto done;

      sectors -= DIRECT_BLOCK_NUM;
      for (int i = 0; i < INDIRECT_BLOCK_NUM; ++i)
      {
        blocks = sectors < BLOCK_POINTER_NUM ? sectors : BLOCK_POINTER_NUM;
        inode_block_clear(&inode->data.indirect[i], blocks, 1);
        if (sectors <= BLOCK_POINTER_NUM)
          goto done;

        sectors -= BLOCK_POINTER_NUM;
      }

    done:
      /* /NEW */
      free_map_release(inode->sector, 1);
    }

    free(inode);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void inode_remove(struct inode *inode)
{
  ASSERT(inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
  {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      /* Read full sector directly into caller's buffer. */
      /* NEW */
      cache_read(sector_idx, buffer + bytes_read);
      /* /NEW */
    }
    else
    {
      /* Read sector into bounce buffer, then partially copy
         into caller's buffer. */
      if (bounce == NULL)
      {
        bounce = malloc(BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
          break;
      }
      /* NEW */
      cache_read(sector_idx, bounce);
      /* /NEW */
      memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }
  free(bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size,
                     off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  /* NEW */
  off_t new_length = offset + size;
  if (new_length - 1 > inode->data.length)
  {
    if (!inode_disk_init(&inode->data, new_length))
      return 0;

    inode->data.length = new_length;
    cache_write(inode->sector, &inode->data);
  }
  /* /NEW */

  while (size > 0)
  {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      /* Write full sector directly to disk. */
      /* NEW */
      cache_write(sector_idx, buffer + bytes_written);
      /* /NEW */
    }
    else
    {
      /* We need a bounce buffer. */
      if (bounce == NULL)
      {
        bounce = malloc(BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
          break;
      }

      /* If the sector contains data before or after the chunk
         we're writing, then we need to read in the sector
         first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
        /* NEW */
        cache_read(sector_idx, bounce);
      /* /NEW */
      else
        memset(bounce, 0, BLOCK_SECTOR_SIZE);
      memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
      /* NEW */
      cache_write(sector_idx, bounce);
      /* /NEW */
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }
  free(bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void inode_deny_write(struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode *inode)
{
  ASSERT(inode->deny_write_cnt > 0);
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode)
{
  return inode->data.length;
}

/* NEW */
/* Initialize inode_disk DATA. */
bool inode_disk_init(struct inode_disk *data, size_t size)
{
  if (size > 0x800000)
    return false;

  size_t sectors = bytes_to_sectors(size);
  size_t blocks = sectors < DIRECT_BLOCK_NUM ? sectors : DIRECT_BLOCK_NUM;
  for (int i = 0; i < blocks; ++i)
    if (!inode_block_init(&data->direct[i], 1, 0))
      return false;

  if (sectors <= DIRECT_BLOCK_NUM)
    return true;

  sectors -= DIRECT_BLOCK_NUM;
  for (int i = 0; i < INDIRECT_BLOCK_NUM; ++i)
  {
    blocks = sectors < BLOCK_POINTER_NUM ? sectors : BLOCK_POINTER_NUM;
    if (!inode_block_init(&data->indirect[i], blocks, 1))
      return false;

    if (sectors <= BLOCK_POINTER_NUM)
      return true;

    sectors -= BLOCK_POINTER_NUM;
  }

  return true;
}

/* Initialize data block BLOCK_SECTOR. */
bool inode_block_init(block_sector_t *block_sector, size_t size, int level)
{
  ASSERT(block_sector);

  block_sector_t indirect_block[BLOCK_SECTOR_SIZE];
  switch (level)
  {
  case 0:
    /* Direct block. */
    if (!*block_sector)
    {
      if (!free_map_allocate(1, block_sector))
        return false;

      cache_write(*block_sector, empty_sector);
    }

    return true;

  case 1:
    /* Indirect block. */
    if (!*block_sector)
    {
      if (!free_map_allocate(1, block_sector))
        return false;

      cache_write(*block_sector, empty_sector);
    }

    cache_read(*block_sector, indirect_block);
    for (int i = 0; i < size; ++i)
    {
      if (!indirect_block[i])
      {
        if (!free_map_allocate(1, &indirect_block[i]))
          return false;

        cache_write(indirect_block[i], empty_sector);
      }
    }

    cache_write(*block_sector, indirect_block);
    return true;

  default:
    return false;
  }
}

/* Free data block BLOCK_SECTOR. */
void inode_block_clear(block_sector_t *block_sector, size_t size, int level)
{
  ASSERT(block_sector);

  block_sector_t indirect_block[BLOCK_SECTOR_SIZE];
  switch (level)
  {
  case 0:
    /* Direct block. */
    break;

  case 1:
    /* Indirect block. */
    cache_read(*block_sector, indirect_block);
    for (int i = 0; i < size; ++i)
      free_map_release(indirect_block[i], 1);
    break;

  default:
    break;
  }

  free_map_release(*block_sector, 1);
}
/* /NEW */