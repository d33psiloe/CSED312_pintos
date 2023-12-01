#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* changes */
#include "userprog/process.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "devices/input.h"

#include "vm/page.h"

/* Addition */
struct lock fs_lock;
//

static void syscall_handler (struct intr_frame *);

/* pintos project2 - System Call */
typedef int pid_t;

static bool is_address_valid (const void *vp);
static void get_arguments (uint32_t *esp, uint32_t *arg, int n);
static void check_arg_addr(const void *arg);

static void halt (void);
static pid_t exec (const char *cmd_line);
static int wait (pid_t pid);

static struct file_obj* fetch_file_obj (struct thread *t , int fd);

static bool create (const char*file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);

static mapid_t mmap (int fd, void *addr);
static struct mmap_file_obj * mmap_file_setup ( mapid_t mapid, struct file *file, void *page_number);
static struct mmap_file_obj *fetch_mmf_obj (mapid_t mapid);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&fs_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  if (!is_address_valid(f->esp))
    exit(-1);

  thread_current()->saved_esp = f->esp;
  uint32_t arg[3];    // arguments for system call functions

  switch (*(uint32_t *)f->esp)
  {
    case SYS_HALT:
      halt();
      break;
    
    case SYS_EXIT:
      get_arguments(f->esp, arg, 1);
      exit(arg[0]);
      break;
    
    case SYS_EXEC:
      get_arguments(f->esp, arg ,1);
      check_arg_addr((const void *)arg[0]);
      f->eax = exec((const char *)arg[0]);
      break;
    
    case SYS_WAIT:
      get_arguments(f->esp, arg, 1);
      f->eax = wait(arg[0]);
      break;
    
    case SYS_CREATE:
      get_arguments(f->esp, arg, 2);
      check_arg_addr((const void *)arg[0]);
      f->eax = create((const char *)arg[0], arg[1]);
      break;
    
    case SYS_REMOVE:
      get_arguments(f->esp, arg, 1);
      check_arg_addr((const void *)arg[0]);
      f->eax = remove(arg[0]);
      break;

    case SYS_OPEN:
      get_arguments(f->esp, arg, 1);
      check_arg_addr((const void *)arg[0]);
      f->eax = open((const char*)arg[0]);
      break;
    
    case SYS_FILESIZE:
      get_arguments(f->esp, arg, 1);
      f->eax = filesize(arg[0]);
      break;

    case SYS_READ:
      get_arguments(f->esp, arg, 3); 
      check_arg_addr((const void *)arg[1]);     
      f->eax = read(arg[0], (void *)arg[1], arg[2]);
      break;

    case SYS_WRITE:
      get_arguments(f->esp, arg, 3);
      check_arg_addr((const void *)arg[1]);
      f->eax = write(arg[0], (const void *) arg[1], (unsigned) arg[2]);
      break;

    case SYS_SEEK:
      get_arguments(f->esp, arg, 2);
      seek(arg[0], arg[1]);
      break;

    case SYS_TELL:
      get_arguments(f->esp, arg, 1);
      f->eax = tell(arg[0]);
      break;
    
    case SYS_CLOSE:
      get_arguments(f->esp, arg, 1);
      close(arg[0]);
      break;

    case SYS_MMAP:
        get_arguments(f->esp, arg, 2);
        f->eax = mmap(arg[0], (void *) arg[1]);
        break;

    case SYS_MUNMAP:
        get_arguments(f->esp, arg, 1);
        munmap((int) arg[0]);
        break;
  }
}

/* pintos project2 - System Call process*/
static bool
is_address_valid (const void *vp)
{
  int i;
  for(i = 0; i < sizeof(uint32_t *); i++)
  {
    /* Addition */
    // lazy-load implementation - page with no mapping now can be a valid case
    if(vp + i == NULL || !is_user_vaddr(vp + i) /*|| pagedir_get_page(thread_current()->pagedir, vp + i) == NULL*/)
      return false;
  }
  return true;
}

// gets stack data by esp and saves it to arg
// n is number of arguments passed by system call function
// arguments start from *(uint32_t *)(esp+4) == *(uint32_t *)esp + 1
static void
get_arguments (uint32_t *esp, uint32_t *arg, int n)
{
    int i;
    for(i = 0; i < n; i++)
    {
        if(!is_address_valid((esp+1)+i))
        {
          exit(-1);
        }
        
        arg[i] = *((esp+1)+i);
    }
}

static void
check_arg_addr(const void *arg)
{
  if(!is_address_valid(arg))
  {
    exit(-1);
  }
}

static void
halt (void)
{
    shutdown_power_off();
}

