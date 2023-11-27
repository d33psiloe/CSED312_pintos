#include "vm/page.h"
#include "vm/frame.h"

#include "threads/thread.h"
#include "userprog/pagedir.h"

static unsigned spage_table_hash_func (const struct hash_elem *e, void *aux);
static bool spage_table_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);


static void spt_entry_destructor (struct hash_elem *e, void * aux);


extern struct lock fs_lock;

/*
  Initializes Supplemental Page Table using hash fns in hash.c
  spage table is per-thread structure
*/
void 
spage_table_init (struct hash *spage_table)
{
  hash_init(spage_table, spage_table_hash_func, spage_table_less_func, NULL);
}

static unsigned 
spage_table_hash_func (const struct hash_elem *e, void *aux)
{
  struct spt_entry *spte = hash_entry(e, struct spt_entry, spt_elem);

  return hash_bytes(&spte->page_number, sizeof(spte->page_number));
}

static bool 
spage_table_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  const uint32_t page_number_a = hash_entry(a, struct spt_entry, spt_elem) -> page_number;
  const uint32_t page_nubmer_b = hash_entry(b, struct spt_entry, spt_elem) -> page_number; 

  return page_number_a < page_nubmer_b;
}

/*
  set up spage table entry that will be loaded from file
  used when lazily loading code/data segments
*/
void  
spt_entry_file_setup (struct hash *spt, struct file* file_, off_t ofs_,
                      void *upage, uint32_t read_bytes_, uint32_t zero_bytes_, bool writable_) 
{
  struct spt_entry *spte = malloc( sizeof(*spte) );

  spte -> page_number = upage;
  spte -> frame_number = NULL;

  spte -> page_type = PAGE_FILE;
  spte -> wb_type = WB_SWAP;

  spte -> file = file_;
  spte -> file_offset = ofs_;
  spte -> read_bytes = read_bytes_;
  spte -> zero_bytes = zero_bytes_;
  
  spte -> writable = writable_;

  hash_insert(spt, &spte->spt_elem);
}

/*
  set up spage table entry that has physical frame allocated
  used in setup_stack()
*/
void
spt_entry_frame_setup (struct hash *spt, void *upage, void *kpage)
{
  struct spt_entry *spte = malloc( sizeof(*spte) );

  spte -> page_number = upage;
  spte -> frame_number = kpage;

  spte -> page_type = PAGE_FRAME;
  spte ->wb_type = WB_SWAP;

  spte -> writable = true;

  hash_insert(spt, &spte->spt_elem); 
}

/*
  Deletes spage table using hash_destroy
  hash destroy uses DESTRUCTOR
*/
void
spage_table_free (struct hash *spt, struct spt_entry *spte) 
{
    hash_destroy(spt, spt_entry_destructor);
}

static void
spt_entry_destructor (struct hash_elem *e, void * aux)
{
  struct spt_entry *spte;
  spte = hash_entry(e, struct spt_entry, spt_elem); 
  free(spte);
} 

/*
  Deletes spage table entry using hash_delete
*/
void
spt_entry_free (struct hash *spt, struct spt_entry *spte)
{
  hash_delete(spt, &spte ->spt_elem);
  free(spte);
}

struct spt_entry*
get_spte(struct hash *spt, const void *upage)
{
  struct spt_entry spte;
  struct hash_elem *e;

  spte.page_number = upage;
  e = hash_find(spt, &spte.spt_elem);

  return e != NULL ? hash_entry(e, struct spt_entry, spt_elem) : NULL;
}

bool
lazy_load_page (struct hash *spt, const void *upage)
{
  struct spt_entry *spte;

  spte = get_spte(spt, upage);
  if (spte == NULL)
    exit(-1);

 
  void *kpage = frame_allocate(PAL_USER, upage);
  if (kpage == NULL)
    exit(-1);
    

  bool is_holding_lock = lock_held_by_current_thread (&fs_lock);
  switch (spte -> page_type)
  {
    case PAGE_FRAME:
      break;
    case PAGE_FILE:
      if(!is_holding_lock)
        lock_acquire (&fs_lock);

      file_read_at(spte->file, kpage, spte->read_bytes, spte->file_offset);
      memset (kpage + spte->read_bytes, 0, spte->zero_bytes);

      if(!is_holding_lock)
        lock_release(&fs_lock);

      break;
    case PAGE_SWAP:
    //implement here
      break;
  }
  
  uint32_t *pagedir = thread_current()->pagedir;
  pagedir_set_page(pagedir, upage, kpage, spte->writable); 

  spte ->frame_number = kpage;
  spte ->page_type = PAGE_FRAME;
  
}
