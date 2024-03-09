#include "vm/frame.h"
#include "threads/palloc.h"
#include "vm/page.h"

static struct list frame_table;
static struct lock frame_lock;

void frame_init_table()
{
    list_init(&frame_table);
    lock_init(&frame_lock);
}

void frame_table_entry_set(void* pp, void *vp, struct frame_table_entry *fte)
{
    lock_acquire(&frame_lock);
    if (!fte)
    {
        fte = malloc(sizeof(struct frame_table_entry));
        if (!fte){
            lock_release(&frame_lock);
            return;
        }
    }
    fte->physical_page = pp;
    fte->virtual_page = vp;
    fte->owner = thread_current();
    list_push_back(&frame_table, &fte->elem);
    struct page_table_entry *pte = malloc(sizeof(struct page_table_entry));
    if (!pte)
    {
        lock_release(&frame_lock);
        frame_free(fte);
        return NULL;
    }
    pte->physical_page = pp;
    pte->virtual_page = vp;
    pte->status = ZERO;
    hash_insert(&thread_current()->page_table, &pte->helem);
    lock_release(&frame_lock);
}

void *frame_get_page(enum palloc_flags flag,void*vp)
{
    void *pp = (void *)palloc_get_page(flag);
    struct frame_table_entry *frame = NULL;
    if (pp)
        frame_table_entry_set(pp,vp, frame);
    else
        pp = frame_evict_page(vp);

    return pp;
}

void *frame_evict_page(void* vp)
{
    struct frame_table_entry *fte_evict = NULL;
    struct list_elem *e;
    lock_acquire(&frame_lock);
    for (e = list_begin(&frame_table); e != list_end(&frame_table);
         e = list_next(e))
    {
        struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
        if (pagedir_is_accessed(thread_current()->pagedir, fte->virtual_page))
        {
            pagedir_set_accessed(thread_current()->pagedir, fte->virtual_page, false);
            continue;
        }
        fte_evict = fte;
    }
    if (!fte_evict)
    {
        for (e = list_begin(&frame_table); e != list_end(&frame_table);
             e = list_next(e))
        {
            struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
            if (pagedir_is_accessed(thread_current()->pagedir, fte->virtual_page))
            {
                pagedir_set_accessed(thread_current()->pagedir, fte->virtual_page, false);
                continue;
            }
            fte_evict = fte;
        }
        if (!fte_evict)
        {
            lock_release(&frame_lock);
            return NULL;
        }
    }
    pagedir_clear_page(fte_evict->owner->pagedir, fte_evict->virtual_page);
    int swap_index = swap_out(fte_evict->physical_page);
    lock_release(&frame_lock);
    struct page_table_entry *pte = page_table_entry_search(&fte_evict->owner->page_table, fte_evict->virtual_page);
    if (!pte)
        return NULL;
    pte->status = SWAP;
    pte->swap_index = swap_index;
    pte->physical_page = NULL;
    frame_free(fte_evict->physical_page);

    return frame_get_page(PAL_USER | PAL_ZERO,vp);
}

void frame_free(void *frame)
{
    struct frame_table_entry *fte;
    struct list_elem *e;

    lock_acquire(&frame_lock);
    for (e = list_begin(&frame_table); e != list_end(&frame_table);
         e = list_next(e))
    {
        fte = list_entry(e, struct frame_table_entry, elem);
        if (fte->physical_page == frame)
        {
            list_remove(e);
            free(fte);
            break;
        }
    }
    palloc_free_page(frame);
    lock_release(&frame_lock);
}