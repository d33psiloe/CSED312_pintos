#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#include "filesys/file.h"
#include "filesys/off_t.h"

enum page_types
  {
    PAGE_FRAME,
    PAGE_STACK,
    PAGE_FILE,
    PAGE_SWAP
  };

struct spt_entry
  {
     void *page_number;
     void *frame_number;

     enum page_types page_type;

     struct hash_elem spt_elem;

     struct file* file;
     off_t file_offset;
     uint32_t read_bytes;
     uint32_t zero_bytes;

     off_t swap_slot;
     
     bool writable;
  };


void spage_table_init (struct hash *spage_table);

void spt_entry_file_setup (struct hash *spt, struct file* file_, off_t ofs_, void *upage, 
                            uint32_t read_bytes_, uint32_t zero_bytes_, bool writable_); 
void spt_entry_frame_setup (struct hash *spt, void *upage, void *kpage);
void spt_entry_stack_setup (struct hash *spt, void *upage);

void spage_table_free (struct hash *spt);
void spt_entry_free (struct hash *spt, struct spt_entry *spte); 

struct spt_entry* spage_table_get_entry(struct hash *spt, const void *upage);
bool lazy_load_page (struct hash *spt, const void *upage);
bool extend_stack (struct hash *spt, void *upage, void *esp);
#endif