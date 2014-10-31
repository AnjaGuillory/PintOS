#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int pid_t;

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
int read (int fd, void *buffer, unsigned size);
int wait (pid_t pid);

#endif /* userprog/syscall.h */
