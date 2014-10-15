#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

int exit (int status);
void halt (void);
int write (int fd, const void *buffer, unsigned size);
//bool create (const char *file, unsigned initial_size);
int open (const char *file);

#endif /* userprog/syscall.h */
