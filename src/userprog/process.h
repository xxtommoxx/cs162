#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "list.h"

#define FAIL_ERROR -1

tid_t process_execute (char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process {
  bool terminated;
  struct process *parent;
  struct list children;
  struct list_elem elem;
  uint8_t return_code;
};

#endif /* userprog/process.h */
