#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/palloc.h"

/* Frame table entry contains information of frame
   It works as an element of frame table */
struct ft_entry
{
    void *frame_number;                 // physical address of frame
    void *page_number;                  // virtual address of frame
    struct thread *owner_thread;        // thread ptr for frame's owner

    struct list_elem fte_elem;
};

extern struct lock frame_lock;

void frame_init (void);
//void *frame_allocate (enum palloc_flags, struct spt_entry *);
void *frame_allocate (enum palloc_flags, void *);
void *free_frame (void *);
void *free_all_frames (struct thread *);
void frame_evict (void);

#endif