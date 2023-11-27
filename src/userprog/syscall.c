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
      f->eax = write(arg[0], (const void *) arg[1], arg[2]);
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
            exit(-1);
        
        arg[i] = *((esp+1)+i);
    }
}

static void
check_arg_addr(const void *arg)
{
  if(!is_address_valid(arg))
      exit(-1);
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
  lock_acquire(&fs_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&fs_lock);
  return success;
}

static bool 
remove (const char *file)
{
  lock_acquire(&fs_lock);
  bool success = filesys_remove(file);
  lock_release(&fs_lock);
  return success;
}

static int
open (const char *file)
{
  lock_acquire(&fs_lock);
  struct file *file_content_ = filesys_open(file);
  struct thread * cur = thread_current();
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
    lock_acquire(&fs_lock);
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
      lock_acquire(&fs_lock);
      read_size = file_read( f->file_content, buffer, size );
      lock_release(&fs_lock);
    }
  }
  return read_size;
}

static int 
write (int fd, const void *buffer, unsigned size) 
{

  // exit if buffer invalid
  if(!is_address_valid(buffer))
    exit(-1);

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
      lock_acquire(&fs_lock);
      write_size = file_write( f->file_content, buffer, size );
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
    lock_acquire(&fs_lock);
    file_seek(f-> file_content, position);
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
    lock_acquire(&fs_lock);
    position = file_tell(f-> file_content);
    lock_release(&fs_lock);
  }
  return position;
}

void
close (int fd)
{
  struct file_obj *f = fetch_file_obj(thread_current(), fd);
  if (f == NULL)
    return;

  else
  {
    lock_acquire(&fs_lock);
    file_close(f-> file_content);
    lock_release(&fs_lock);
    list_remove(&f->file_elem);
    free(f);
    return;
  }
  
}
