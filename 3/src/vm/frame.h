#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "list.h"
#include "threads/thread.h"
#include "threads/palloc.h"

struct frame_table_entry
{
    void *physical_page;
    void *virtual_page;
    struct thread *owner;
    struct list_elem elem;
};

void frame_init_table();
void frame_table_entry_set(void *,void*, struct frame_table_entry *);
void *frame_get_page(enum palloc_flags,void*);
void *frame_evict_page(void*);
void frame_free(void *);

#endif /* vm/frame.h */