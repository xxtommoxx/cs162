#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"



void acquire_file_lock (void);
void release_file_lock (void);
void syscall_init (void);

#endif /* userprog/syscall.h */
