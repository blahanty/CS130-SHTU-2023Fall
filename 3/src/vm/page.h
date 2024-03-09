#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "filesys/off_t.h"
#include "hash.h"
#include "threads/thread.h"

enum pte_status
{
    ZERO,
    FRAME,
    SWAP,
    MMAP
};

struct page_table_entry
{
    void *physical_page;
    void *virtual_page;
    struct hash_elem helem;
    enum pte_status status;
    int swap_index;
    struct file *file;
    off_t ofs;
    uint64_t read_bytes, zero_bytes;
    bool writable;
};

struct page_table_entry *page_table_entry_set(struct hash *, void *);
struct page_table_entry *page_table_entry_search(struct hash *, const void *);
bool page_load(struct hash *, uint32_t *, void *);
void page_table_entry_remove(struct hash *, struct page_table_entry *);
unsigned page_table_entry_hash(const struct hash_elem *, void *);
bool hash_less(const struct hash_elem *, const struct hash_elem *, void *);

typedef int mmapid_t;

struct mmap_entry
{
    mmapid_t id;
    struct file *file;
    void *va;
    struct list_elem elem;
};
#endif /* vm/page.h */