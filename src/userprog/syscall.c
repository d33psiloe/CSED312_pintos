#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static void get_stack_data (int *esp, int *arg, int n);

void exit (int status);
int write (int fd, const void *buffer, unsigned size);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  int arg[3];

  switch (*(int *)(f->esp)) {
    case SYS_HALT:
      break;
    case SYS_EXIT:
      exit(*(uint32_t *)(f->esp + 4));
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      get_stack_data(f->esp, arg, 3);
      f->eax = write(arg[0], arg[1], arg[2]);
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
  }



  //thread_exit ();
}


static void
get_stack_data (int *esp, int *arg, int n)
{
    int i;
    for(i = 0; i < n; i++)
    {
        arg[i] = *((esp + 1) + i);
    }
}

void exit (int status) {
  thread_exit ();
}


int write (int fd, const void *buffer, unsigned size) {

  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1; 
}