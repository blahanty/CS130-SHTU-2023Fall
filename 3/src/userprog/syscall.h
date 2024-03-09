#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* NEW */
#include <stdbool.h>
#include <debug.h>
#include "vm/page.h"

typedef int pid_t;
/* /NEW */

void syscall_init(void);

/* NEW */
void halt(void) NO_RETURN;
void exit(int) NO_RETURN;
pid_t exec(const char *);
int wait(pid_t);
bool create(const char *, unsigned);
bool remove(const char *);
int open(const char *);
int filesize(int);
int read(int, void *, unsigned);
int write(int, const void *, unsigned);
void seek(int, unsigned);
unsigned tell(int);
void close(int);
mmapid_t mmap(int, void *);
void munmap(mmapid_t);
/* /NEW */

#endif /* userprog/syscall.h */
