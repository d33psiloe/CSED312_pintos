#include "vm/frame.h"
#include "lib/kernel/list.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "vm/swap.h"

/* frame table (linked list) */
static struct list frame_table;
struct lock frame_lock;
static struct list_elem *clock_hand;

static void *frame_free (struct ft_entry *fte);
static struct ft_entry *get_fte (void *frame_number);

void 
frame_init ()
{
    list_init (&frame_table);
    lock_init (&frame_lock);
    clock_hand = NULL;
}

/* try allocation by palloc_get_page()
   if failed, search for victim frame by clock algorithm and evict it, then try allocation again
   eviction must support write-back for dirty (page) */
void *
//frame_allocate (enum palloc_flags flags, struct spt_entry *spte)
frame_allocate (enum palloc_flags flags, void *page_number)
{
    struct ft_entry *fte;
    void *frame_number;

    //lock_acquire (&frame_lock);

    frame_number = palloc_get_page (flags);
    /* if allocation fails... do eviction and try again*/
    if (frame_number == NULL)
    {
        printf("\n%s\n", "check1");
        frame_evict();
        printf("\n%s\n", "check2");
        frame_number = palloc_get_page (PAL_USER);
        if (frame_number == NULL)
            return NULL;
    }

    fte = (struct ft_entry *) malloc(sizeof *fte);
    fte->frame_number = frame_number;
    fte->page_number = page_number;
    fte->owner_thread = thread_current ();

    lock_acquire (&frame_lock);
    list_push_back (&frame_table, &fte->fte_elem);
    lock_release (&frame_lock);

    //lock_release (&frame_lock);     // ---
    
    return frame_number;
}

/* deallocates the given frame table entry */
static void *
frame_free (struct ft_entry *fte)
{
    printf("\n%s\n", "frame free start");
    //ASSERT(lock_held_by_current_thread(&frame_lock));
    if (fte == NULL)
        exit (-1);

    lock_acquire (&frame_lock);
    list_remove (&fte->fte_elem);
    lock_release (&frame_lock);
    
    palloc_free_page (fte->frame_number);       // free the physical address
    pagedir_clear_page (fte->owner_thread->pagedir, fte->page_number);  // deactivate corresponding virtual address accesses
    free (fte);

    printf("\n%s\n", "frame free end");
}

/* deallocates the frame with given physical address from frame table */
void *
free_frame (void *frame_number)
{
    //lock_acquire (&frame_lock);

    struct ft_entry *fte = get_fte (frame_number);
    frame_free (fte);

    //lock_release (&frame_lock);
}

/* deallocate all frames owned by given owner thread
   used when process_exit() is called */
void *
free_all_frames (struct thread *t)
{
    //lock_acquire (&frame_lock);

    printf("\n%s\n", "free all frame start");

    struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table); )
    {   
        struct ft_entry *fte = list_entry (e, struct ft_entry, fte_elem);
        e = list_next (e);
        if (fte->owner_thread == t)
            frame_free (fte);
    }

    printf("\n%s\n", "free all frame end");

    //lock_release (&frame_lock);
}

/* swap table - Addition */
void
frame_evict ()
{
    if (!list_empty(&frame_table))
    {
        struct ft_entry *fte;
        do
        {
            fte = list_entry(clock_hand, struct ft_entry, fte_elem);       
            if (fte != NULL)
                pagedir_set_accessed (fte->owner_thread->pagedir, fte->page_number, false);
            
            // iterate like circular queue
            if (clock_hand == list_end (&frame_table) || fte == NULL)
                clock_hand = list_begin (&frame_table);
            else 
                clock_hand = list_next (clock_hand);
        } while (!pagedir_is_accessed (fte->owner_thread->pagedir, fte->page_number));

        struct spt_entry *spte = spage_table_get_entry (&thread_current()->spage_table, fte->page_number);
        spte->page_type = PAGE_SWAP;
        spte->swap_idx = swap_out (fte->frame_number);

        //lock_release (&frame_lock);         // temporarily release frame lock

        free_frame (fte->frame_number);     // free the evicted frame
    
        //lock_acquire (&frame_lock);         // acquire frame lock again
    }
}

/* get matching frame table entry for the given physical address */
static struct ft_entry *
get_fte (void *frame_number)
{
    struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
        if (list_entry (e, struct ft_entry, fte_elem)->frame_number == frame_number)
            return list_entry (e, struct ft_entry, fte_elem);
    return NULL;
}