#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "list.h"

#define FAIL_ERROR -1

struct process {
  tid_t tid;

  struct semaphore wait_sema;
  bool failed;
  int return_code;

  struct semaphore children_sema;
  struct list children;
  struct list_elem elem;
};

tid_t process_execute (char *file_name);
void process_init (void);
int process_wait (tid_t);
void process_exit (void);
void process_failed (void);
void process_activate (void);
struct process *process_current (void);


#endif /* userprog/process.h */
