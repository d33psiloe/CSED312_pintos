#include "vm/swap.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"

#define SECTOR_NUM (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *swap_disk;
static struct bitmap *swap_table;
static struct lock swap_lock;

/* swap table works like:
   true - the corresponding swap slot is valid
   false - the corresponding swap slot is invalid */
void
swap_init ()
{
    swap_disk = block_get_role (BLOCK_SWAP);
    size_t table_bits = block_size (swap_disk) / SECTOR_NUM;
    swap_table = bitmap_create (table_bits);
    bitmap_set_all (swap_table, true);
    lock_init (&swap_lock);
}

void
swap_in (void *frame_number, size_t idx)
{   
    lock_acquire (&swap_lock);

    if (bitmap_test (swap_table, idx))
        exit (-1);

    // set the slot invalid (true -> false)
    bitmap_flip (swap_table, idx);

    lock_release(&swap_lock);

    // read from swap disk
    size_t i;
    for (i = 0; i < SECTOR_NUM; i++)
        block_read (swap_disk, (idx * SECTOR_NUM) + i, frame_number + (i * BLOCK_SECTOR_SIZE));
}

size_t
swap_out (void *frame_number)
{
    lock_acquire (&swap_lock);
    // find the location of first valid slot
    size_t idx = bitmap_scan_and_flip (swap_table, 0, 1, true);
    lock_release (&swap_lock);

    // write at swap disk
    size_t i;
    for (i = 0; i < SECTOR_NUM; i++)
        block_write (swap_disk, (idx * SECTOR_NUM) + i, frame_number + (i * BLOCK_SECTOR_SIZE));

    return idx;
}