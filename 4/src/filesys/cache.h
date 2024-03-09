#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "devices/block.h"

#define CACHE_SIZE 64
#define CACHE_WRITE_BACK_INTERVAL 1000

struct cache_entry
{
    block_sector_t sector;
    uint8_t buffer[BLOCK_SECTOR_SIZE];
    bool valid;
    bool dirty;
    bool second_chance;
};

void cache_init();
int cache_get(block_sector_t);
int cache_acquire(block_sector_t);
void cache_read(block_sector_t, void *);
void cache_write(block_sector_t, void *);
void cache_write_back(int);
void cache_clear();
void cache_write_behind();
void cache_write_behind_func(void *);
void cache_read_ahead(block_sector_t);
void cache_read_ahead_func(void *);

#endif /* filesys/cache.h */