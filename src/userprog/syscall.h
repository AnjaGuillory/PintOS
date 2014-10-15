#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void exit (int status);
void halt (void);
int write (int fd, const void *buffer, unsigned size);

#endif /* userprog/syscall.h */
