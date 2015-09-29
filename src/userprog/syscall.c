#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void*
to_kernel_address(void *addr) {
  return pagedir_get_page (thread_current ()->pagedir, addr);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);

  switch(args[0]) {
    case SYS_EXIT: ;
      int return_code = args[1];
      process_current ()->return_code = return_code;
      char *proc_name = thread_current ()->name;
      printf("%s: exit(%d)\n", proc_name, return_code);
      f->eax = return_code;
      thread_exit();
      break;
    case SYS_WRITE:
      printf("%s", (char *) args[2]);
      f->eax = 0;
      break;
    case SYS_NULL:
      f->eax = args[1] + 1;
      break;
    case SYS_HALT:
      shutdown_power_off ();
      break;
    case SYS_EXEC: ;
      char *cmd_line = to_kernel_address(args[1]);
      tid_t pid = process_execute (cmd_line);
      f->eax = pid;
      break;
    case SYS_WAIT: ;
      int wait_return_code = process_wait(args[1]);
      f->eax = wait_return_code;
      break;
    default:
      printf("Unhandled system call number: %d\n", args[0]);
  }
}

