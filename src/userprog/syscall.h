#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void exit (int status);
void halt (void);
int write (int fd, const void *buffer, unsigned size);
//bool create (const char *file, unsigned initial_size);
int open (const char *file);
int filesize (int fd);
unsigned tell (int fd);
void seek (int fd, unsigned position);
void close (int fd);

#endif /* userprog/syscall.h */
