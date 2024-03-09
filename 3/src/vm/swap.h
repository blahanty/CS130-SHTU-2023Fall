#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>

void swap_init();
void swap_in(int, void *);
int swap_out(void *);

#endif /* vm/swap.h */
