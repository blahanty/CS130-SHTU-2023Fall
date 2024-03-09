#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* NEW */
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
/* /NEW */

static void syscall_handler(struct intr_frame *);
/* NEW */
static bool check_ptr(const void *);
static bool check_esp(const void *);
static struct thread_file *get_thread_file(int);
static struct file *get_file(int);
/* /NEW */

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  /* NEW */
  void *esp = f->esp;
  /* Check the address space of whole arguments. */
  if (!check_esp(esp))
    exit(-1);

  int sys_num = *(int *)esp;
  void *argv[3] = {esp + 4, esp + 8, esp + 12};
  /* Choose which syscall to call according to sys_num. */
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

  case SYS_CHDIR:
    f->eax = chdir(*(const char **)argv[0]);
    break;

  case SYS_MKDIR:
    f->eax = mkdir(*(const char **)argv[0]);
    break;

  case SYS_READDIR:
    f->eax = readdir(*(int *)argv[0], *(char **)argv[1]);
    break;

  case SYS_ISDIR:
    f->eax = isdir(*(int *)argv[0]);
    break;

  case SYS_INUMBER:
    f->eax = inumber(*(int *)argv[0]);
    break;

  default:
    exit(-1);
    NOT_REACHED();
  }
  /* /NEW */
}

/* NEW */
/* Check whether the given pointer PTR is valid. */
static bool
check_ptr(const void *ptr)
{
  struct thread *t = thread_current();
  if (!is_user_vaddr(ptr) || !ptr || !pagedir_get_page(t->pagedir, ptr))
    return false;

  return true;
}

