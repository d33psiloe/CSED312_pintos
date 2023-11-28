#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int mapid_t;

void syscall_init (void);
void exit (int status);
void close (int fd);
void munmap (mapid_t mapping);

/* Addition */
extern struct lock fs_lock;
//



#endif /* userprog/syscall.h */
