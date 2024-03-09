#include "vm/swap.h"
#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"

#define BLOCKS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct block *swap_block;
struct bitmap *swap_bitmap;

void swap_init()
{
    swap_block = block_get_role(BLOCK_SWAP);
    int size = block_size(swap_block) / BLOCKS_PER_PAGE;
    swap_bitmap = bitmap_create(size);
    bitmap_set_all(swap_bitmap, true);
}

void swap_in(int swap_index, void *pp)
{
    for (int i = 0; i < BLOCKS_PER_PAGE; ++i)
        block_read(swap_block, swap_index * BLOCKS_PER_PAGE + i, pp + i * BLOCK_SECTOR_SIZE);

    bitmap_set(swap_bitmap, swap_index, true);
}

int swap_out(void *pp)
{
    int swap_index = bitmap_scan(swap_bitmap, 0, 1, true);
    for (int i = 0; i < BLOCKS_PER_PAGE; ++i)
        block_write(swap_block, swap_index * BLOCKS_PER_PAGE + i, pp + i * BLOCK_SECTOR_SIZE);

    bitmap_set(swap_bitmap, swap_index, false);
    return swap_index;
}
