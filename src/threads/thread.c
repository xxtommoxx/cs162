#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/fixed-point.h"
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* Ready threads in multi-level queue */
static struct list mlq[MLQ_SIZE + 1];

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

static struct list sleep_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

// the count aka the size of ready_list when mlfqs is disabled
static int ready_count;

static fixed_point_t load_avg;
static void update_load_avg(void);

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority, int nice, fixed_point_t recent_cpu);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

static bool sleep_less (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
static bool priority_less (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

static int get_max_priority_donation (struct thread *a);
static int get_max_priority_donation_helper (struct thread *a, int depth);
static void add_to_ready_list(struct thread *t);

static void update_priority(struct thread *t, void *aux UNUSED);
static void update_recent_cpu(struct thread *t, void *aux UNUSED);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&all_list);
  list_init (&sleep_list);

  if (!thread_mlfqs) {
    list_init (&ready_list);
  } else {
    int i;
    for (i = 0; i <= MLQ_SIZE; i++)
      list_init (&mlq[i]);
  }

  load_avg = fix_int (0);
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT, 0, fix_int(0));
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  ASSERT (intr_get_level () == INTR_OFF);
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  struct list_elem *e;

  for (e = list_begin (&sleep_list); e != list_end (&sleep_list); ) {
    struct thread *t = list_entry(e, struct thread, elem);

    if (timer_elapsed(t->sleep_tick) >= 0) {
      e = list_remove(e);
      add_to_ready_list(t);
    } else {
      break; // list is sorted so stop checking
    }
  }

  if (thread_mlfqs) {
    t->recent_cpu = fix_add(t->recent_cpu, fix_int (1));

    if (timer_ticks() % TIMER_FREQ == 0) {
      update_load_avg();
      thread_foreach (&update_recent_cpu, NULL);
    }

    if (timer_ticks() % 4 == 0) {
      thread_foreach (&update_priority, NULL);
    }
  }


  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE || (!thread_mlfqs && !is_highest_priority(t))) {
    intr_yield_on_return ();
  }
}

static void update_load_avg() {
  // plus 1 since at most one thread could be running
  int running_thread = thread_current () == idle_thread ? 0 : 1;

  load_avg = fix_add (
                      fix_mul (fix_frac (59, 60), load_avg),
                      fix_mul (fix_frac (1, 60), fix_int (ready_count + running_thread))
                     );
}

static void update_priority(struct thread *t, void *aux UNUSED) {
  fixed_point_t recentCoeff = fix_div (t->recent_cpu, fix_int (4));
  fixed_point_t niceCoeff = fix_mul (fix_int (t->nice), fix_int (2));

  int newPriority = fix_trunc (fix_sub (fix_sub (fix_int (PRI_MAX), recentCoeff), niceCoeff));

  if (newPriority < PRI_MIN)
    newPriority = PRI_MIN;
  else if (newPriority > PRI_MAX)
    newPriority = PRI_MAX;

  ASSERT (newPriority >= PRI_MIN && newPriority <= PRI_MAX);

  if (t != idle_thread && t->status == THREAD_READY && newPriority != t->priority) {
    list_remove (&t->elem);
    list_push_back (&mlq[newPriority], &t->elem);
  }

  t->priority = newPriority;
}

static void update_recent_cpu(struct thread *t, void *aux UNUSED) {
  fixed_point_t coeff = fix_div (fix_mul (fix_int (2), load_avg),
                                 fix_add (fix_mul (fix_int (2), load_avg), fix_int (1)));
  t->recent_cpu = fix_add (fix_mul (coeff, t->recent_cpu), fix_int (t->nice));
}

bool
is_highest_priority(struct thread *t) {
  ASSERT(!thread_mlfqs);

  if (!list_empty (&ready_list)) {
    struct thread *head = list_entry (list_front(&ready_list), struct thread, elem);
    return get_max_priority_donation(t) >= get_max_priority_donation(head);
  }
  return true;
}

