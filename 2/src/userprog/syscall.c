#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static void syscall_handler(struct intr_frame *);
static bool check_ptr(const void *ptr);
static bool check_esp(const void *esp_);
static struct thread_file *get_thread_file(int fd);
static struct file *get_file(int fd);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  void *esp = f->esp;
  // check the address space of whole arguments
  if (!check_esp(esp))
  {
    exit(-1);
  }
  int sys_num = *(int *)esp;
  void *argv[3] = {esp + 4, esp + 8, esp + 12};
  // choose which syscall to call according to sys_num
  switch (sys_num)
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    exit(*(int *)argv[0]);
    break;
  case SYS_EXEC:
    f->eax = exec(*(const char **)argv[0]);
    break;
  case SYS_WAIT:
    f->eax = wait(*(pid_t *)argv[0]);
    break;
  case SYS_REMOVE:
    f->eax = remove(*(const char **)argv[0]);
    break;
  case SYS_OPEN:
    f->eax = open(*(const char **)argv[0]);
    break;
  case SYS_FILESIZE:
    f->eax = filesize(*(int *)argv[0]);
    break;
  case SYS_TELL:
    f->eax = tell(*(int *)argv[0]);
    break;
  case SYS_CLOSE:
    close(*(int *)argv[0]);
    break;
  case SYS_SEEK:
    seek(*(int *)argv[0], *(unsigned *)argv[1]);
    break;
  case SYS_CREATE:
    f->eax = create(*(const char **)argv[0], *(unsigned *)argv[1]);
    break;
  case SYS_READ:
    f->eax = read(*(int *)argv[0], *(void **)argv[1], *(unsigned *)argv[2]);
    break;
  case SYS_WRITE:
    f->eax = write(*(int *)argv[0], *(const void **)argv[1], *(unsigned *)argv[2]);
    break;
  default:
    exit(-1);
    NOT_REACHED();
  }
}

/* Check whether the given pointer is valid */
static bool
check_ptr(const void *ptr)
{
  struct thread *t = thread_current();
  if (!is_user_vaddr(ptr) || !ptr || !pagedir_get_page(t->pagedir, ptr))
    return false;
  return true;
}

/* Check the validation of the given esp and the address of the arguments  */
static bool
check_esp(const void *esp)
{
  // check the whole address space of sysnum
  if (!check_ptr(esp) || !check_ptr(esp + 3))
    return false;
  int sys_num = *(int *)esp;
  esp += sizeof(int);
  bool success = true;
  // check the whole address space of arguments
  switch (sys_num)
  {
  case SYS_HALT:
    break;
  case SYS_EXIT:
  case SYS_EXEC:
  case SYS_WAIT:
  case SYS_REMOVE:
  case SYS_OPEN:
  case SYS_FILESIZE:
  case SYS_TELL:
  case SYS_CLOSE:
    if (!check_ptr(esp) || !check_ptr(esp + 3))
      success = false;
    break;
  case SYS_SEEK:
  case SYS_CREATE:
    if (!check_ptr(esp) || !check_ptr(esp + 7))
      success = false;
    break;
  case SYS_READ:
  case SYS_WRITE:
    if (!check_ptr(esp) || !check_ptr(esp + 7))
      success = false;
    break;
  default:
    success = false;
  }
  return success;
}

/* Check the validation of all character in a string */
static bool
check_str(const char *str)
{
  for (const char *i = str;; i++)
  {
    if (!check_ptr(i))
      return false;
    if (*i == '\0')
      return 1;
  }
}

/* SysCall halt */
void halt(void)
{
  shutdown_power_off();
}

/* SysCall exit */
void exit(int status)
{
  struct thread *t = thread_current();
  /* Update the information of exit status */
  t->child_info->exit_status = status;
  thread_exit();
  NOT_REACHED();
}

/* SysCall exit */
pid_t exec(const char *file)
{
  if (!check_str(file))
    exit(-1);
  int pid;
  pid = process_execute(file);
  return pid;
}

