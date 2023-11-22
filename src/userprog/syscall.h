#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


void syscall_init (void);
void exit (int status);
void close (int fd);

struct lock fs_lock;
#endif /* userprog/syscall.h */
