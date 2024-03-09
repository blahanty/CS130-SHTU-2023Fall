#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
/* NEW */
#include <list.h>
/* /NEW */
#include "filesys/off_t.h"
#include "devices/block.h"

/* NEW */
#define DIRECT_BLOCK_NUM 25
#define INDIRECT_BLOCK_NUM 100
#define BLOCK_POINTER_NUM (BLOCK_SECTOR_SIZE / 4)
/* /NEW */

struct bitmap;

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
    off_t length;   /* File size in bytes. */
    unsigned magic; /* Magic number. */
    /* NEW */
    bool is_dir;
    block_sector_t direct[DIRECT_BLOCK_NUM];     /* Direct blocks. */
    block_sector_t indirect[INDIRECT_BLOCK_NUM]; /* Indirect blocks. */
    /* /NEW */
};

/* In-memory inode. */
struct inode
{
    struct list_elem elem;  /* Element in inode list. */
    block_sector_t sector;  /* Sector number of disk location. */
    int open_cnt;           /* Number of openers. */
    bool removed;           /* True if deleted, false otherwise. */
    int deny_write_cnt;     /* 0: writes ok, >0: deny writes. */
    struct inode_disk data; /* Inode content. */
};

void inode_init(void);
/* NEW */
bool inode_create(block_sector_t, off_t, bool);
/* /NEW */
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);
/* NEW */
bool inode_disk_init(struct inode_disk *, size_t);
bool inode_block_init(block_sector_t *, size_t, int);
void inode_block_clear(block_sector_t *, size_t, int);
/* /NEW */

#endif /* filesys/inode.h */
