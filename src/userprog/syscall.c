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
    case SYS_EXIT:
      f->eax = args[1];
      struct thread *curr = thread_current ();
      printf("%s: exit(%d)\n", curr->name, args[1]);
      thread_exit();
      break;
    case SYS_WRITE:
      printf("%s", args[2]);
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
    default:
      printf("Unhandled system call number: %d\n", args[0]);
  }
}

