#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

int exit (int status);
void halt (void);

#endif /* userprog/syscall.h */
