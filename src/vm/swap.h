#ifndef VM_SWAP_H
#define VM_SWAP_H

typedef unsigned int size_t;
void swap_init (void);
void swap_in (struct spt_entry *, void *);
size_t swap_out (void *);

#endif