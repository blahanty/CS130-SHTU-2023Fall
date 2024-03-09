#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* NEW */
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/exception.h"
#include "vm/page.h"
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

/* NEW */
static void *ge;
/* /NEW */

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  /* NEW */
  void *esp = f->esp;
  ge = esp;
  /* Check the address space of whole arguments. */
  if (!check_esp(esp))
  {
    exit(-1);
  }

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

  case SYS_MMAP:
    f->eax = mmap(*(int *)argv[0], *(void **)argv[1]);
    break;

  case SYS_MUNMAP:
    munmap(*(mmapid_t *)argv[0]);
    break;

  default:
    exit(-1);
    NOT_REACHED();
  }
}

/* Check whether the given pointer PTR is valid. */
static bool
check_ptr(const void *ptr)
{
  struct thread *t = thread_current();
  if (!is_user_vaddr(ptr) || !ptr || !pagedir_get_page(t->pagedir, ptr))
    return false;

  return true;
}

/* Check the validation of the given ESP and the address of the arguments. */
static bool
check_esp(const void *esp)
{
  /* Check the whole address space of sys_num. */
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
  case SYS_MUNMAP:
    if (!check_ptr(esp) || !check_ptr(esp + 3))
      success = false;
    break;

  case SYS_SEEK:
  case SYS_CREATE:
  case SYS_MMAP:
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

/* Check the validation of all character in a string STR. */
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

/* SysCall halt. */
void halt(void)
{
  shutdown_power_off();
}

/* SysCall exit. */
void exit(int status)
{
  struct thread *t = thread_current();
  /* Update the information of exit status. */
  t->child_info->exit_status = status;
  thread_exit();
  NOT_REACHED();
}

/* SysCall exec. */
pid_t exec(const char *file)
{
  if (!check_str(file))
    exit(-1);

  int pid;
  pid = process_execute(file);
  return pid;
}

/* SysCall wait. */
int wait(pid_t pid)
{
  return process_wait(pid);
}

/* Create FILE with INITIAL_SIZE. Return true if success and false otherwise. */
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

/* Remove FILE. Return true if success and false otherwise. */
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

/* SysCall open. */
int open(const char *file)
{
  if (!check_str(file))
    exit(-1);

  thread_acquire_file_lock();
  struct file *temp = filesys_open(file);
  thread_release_file_lock();

  if (temp)
    return thread_add_file(temp);
  else
    return -1;
}

/* Return file size according to the file descriptior FD. */
int filesize(int fd)
{
  thread_acquire_file_lock();
  int size = file_length(get_file(fd));
  thread_release_file_lock();

  return size;
}

/* SysCall read. */
int read(int fd, void *buffer, unsigned length)
{
  void *buf = buffer;
  if (length < PGSIZE)
  {
    read_prepare(buf);
    read_prepare(buf + length);
  }
  else
  {
    buf = pg_round_down(buf);
    unsigned count = (length % PGSIZE) ? length / PGSIZE + 1 : length / PGSIZE;
    for (int i = 0; i <= count; i++, buf += PGSIZE)
      read_prepare(buf);
  }
  /* Read from the STDIN. */
  if (fd == 0)
    return input_getc();

  struct file *file = get_file(fd);
  /* If no such file, exit. */
  if (!file)
    exit(-1);

  thread_acquire_file_lock();
  int size = file_read(file, buffer, length);
  thread_release_file_lock();
  return size;
}

void read_prepare(void *buf)
{
  if (buf == NULL || !is_user_vaddr(buf))
    exit(-1);

  struct thread *cur = thread_current();
  bool success = false;
  if (!pagedir_get_page(cur->pagedir, buf))
  {
    struct page_table_entry *pte = page_table_entry_search(&cur->page_table, buf);
    if (pte)
      success = page_load(&cur->page_table, cur->pagedir, pte->virtual_page);
    else if (buf >= ge - 32)
    {
      void *page = pg_round_down(buf);
      if (buf >= (PHYS_BASE - STACK_SIZE) && (buf >= ge || buf == (ge - 4) || buf == (ge - 32)))
      {
        pte = malloc(sizeof(struct page_table_entry));
        pte->physical_page = NULL;
        pte->virtual_page = page;
        pte->status = ZERO;
        pte->writable = true;
        hash_insert(&thread_current()->page_table, &pte->helem);
      }
      else
        exit(-1);

      if (page_load(&cur->page_table, cur->pagedir, page))
        success = true;
    }

    if (!success)
      exit(-1);
  }
  else
    return;
}

/* SysCall write. */
int write(int fd, const void *buffer, unsigned length)
{
  /* Check the validation of buffer. */
  if (!check_ptr(buffer) || !check_ptr(buffer + length - 1))
    exit(-1);

  /* Write to the STDOUT. */
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

/* SysCall seek. */
void seek(int fd, unsigned position)
{
  struct file *file = get_file(fd);
  if (!file)
    exit(-1);

  thread_acquire_file_lock();
  file_seek(file, position);
  thread_release_file_lock();
}

/* SysCall tell. */
unsigned tell(int fd)
{
  struct file *file = get_file(fd);

  thread_acquire_file_lock();
  unsigned pos = file_tell(file);
  thread_release_file_lock();

  return pos;
}

/* SysCall close. */
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

/* SysCall mmap. */
mmapid_t mmap(int fd, void *data)
{
  if (fd < 2)
    return -1;

  if (!data || pg_ofs(data))
    return -1;

  struct file *file = NULL;
  struct thread_file *f = get_thread_file(fd);
  if (f && f->file)
    file = file_reopen(f->file);

  if (!file)
    return -1;

  int size = file_length(file);
  if (!size)
    return -1;

  for (int ofs = 0; ofs < size; ofs += PGSIZE)
  {
    if (page_table_entry_search(&thread_current()->page_table, data + ofs))
      return -1;
  }

  for (int ofs = 0; ofs < size; ofs += PGSIZE)
  {
    struct page_table_entry *pte = malloc(sizeof(struct page_table_entry));
    if (!pte)
      return -1;
    pte->virtual_page = data + ofs;
    pte->status = MMAP;
    pte->file = file;
    pte->ofs = ofs;
    int zero_bytes = 0;
    int read_bytes = PGSIZE;
    if (ofs + PGSIZE >= size)
    {
      read_bytes = size - ofs;
      zero_bytes = PGSIZE - read_bytes;
    }
    pte->read_bytes = read_bytes;
    pte->zero_bytes = zero_bytes;
    pte->writable = true;
    hash_insert(&thread_current()->page_table, &pte->helem);
  }

  struct mmap_entry *mmap = malloc(sizeof(struct mmap_entry));
  if (!mmap)
    return -1;
  mmap->id = list_empty(&thread_current()->mmap_list) ? 1 : list_entry(list_back(&thread_current()->mmap_list), struct mmap_entry, elem)->id + 1;
  mmap->file = file;
  mmap->va = data;
  list_push_back(&thread_current()->mmap_list, &mmap->elem);

  return mmap->id;
}

/* SysCall munmap. */
void munmap(mmapid_t id)
{
  struct thread *cur = thread_current();
  struct mmap_entry *me = NULL;
  struct list_elem *e;
  if (list_empty(&cur->mmap_list))
    return;
  for (e = list_begin(&cur->mmap_list); e != list_end(&cur->mmap_list); e = list_next(e))
  {
    struct mmap_entry *mmap = list_entry(e, struct mmap_entry, elem);
    if (mmap->id == id)
      me = mmap;
  }

  if (!me)
    return;

  int size = file_length(me->file);
  for (int ofs = 0; ofs < size; ofs += PGSIZE)
  {
    void *addr = me->va + ofs;
    int bytes = PGSIZE;
    if (ofs + PGSIZE >= size)
      bytes = size - ofs;

    struct page_table_entry *pte = page_table_entry_search(&cur->page_table, addr);
    if (!pte)
      continue;
    switch (pte->status)
    {
    case ZERO:
      break;

    case FRAME:
      if (pagedir_is_dirty(cur->pagedir, pte->virtual_page) ||
          pagedir_is_dirty(cur->pagedir, pte->physical_page))
        file_write_at(me->file, pte->virtual_page, bytes, ofs);

      frame_free(pte->physical_page);
      pagedir_clear_page(cur->pagedir, pte->virtual_page);
      break;

    case SWAP:
      if (pagedir_is_dirty(cur->pagedir, pte->virtual_page))
      {
        void *temp_page = palloc_get_page(0);
        swap_in(pte->swap_index, temp_page);
        file_write_at(me->file, temp_page, PGSIZE, ofs);
        palloc_free_page(temp_page);
      }
      break;

    case MMAP:
      break;
    }

    hash_delete(&cur->page_table, &pte->helem);
  }

  list_remove(&me->elem);
  file_close(me->file);
  free(me);
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

  /* If there is no such file, return NULL. */
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