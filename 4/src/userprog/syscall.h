#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* NEW */
#include <stdbool.h>
#include <debug.h>

#define READDIR_MAX_LEN 14

typedef int pid_t;
/* /NEW */

void syscall_init(void);

/* NEW */
void halt() NO_RETURN;
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
bool chdir(const char *);
bool mkdir(const char *);
bool readdir(int, char *);
bool isdir(int);
int inumber(int);
/* /NEW */

#endif /* userprog/syscall.h */
