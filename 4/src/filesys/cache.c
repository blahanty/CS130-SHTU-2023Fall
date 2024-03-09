#include "filesys/cache.h"
#include "devices/timer.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

static struct cache_entry cache[CACHE_SIZE];
static struct lock cache_lock;
static int clock_ptr;

/* Initialize buffer cache. */
void cache_init()
{
    for (int i = 0; i < CACHE_SIZE; ++i)
        cache[i].valid = false;

    lock_init(&cache_lock);
    cache_write_behind();
}

/* Get cache data whose sector is SEC. */
int cache_get(block_sector_t sec)
{
    lock_acquire(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; ++i)
        if (cache[i].valid && cache[i].sector == sec)
        {
            lock_release(&cache_lock);
            return i;
        }
    lock_release(&cache_lock);

    return cache_acquire(sec);
}

/* Acquire free cache block.
   If no free cache block, evict one using clock algorithm. */
int cache_acquire(block_sector_t sec)
{
    lock_acquire(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; ++i)
        if (!cache[i].valid)
        {
            cache[i].sector = sec;
            cache[i].valid = true;
            cache[i].dirty = false;
            cache[i].second_chance = false;
            block_read(fs_device, sec, cache[i].buffer);
            lock_release(&cache_lock);
            return i;
        }

    /* No free cache block. */
    int ret;
    while (true)
    {
        if (cache[clock_ptr].second_chance)
        {
            cache[clock_ptr++].second_chance = false;
            clock_ptr %= CACHE_SIZE;
        }
        else
        {
            cache_write_back(clock_ptr);
            cache[clock_ptr].sector = sec;
            cache[clock_ptr].valid = true;
            cache[clock_ptr].second_chance = false;
            block_read(fs_device, sec, cache[clock_ptr].buffer);

            ret = clock_ptr++;
            clock_ptr %= CACHE_SIZE;
            break;
        }
    }
    lock_release(&cache_lock);

    return ret;
}

/* Read cache block whose sector is SEC into DEST. */
void cache_read(block_sector_t sec, void *dest)
{
    int index = cache_get(sec);

    lock_acquire(&cache_lock);
    cache[index].second_chance = true;
    memcpy(dest, cache[index].buffer, BLOCK_SECTOR_SIZE);
    lock_release(&cache_lock);
}

/* Write cache block whose sector is SEC from SRC. */
void cache_write(block_sector_t sec, void *src)
{
    int index = cache_get(sec);

    lock_acquire(&cache_lock);
    cache[index].second_chance = true;
    cache[index].dirty = true;
    memcpy(cache[index].buffer, src, BLOCK_SECTOR_SIZE);
    lock_release(&cache_lock);
}

/* Write cache block whose index is INDEX back to disk if dirty. */
void cache_write_back(int index)
{
    if (cache[index].valid && cache[index].dirty)
    {
        block_write(fs_device, cache[index].sector, cache[index].buffer);
        cache[index].dirty = false;
    }
}

/* Clear buffer cache. */
void cache_clear()
{
    lock_acquire(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; ++i)
    {
        cache_write_back(i);
        cache[i].valid = false;
    }
    lock_release(&cache_lock);
}

/* Write behind. */
void cache_write_behind()
{
    thread_create("cache write behind", PRI_DEFAULT, cache_write_behind_func, NULL);
}

/* Write behind function. */
void cache_write_behind_func(void *aux UNUSED)
{
    while (true)
    {
        lock_acquire(&cache_lock);
        for (int i = 0; i < CACHE_SIZE; ++i)
            cache_write_back(i);
        lock_release(&cache_lock);

        timer_msleep(CACHE_WRITE_BACK_INTERVAL);
    }
}

/* Read ahead. */
void cache_read_ahead(block_sector_t sec) {}

/* Read ahead function. */
void cache_read_ahead_func(void *aux UNUSED) {}