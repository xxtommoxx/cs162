#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "list.h"
#include "filesys/file.h"

#define FAIL_ERROR -1

#define STDOUT_FD 1
#define STDIN_FD 0

struct file_descriptor {
    uint32_t id;
    struct file *f;
    struct list_elem elem;
};


struct process {
  tid_t tid;

  struct file *executable;

  struct semaphore wait_sema;
  int return_code;

  struct semaphore children_sema;
  struct list children;
  struct list_elem elem;

  struct list files;
  uint32_t current_fd_id;
};

tid_t process_execute (char *file_name);
void process_init (void);
int process_wait (tid_t);
void process_exit (void);
void process_failed (void);
void process_activate (void);
struct process *process_current (void);

void process_remove_fd (uint32_t);
struct file *process_get_file (uint32_t);
uint32_t process_create_fd (struct file*);
#endif /* userprog/process.h */