static void
add_to_ready_list(struct thread *t) {
  ASSERT (intr_get_level () == INTR_OFF);

  if (!thread_mlfqs) {
    list_push_back (&ready_list, &t->elem);
    list_sort(&ready_list, &donation_less, NULL);
  } else {
    list_push_back (&mlq[t->priority], &t->elem);
  }

  ready_count++;
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */

tid_t
thread_create_process (const char *name, struct process *proc,
                       int priority, thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority, thread_current ()-> nice, thread_current ()->recent_cpu);
  tid = t->tid = allocate_tid ();
  t->proc = proc;

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  if (!thread_mlfqs && !is_highest_priority (thread_current ()))
      thread_yield();

  return tid;


}
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{
  return thread_create_process(name, thread_current ()->proc, priority, function, aux);
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  add_to_ready_list(t);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    add_to_ready_list (cur);

  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

void
thread_sleep (int64_t ticks) {
  ASSERT (!intr_context ());

  enum intr_level old_level;
  old_level = intr_disable ();

  struct thread *cur = thread_current ();

  cur->status = THREAD_BLOCKED;
  cur->sleep_tick = timer_ticks() + ticks;
  list_push_back (&sleep_list, &cur->elem);

  list_sort (&sleep_list, &sleep_less, NULL);

  schedule();

  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority)
{
  struct thread *curr = thread_current();
  curr->priority = new_priority;

  // a lock is not needed since when schedule interrupts are turned off and the greatest
  // priority will eventually be scheduled
  if (!thread_mlfqs && !is_highest_priority (curr))
    thread_yield();
}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  if (!thread_mlfqs)
    return get_max_priority_donation(thread_current ());
  else
    return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice)
{
  thread_current ()->nice = nice;
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  return thread_current ()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  fixed_point_t times_100 = fix_mul (load_avg, fix_int (100));
  return fix_round (times_100);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  return fix_trunc (fix_mul (thread_current ()->recent_cpu, fix_int(100)));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority, int nice, fixed_point_t recent_cpu)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->nice = nice;
  t->recent_cpu = recent_cpu;
  t->magic = THREAD_MAGIC;
  list_init (&t->waiting_thread_list);

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

void
thread_lock_wait_acquired (struct thread *t) {
  ASSERT(!thread_mlfqs)
  list_remove (&t->wait_elem);
}

void
thread_lock_wait_added (struct lock *lock) {
  ASSERT(lock->holder != NULL);
  ASSERT(!thread_mlfqs)

  list_push_front(&lock->holder->waiting_thread_list, &thread_current ()->wait_elem);
  list_sort(&lock->holder->waiting_thread_list, &priority_less, NULL);

  list_sort(&ready_list, &donation_less, NULL);
}


/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  if (ready_count == 0) {
    return idle_thread;
  }
  else {
    ready_count--;

    if (!thread_mlfqs) {
      return list_entry (list_pop_front (&ready_list), struct thread, elem);
    } else {
      int i;
      for ( i = PRI_MAX; i >= PRI_MIN; i--) {
        if (!list_empty (&mlq[i])) {
          return list_entry (list_pop_front (&mlq[i]), struct thread, elem);
        }
      }

      // panic since something is seriously wrong
      ASSERT(false);

      return NULL;
    }
  }
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);

      palloc_free_page (prev);
    }

}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  // one is for main thread lock_acquire uses malloc which is not setup yet
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Sort by priority donation
 * If a thread is holding a lock where any of the waiting threads has a
 * higher priority than itself then  use the max of that.
*/
bool
donation_less (const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED)
{
  struct thread *a = list_entry (a_, struct thread, elem);
  struct thread *b = list_entry (b_, struct thread, elem);
  return get_max_priority_donation(a) > get_max_priority_donation(b);
}

static int get_max_priority_donation (struct thread *a) {
  ASSERT(!thread_mlfqs)
  return get_max_priority_donation_helper(a, 0);
}

static
int get_max_priority_donation_helper (struct thread *a, int depth) {
  ASSERT(depth < 10);

  int t_priority = a->priority;
  int t_donation = PRI_MIN;

  struct list *wait_list = &a->waiting_thread_list;

  if (!list_empty (wait_list)) {
    // wait sorted descending priority order
    struct thread *wait_t = list_entry (list_begin (wait_list), struct thread, wait_elem);
    int wait_max_priority = get_max_priority_donation_helper(wait_t, depth+1);
    t_donation = t_donation > wait_max_priority ? t_donation : wait_max_priority;
  }

  return t_priority > t_donation ? t_priority : t_donation;
}

/* Sort fn based on time_sleep_ticks */
static bool
sleep_less (const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED)
{
  const struct thread *a = list_entry (a_, struct thread, elem);
  const struct thread *b = list_entry (b_, struct thread, elem);

  return a->sleep_tick < b->sleep_tick;
}

/* Sort fn based on priority */
bool
priority_less (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
  const struct thread *a = list_entry (a_, struct thread, elem);
  const struct thread *b = list_entry (b_, struct thread, elem);

  return a->priority > b->priority;
}


/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
