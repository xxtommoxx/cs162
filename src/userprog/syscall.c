#include <debug.h>
#include <stdio.h>
#include <syscall-nr.h>

#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static void validate_user_addr (uint32_t *addr);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void*
to_kernel_address(uint32_t *addr) {
  validate_user_addr (addr);

  void *kernel_page = pagedir_get_page (thread_current ()->pagedir, (void *) addr);

  if (kernel_page != NULL) {
    return kernel_page;
  } else {
    process_failed ();
    NOT_REACHED ();
  }
}

void
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
    case SYS_WRITE: ;
      char *buf = to_kernel_address(args[2]);
      printf("%s", buf);
      f->eax = 0;
      break;
    case SYS_NULL: ;
      validate_user_addr (args + 1);
      f->eax = args[1] + 1;
      break;
    case SYS_HALT: ;
      shutdown_power_off ();
      break;
    case SYS_EXEC: ;
      char *cmd_line = to_kernel_address(args[1]);
      tid_t pid = process_execute (cmd_line);
      f->eax = pid;
      break;
    case SYS_WAIT: ;
      validate_user_addr (args + 1);
      int wait_return_code = process_wait(args[1]);
      f->eax = wait_return_code;
      break;
    default:
      printf("Unhandled system call number: %d\n", args[0]);
  }
}

