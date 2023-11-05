#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


void syscall_init (void);
void exit (int status);
void close (int fd);

#endif /* userprog/syscall.h */
