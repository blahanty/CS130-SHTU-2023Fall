#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"

struct page_table_entry *page_table_entry_search(struct hash *pt, const void *vp)
{
    struct page_table_entry pte;
    struct hash_elem *e;

    pte.virtual_page = vp;
    e = hash_find(pt, &pte.helem);
    return e ? hash_entry(e, struct page_table_entry, helem) : NULL;
}

bool page_load(struct hash *pt, uint32_t *pd, void *vp)
{
    struct page_table_entry *pte = page_table_entry_search(pt, vp);
    if (!pte)
        return false;
    if (pte->status == FRAME)
        return true;
    void *pp = frame_get_page(PAL_USER | PAL_ZERO,vp);
    if (!pp)
        return false;

    switch (pte->status)
    {
    case ZERO:
        memset(pp, 0, PGSIZE);
        break;

    case SWAP:
        swap_in(pte->swap_index, pp);
        break;
    case MMAP:
        file_seek(pte->file, pte->ofs);
        int read_bytes = file_read(pte->file, pp, pte->read_bytes);
        if (read_bytes != pte->read_bytes)
        {
            frame_free(pp);
            return false;
        }
        memset(pp + read_bytes, 0, pte->zero_bytes);
        break;
    default:
        break;
    }
    if (!pagedir_set_page(pd, vp, pp, true))
    {
        frame_free(pp);
        return false;
    }
    pte->physical_page = pp;
    pte->status = FRAME;
    pagedir_set_dirty(pd, pp, false);
    return true;
}

unsigned page_table_entry_hash(const struct hash_elem *e, void *aux UNUSED)
{
    const struct page_table_entry *pte = hash_entry(e, struct page_table_entry, helem);
    return hash_bytes(&pte->virtual_page, sizeof(pte->virtual_page));
}

bool hash_less(const struct hash_elem *e1, const struct hash_elem *e2, void *aux UNUSED)
{
    const struct page_table_entry *a = hash_entry(e1, struct page_table_entry, helem);
    const struct page_table_entry *b = hash_entry(e2, struct page_table_entry, helem);
    return a->virtual_page < b->virtual_page;
}