void 
exit (int status)
{
    struct thread *cur = thread_current();
    cur->exit_status = status;
    
    printf("%s: exit(%d)\n", cur->name, status);
    thread_exit();
}

static pid_t
exec (const char *cmd_line)
{
  printf("\n%s\n", "EXEC");
  tid_t pid = process_execute(cmd_line);
  
  return pid;
}

static int
wait (pid_t pid)
{
  return process_wait(pid);
}


/* pintos project2 - System Call file*/

static struct file_obj*
fetch_file_obj (struct thread *t , int fd)
{
  struct list *fd_list = &t->fd_list;
  if(fd_list == NULL)
    return NULL;
  else
  {
    /* Traverse through fd_list and find file_obj that matches with fd */
    struct list_elem *e;
    for(e = list_begin(fd_list); e != list_end(fd_list); e = list_next(e))
    {
      struct file_obj *cur_file_obj = list_entry(e, struct file_obj, file_elem);
      if( cur_file_obj -> fd_number == fd )
        return cur_file_obj;
    }
    /* could not find file_obj */
    return NULL;
  }
}

static bool
create (const char*file, unsigned initial_size)
{ 
  bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
  if(!is_holding_lock)
    lock_acquire (&fs_lock);

  bool success = filesys_create(file, initial_size);

  if(!is_holding_lock)
    lock_release(&fs_lock);
  return success;
}

static bool 
remove (const char *file)
{
  bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
  if(!is_holding_lock)
    lock_acquire (&fs_lock);

  bool success = filesys_remove(file);

  if(!is_holding_lock)
    lock_release(&fs_lock);
  return success;
}

static int
open (const char *file)
{
  bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
  if(!is_holding_lock)
    lock_acquire (&fs_lock);

  struct file *file_content_ = filesys_open(file);
  struct thread * cur = thread_current();

  if(!is_holding_lock)
    lock_release(&fs_lock);

  if (file_content_ == NULL)
    return -1;
  else
  {
    /* Allocate memory for file_object*/
    struct file_obj *file_object = malloc(sizeof(* file_object));
    
    /* Initialize file_object */
    int fd_number_ = cur -> next_fd_num;
    file_object -> fd_number = fd_number_;
    file_object -> file_content = file_content_;
    list_push_back(&cur -> fd_list, &file_object -> file_elem);
    cur -> next_fd_num++;
    return fd_number_;
  }
}

static int
filesize (int fd)
{
  struct file_obj *f = fetch_file_obj(thread_current(), fd);
  if( f == NULL)
    return -1;
  else
  {
    // if(!lock_held_by_current_thread(&fs_lock))
      lock_acquire (&fs_lock);
    int file_size = file_length( f->file_content );
    lock_release(&fs_lock);
    return file_size;
  }
}

static int
read (int fd, void *buffer, unsigned size)
{
  int read_size = 0;

  if( fd == 0 )
  {
    int i;
    for( i = 0; i < size; i++)
    {
      *((char *) (buffer +i)) = input_getc();
      read_size++;
    } 
  }
  else
  {
    struct file_obj *f = fetch_file_obj(thread_current(), fd);
    if( f == NULL)
      return -1;
    else
    {
      bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
      if(!is_holding_lock)
        lock_acquire (&fs_lock);
      read_size = file_read( f->file_content, buffer, size );
      if(!is_holding_lock)
        lock_release(&fs_lock);
    }
  }
  return read_size;
}

static int 
write (int fd, const void *buffer, unsigned size) 
{
  int write_size = 0;

  if (fd == 1) 
  {
    putbuf(buffer, size);
    return size;
  }
  else
  {
    struct file_obj *f = fetch_file_obj(thread_current(), fd);
    if( f == NULL)
      return -1;
    else
    {
      bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
      if(!is_holding_lock)
        lock_acquire (&fs_lock);
      write_size = file_write( f->file_content, buffer, size );
      if(!is_holding_lock)
        lock_release(&fs_lock);
      return write_size;
    }
  }
}

static void
seek (int fd, unsigned position)
{
  struct file_obj *f = fetch_file_obj(thread_current(), fd);
  if (f == NULL)
    return;
  else
  {
    bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
    if(!is_holding_lock)
      lock_acquire (&fs_lock);
    file_seek(f-> file_content, position);
    if(!is_holding_lock)
      lock_release(&fs_lock);
  }
}

static unsigned
tell (int fd)
{
  int position;
  struct file_obj *f = fetch_file_obj(thread_current(), fd);
  if (f == NULL)
    return 0;
  else
  {
    bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
    if(!is_holding_lock)
      lock_acquire (&fs_lock);
    position = file_tell(f-> file_content);
    if(!is_holding_lock)
      lock_release(&fs_lock);
  }
  return position;
}