/* SysCall wait */
int wait(pid_t pid)
{
  return process_wait(pid);
}

/* Create a file with initial_size. Return true if success and false otherwise */
bool create(const char *file, unsigned initial_size)
{
  /* If the string FILE is not valid, exit(-1) */
  if (!check_str(file))
    exit(-1);
  thread_acquire_file_lock();
  bool success = filesys_create(file, initial_size);
  thread_release_file_lock();
  return success;
}

/* Remove a file. Return true if success and false otherwise */
bool remove(const char *file)
{
  /* If the string FILE is not valid, exit(-1) */
  if (!check_str(file))
    exit(-1);
  thread_acquire_file_lock();
  bool success = filesys_remove(file);
  thread_release_file_lock();
  return success;
}

int open(const char *file)
{
  if (!check_str(file))
    exit(-1);
  thread_acquire_file_lock();
  struct file *temp = filesys_open(file);
  thread_release_file_lock();
  if (temp)
  {
    return thread_add_file(temp);
  }
  else
    return -1;
}

/* Return file size according to the file descriptior*/
int filesize(int fd)
{
  thread_acquire_file_lock();
  int size = file_length(get_file(fd));
  thread_release_file_lock();
  return size;
}

/* SysCall read */
int read(int fd, void *buffer, unsigned length)
{
  // Check the validation of buffer
  if (!check_ptr(buffer) || !check_ptr(buffer + length - 1))
    exit(-1);

  // Read from the STDIN
  if (fd == 0)
  {
    return input_getc();
  }
  struct file *file = get_file(fd);
  // If no such file, exit
  if (!file)
    exit(-1);
  thread_acquire_file_lock();
  int size = 0;
  size = file_read(file, buffer, length);
  thread_release_file_lock();
  return size;
}

/* SysCall write */
int write(int fd, const void *buffer, unsigned length)
{
  // Check the validation of buffer
  if (!check_ptr(buffer) || !check_ptr(buffer + length - 1))
    exit(-1);
  // write to the STDOUT
  if (fd == 1)
  {
    putbuf((const char *)buffer, length);
    return length;
  }
  else
  {
    struct file *file = get_file(fd);
    if (!file)
      exit(-1);
    thread_acquire_file_lock();
    int size = file_write(file, buffer, length);
    thread_release_file_lock();
    return size;
  }
}

/* SysCall seek */
void seek(int fd, unsigned position)
{
  struct file *file = get_file(fd);
  if (!file)
    exit(-1);
  thread_acquire_file_lock();
  file_seek(file, position);
  thread_release_file_lock();
}

/* SysCall tell */
unsigned tell(int fd)
{
  struct file *file = get_file(fd);
  thread_acquire_file_lock();
  unsigned pos = file_tell(file);
  thread_release_file_lock();
  return pos;
}

/* SysCall close */
void close(int fd)
{
  struct thread_file *thread_file = get_thread_file(fd);
  if (!thread_file)
    exit(-1);
  if (thread_file->opened == 0)
    exit(-1);
  thread_acquire_file_lock();
  file_close(thread_file->file);
  thread_close_file(fd);
  thread_release_file_lock();
}

/* Get the thread_file according to the file descriptor */
static struct thread_file *
get_thread_file(int fd)
{
  struct thread *t = thread_current();
  // find the file according to the file descriptor
  for (struct list_elem *i = list_begin(&t->owned_files); i != list_end(&t->owned_files); i = list_next(i))
  {
    struct thread_file *temp = list_entry(i, struct thread_file, file_elem);
    if (temp->fd == fd)
    {
      return temp;
    }
  }
  // if there is no such file, return NULL
  return NULL;
}

/* Get the file according ot the given file descriptor */
static struct file *
get_file(int fd)
{
  struct thread_file *thread_file = get_thread_file(fd);
  if (thread_file)
    return thread_file->file;
  return NULL;
}