/* Check the validation of the given ESP and the address of the arguments.  */
static bool
check_esp(const void *esp)
{
  /* Check the whole address space of sysnum. */
  if (!check_ptr(esp) || !check_ptr(esp + 3))
    return false;

  int sys_num = *(int *)esp;
  esp += sizeof(int);
  bool success = true;
  /* Check the whole address space of arguments. */
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
  case SYS_CHDIR:
  case SYS_MKDIR:
  case SYS_ISDIR:
  case SYS_INUMBER:
    if (!check_ptr(esp) || !check_ptr(esp + 3))
      success = false;
    break;

  case SYS_SEEK:
  case SYS_CREATE:
  case SYS_READDIR:
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

/* Check the validation of all character in the string STR. */
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

/* Terminates Pintos. */
void halt(void)
{
  shutdown_power_off();
}

/* Terminates the current user program, returning STATUS to the kernel. */
void exit(int status)
{
  struct thread *t = thread_current();
  /* Update the information of exit status. */
  t->child_info->exit_status = status;
  thread_exit();
  NOT_REACHED();
}

/* Runs the executable whose name is given in CMD_LINE, passing any given arguments,
   and returns the new process’s program id (pid). */
pid_t exec(const char *file)
{
  if (!check_str(file))
    exit(-1);

  int pid;
  pid = process_execute(file);
  return pid;
}

/* Waits for a child process PID and retrieves the child’s exit status. */
int wait(pid_t pid)
{
  return process_wait(pid);
}

/* Creates a new file called FILE initially INITIAL_SIZE bytes in size. */
bool create(const char *file, unsigned initial_size)
{
  /* If the string FILE is not valid, exit(-1). */
  if (!check_str(file))
    exit(-1);

  thread_acquire_file_lock();
  bool success = filesys_create(file, initial_size, false);
  thread_release_file_lock();

  return success;
}

/* Deletes the file called FILE. */
bool remove(const char *file)
{
  /* If the string FILE is not valid, exit(-1). */
  if (!check_str(file))
    exit(-1);

  thread_acquire_file_lock();
  bool success = filesys_remove(file);
  thread_release_file_lock();

  return success;
}

/* Opens the file called FILE. */
int open(const char *file)
{
  if (!check_str(file))
    exit(-1);

  thread_acquire_file_lock();
  struct file *temp_file = filesys_open(file);
  thread_release_file_lock();

  if (!temp_file)
    return -1;

  if (!(file_get_inode(temp_file)->data.is_dir))
    return thread_add_file(temp_file, NULL);

  thread_acquire_file_lock();
  struct dir *temp_dir = dir_open(inode_reopen(file_get_inode(temp_file)));
  thread_release_file_lock();

  return thread_add_file(temp_file, temp_dir);
}

/* Returns the size, in bytes, of the file open as FD. */
int filesize(int fd)
{
  thread_acquire_file_lock();
  int size = file_length(get_file(fd));
  thread_release_file_lock();

  return size;
}

/* Reads size bytes from the file open as FD into BUFFER. */
int read(int fd, void *buffer, unsigned length)
{
  /* Check the validation of BUFFER. */
  if (!check_ptr(buffer) || !check_ptr(buffer + length - 1))
    exit(-1);

  /* Read from the STDIN. */
  if (fd == 0)
    return input_getc();

  struct thread_file *thread_file = get_thread_file(fd);
  /* If no such file, exit(-1). */
  if (!thread_file || !thread_file->file)
    exit(-1);

  if (thread_file->dir)
    return -1;

  thread_acquire_file_lock();
  int size = 0;
  size = file_read(thread_file->file, buffer, length);
  thread_release_file_lock();

  return size;
}

/* Writes SIZE bytes from buffer to the open file FD. */
int write(int fd, const void *buffer, unsigned length)
{
  /* Check the validation of BUFFER. */
  if (!check_ptr(buffer) || !check_ptr(buffer + length - 1))
    exit(-1);

  /* Write to the STDOUT. */
  if (fd == 1)
  {
    putbuf((const char *)buffer, length);
    return length;
  }

  struct thread_file *thread_file = get_thread_file(fd);
  if (!thread_file || !thread_file->file)
    exit(-1);

  if (thread_file->dir)
    return -1;

  thread_acquire_file_lock();
  int size = file_write(thread_file->file, buffer, length);
  thread_release_file_lock();

  return size;
}

/* Changes the next byte to be read or written in open file FD to position, expressed in
   bytes from the beginning of the file. */
void seek(int fd, unsigned position)
{
  struct file *file = get_file(fd);
  if (!file)
    exit(-1);

  thread_acquire_file_lock();
  file_seek(file, position);
  thread_release_file_lock();
}

/* Returns the position of the next byte to be read or written in open file FD, expressed
   in bytes from the beginning of the file. */
unsigned tell(int fd)
{
  struct file *file = get_file(fd);

  thread_acquire_file_lock();
  unsigned pos = file_tell(file);
  thread_release_file_lock();

  return pos;
}

/* Closes file descriptor FD. */
void close(int fd)
{
  struct thread_file *thread_file = get_thread_file(fd);
  if (!thread_file)
    exit(-1);

  if (thread_file->opened == 0)
    exit(-1);

  thread_acquire_file_lock();
  file_close(thread_file->file);
  if (thread_file->dir)
    dir_close(thread_file->dir);

  thread_close_file(fd);
  thread_release_file_lock();
}

/* Changes the current working directory of the process to DIR, which may be relative
or absolute. */
bool chdir(const char *dir)
{
  if (!check_str(dir))
    exit(-1);

  thread_acquire_file_lock();
  struct dir *dir_opened = dir_open_path(dir);
  thread_release_file_lock();
  if (!dir_opened)
    return false;

  thread_acquire_file_lock();
  struct thread *cur = thread_current();
  dir_close(cur->cur_dir);
  cur->cur_dir = dir_opened;
  thread_release_file_lock();

  return true;
}

/* Creates the directory named DIR, which may be relative or absolute. */
bool mkdir(const char *dir)
{
  if (!check_str(dir))
    exit(-1);

  thread_acquire_file_lock();
  bool success = filesys_create(dir, 0, true);
  thread_release_file_lock();

  return success;
}

/* Reads a directory entry from file descriptor FD, which must represent a directory. */
bool readdir(int fd, char *name)
{
  if (!check_str(name) || !check_str(name + READDIR_MAX_LEN + 1))
    exit(-1);

  bool success = false;
  thread_acquire_file_lock();
  struct thread_file *thread_file = get_thread_file(fd);
  if (!thread_file || !thread_file->dir)
  {
    thread_release_file_lock();
    return false;
  }

  struct inode *node = file_get_inode(thread_file->file);
  if (!node || !node->data.is_dir)
  {

    thread_release_file_lock();
    return false;
  }

  success = dir_readdir(thread_file->dir, name);
  thread_release_file_lock();
  return success;
}

/* Return true if FD represents a directory rather than an ordinary file. */
bool isdir(int fd)
{
  return file_get_inode(get_thread_file(fd)->file)->data.is_dir;
}

/* Returns the inode number of the inode associated with FD, which may represent an
ordinary file or a directory. */
int inumber(int fd)
{
  return inode_get_inumber(file_get_inode(get_thread_file(fd)->file));
}

/* Get the thread_file according to the file descriptor FD. */
static struct thread_file *
get_thread_file(int fd)
{
  struct thread *t = thread_current();
  /* Find the file according to the file descriptor. */
  for (struct list_elem *i = list_begin(&t->owned_files); i != list_end(&t->owned_files); i = list_next(i))
  {
    struct thread_file *temp = list_entry(i, struct thread_file, file_elem);
    if (temp->fd == fd)
      return temp;
  }

  /* if there is no such file, return NULL. */
  return NULL;
}

/* Get the file according to the given file descriptor FD. */
static struct file *
get_file(int fd)
{
  struct thread_file *thread_file = get_thread_file(fd);
  if (thread_file)
    return thread_file->file;

  return NULL;
}
/* /NEW */