void
close (int fd)
{
  struct file_obj *f = fetch_file_obj(thread_current(), fd);
  if (f == NULL){
    return;
  }

  else
  { 
    bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
    if(!is_holding_lock)
      lock_acquire (&fs_lock);

    file_close(f-> file_content);

    if(!is_holding_lock)
      lock_release(&fs_lock);
    list_remove(&f->file_elem);
    free(f);
    return;
  }
  
}

/* pintos project3 mmap file */
/* changes */
static mapid_t
mmap (int fd, void *addr)
{

  struct file_obj *f = fetch_file_obj (thread_current(), fd);

  if(f == NULL ||  !(is_user_vaddr(addr)) || addr == NULL || (int32_t) addr % PGSIZE != 0 )
    return -1;

  bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
  if(!is_holding_lock)
    lock_acquire (&fs_lock);

  struct file *mmap_file = file_reopen (f->file_content);

  struct mmap_file_obj *mmf_obj = mmap_file_setup (thread_current()->next_mapid++, mmap_file, addr);
  if(mmf_obj == NULL)
  { 
    // printf("\n%s\n", "mmf_overlap");
    if(!is_holding_lock)
      lock_release (&fs_lock);
    return -1;
  }

  if(!is_holding_lock)
    lock_release (&fs_lock);

  return mmf_obj->mapid;
}

static struct mmap_file_obj *
mmap_file_setup ( mapid_t mapid, struct file *file, void *page_number)
{
  struct mmap_file_obj *mmf_obj = malloc(sizeof( *mmf_obj));

  mmf_obj->mapid = mapid;
  mmf_obj->file = file;
  mmf_obj->page_number = page_number; //mmap file start address

  off_t ofs;
  int read_bytes = file_length(file);
  struct hash *spt =  &thread_current()->spage_table;

  /* check if overlaps mmaped pages */
  for (ofs = 0; ofs < read_bytes; ofs += PGSIZE)
        if (spage_table_get_entry(spt, page_number + ofs))
            return NULL;


  /* mmaping of the file; setup spage table entry*/

  ofs = 0;    
  while(read_bytes > 0)
  {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    spt_entry_file_setup (spt, file, ofs, page_number, 
                          page_read_bytes, page_zero_bytes, true);

    /* Advance. */
    read_bytes -= page_read_bytes;
    page_number += PGSIZE;
    ofs += page_read_bytes;
  }

  list_push_back(&thread_current()->mmap_file_list, &mmf_obj->mmf_list_elem);
  
  return mmf_obj;

}

void
munmap (mapid_t mapping)
{
  struct thread *t = thread_current();
  struct hash *spt =  &thread_current()->spage_table;

  struct mmap_file_obj *mmf_obj= fetch_mmf_obj (mapping);
  if (mmf_obj == NULL)
  {
    return;
  }
  void *page_number = mmf_obj -> page_number;

  bool is_holding_lock = lock_held_by_current_thread(&fs_lock);
  if(!is_holding_lock)
    lock_acquire(&fs_lock);

  // printf("\n%s\n", "can pass?");
  off_t ofs;
  int write_bytes = file_length(mmf_obj->file);
  // printf("\n%s\n", "pass can?");
  while (write_bytes > 0)
  {

    size_t page_write_bytes = write_bytes < PGSIZE ? write_bytes : PGSIZE;

    struct spt_entry *spte = spage_table_get_entry(spt, page_number);
    void *kpage = pagedir_get_page(t->pagedir, page_number);

    if (pagedir_is_dirty(t->pagedir, page_number)) {
        file_write_at(spte->file, kpage, spte->read_bytes, spte-> file_offset);
    }

    pagedir_clear_page (t->pagedir, page_number);
    spt_entry_free (spt, spte);

    write_bytes -= page_write_bytes;
    page_number += PGSIZE;
  }
  
  list_remove(&mmf_obj->mmf_list_elem);
  if(!is_holding_lock)
    lock_release(&fs_lock);
}

static struct mmap_file_obj *
fetch_mmf_obj (mapid_t mapid)
{
  struct list *mmf_list = &thread_current()-> mmap_file_list;
  struct list_elem *e;
  for (e = list_begin(mmf_list); e != list_end(mmf_list); e = list_next(e))
  {
    struct mmap_file_obj *mmf_obj = list_entry (e, struct mmap_file_obj, mmf_list_elem);
    if (mmf_obj->mapid == mapid)
      return mmf_obj;
  }
  
  return NULL;
}