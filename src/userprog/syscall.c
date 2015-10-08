#include <debug.h>
#include <stdio.h>
#include <syscall-nr.h>

#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "kernel/console.h"

#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

#include "devices/shutdown.h"


/*
 * TODO: Verify ranges for char pointers.
 */

static void syscall_handler (struct intr_frame *);
static void validate_user_addr (uint32_t *addr);
static struct lock file_lock;

void
syscall_init (void)
{
  lock_init (&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
acquire_file_lock (void)
{
  if (!lock_held_by_current_thread (&file_lock)) {
    lock_acquire (&file_lock);
  }
}

void
release_file_lock (void)
{
    if (lock_held_by_current_thread (&file_lock)) {
        lock_release (&file_lock);
    }
}

static void*
to_kernel_address (void *addr) {
  validate_user_addr (addr);

  void *kernel_page = pagedir_get_page (thread_current ()->pagedir, addr);

  if (kernel_page != NULL) {
    return kernel_page;
  } else {
    process_failed ();
    NOT_REACHED ();
  }
}

static void
validate_user_addr_range (char *addr, int upto) {

  int i;

  for (i = 0; i < upto; i++) {
    validate_user_addr((void*) (addr + i));
  }
}


static void
validate_user_addr (uint32_t *addr) {
  if (!is_user_vaddr ((void *) addr)) {
    process_failed ();
    NOT_REACHED ();
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{

  to_kernel_address (f->esp);

  uint32_t *args = f->esp;

  switch(args[0]) {
    case SYS_EXIT: ;
      validate_user_addr (args + 1);
      int return_code = args[1];
      process_current ()->return_code = return_code;
      char *proc_name = thread_current ()->name;
      printf("%s: exit(%d)\n", proc_name, return_code);
      f->eax = return_code;
      thread_exit();
      break;
    case SYS_NULL: ;
      validate_user_addr (args + 1);
      f->eax = args[1] + 1;
      break;
    case SYS_HALT: ;
      shutdown_power_off ();
      break;
    case SYS_EXEC: ;
      char *cmd_line = to_kernel_address ((void *)args[1]);
      tid_t pid = process_execute (cmd_line);
      f->eax = pid;
      break;
    case SYS_WAIT: ;
      validate_user_addr (args + 1);
      int wait_return_code = process_wait (args[1]);
      f->eax = wait_return_code;
      break;
    case SYS_CREATE: ;
      char *file = to_kernel_address((void *)args[1]);
      off_t init_size = args[2];
      acquire_file_lock();
      bool create_success = filesys_create (file, init_size);
      lock_release (&file_lock);
      f->eax = create_success;
      break;
    case SYS_REMOVE: ;
      char *remove_file = to_kernel_address ((void *)args[1]);
      acquire_file_lock();
      bool remove_success = filesys_remove (remove_file);
      lock_release (&file_lock);
      f->eax = remove_success;
      break;
    case SYS_OPEN: ;
      char *open_file = to_kernel_address ((void *)args[1]);
      acquire_file_lock();
      struct file *f1 = filesys_open (open_file);

      if (f1 == NULL) {
        f->eax = -1;
      } else {
        f->eax = process_create_fd (f1);
      }
      lock_release (&file_lock);
      break;
    case SYS_FILESIZE: ;
      struct file *f2 = process_get_file (args[1]);

      acquire_file_lock();
      if (f2 == NULL) {
        f->eax = -1;
      } else {
        f->eax = file_length (f2);
      }
      lock_release (&file_lock);
      break;
    case SYS_READ: ;
      int read_fd = args[1];

      if (read_fd == STDIN_FD) {
        f->eax = input_getc ();
      } else {
        acquire_file_lock();
        struct file *f3 = process_get_file (read_fd);

        if (f3 == NULL) {
          f->eax = -1;
        } else {
          char *buf = (char *) args[2];
          validate_user_addr_range (buf, args[3]);
          f->eax = file_read (f3, (void *) buf, args[3]);
        }

        lock_release (&file_lock);
      }
      break;
    case SYS_WRITE: ;
      acquire_file_lock();

      int write_fd = args[1];
      char *write_buf = to_kernel_address ((void*) args[2]);

      if (write_fd == STDOUT_FD) {
        putbuf (write_buf, args[3]);
        f->eax = args[3];
      } else {
        acquire_file_lock();
        struct file *f4 = process_get_file (write_fd);

        if (f4 == NULL) {
          f->eax = -1;
        } else {
          f->eax =  file_write (f4, write_buf, args[3]);
        }
      }

      lock_release (&file_lock);
      break;

    case SYS_SEEK: ;
      acquire_file_lock();
      int seek_fd = args[1];
      struct file *f5 = process_get_file (seek_fd);

      if (f5 != NULL) {
        file_seek (f5, args[2]);
      }

      lock_release (&file_lock);
      break;
    case SYS_CLOSE: ;
      acquire_file_lock();
      process_remove_fd (args[1]);
      lock_release (&file_lock);
      break;

    case SYS_TELL: ;
      acquire_file_lock();
      int tell_fd = args[1];
      struct file *f6 = process_get_file (tell_fd);

      if (f6 == NULL) {
        f->eax = -1;
      } else {
        f->eax = file_tell (f6);
      }

      lock_release (&file_lock);
      break;
    default:
      printf("Unhandled system call number: %d\n", args[0]);
  }
}



