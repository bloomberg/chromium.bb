/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * gc_instrumentation test: test compiler and syscall instrumentation
 */

#include "native_client/tests/gc_instrumentation/gc_instrumentation.h"
/* TODO(bradchen): fix this include once it is moved to the right place */
#include "native_client/src/untrusted/nacl/gc_hooks.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/nacl_syscalls.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "native_client/src/untrusted/pthread/pthread_internal.h"


/* Cheap way to keep track if we're in the main thread */
__thread int cheap_thread_id;
int* main_thread_id = NULL;


unsigned int nacl_pre_calls = 0;
unsigned int nacl_post_calls = 0;

void nacl_pre_gc_hook(void) {
  if (main_thread_id && (main_thread_id == &cheap_thread_id))
    nacl_pre_calls++;
}

void nacl_post_gc_hook(void) {
  if (main_thread_id && (main_thread_id == &cheap_thread_id))
    nacl_post_calls++;
}

unsigned int num_errors = 0;

#define CHECK(val)                                                        \
  do {                                                                    \
    if (!(val)) {                                                         \
      num_errors++;                                                       \
      printf("CHECK \"" #val "\" failed at %s:%d\n", __FILE__, __LINE__); \
    }                                                                     \
  } while (0)

#define CHECK_SYSCALL_PRE()                              \
  do {                                                   \
    local_pre_call_count = nacl_pre_calls;               \
    local_post_call_count = nacl_post_calls;             \
  } while (0)

#define CHECK_SYSCALL_WRAPPED()                          \
  do {                                                   \
    CHECK(local_pre_call_count == nacl_pre_calls - 1);   \
    CHECK(local_post_call_count == nacl_post_calls - 1); \
  } while (0)

#define CHECK_SYSCALL_NOT_WRAPPED()                      \
  do {                                                   \
    CHECK(local_pre_call_count == nacl_pre_calls);       \
    CHECK(local_post_call_count == nacl_post_calls);     \
  } while (0)


void test_syscall_wrappers(void) {
  /*
   * This tests whether various IRT calls generate
   * blocking-notification callbacks.  The test expectations here are
   * subject to change.  We might need to update them when the IRT or
   * the NaCl trusted runtime are changed.
   *
   * For example, if the IRT's mutex_lock() is always reported as
   * blocking today, it might not be reported as blocking in the
   * uncontended case in the future.
   *
   * Conversely, while the IRT's mutex_unlock() might always be
   * reported as non-blocking today, in a future implementation it
   * might briefly hold a lock to inspect a futex wait queue, which
   * might be reported as blocking.
   *
   * The user-code libpthread implementation is similarly subject to
   * change, but it is one level removed from the IRT interfaces that
   * generate blocking-notification callbacks.  Therefore, we test the
   * IRT interfaces rather than testing pthread_mutex, pthread_cond,
   * etc.
   */

  unsigned int local_pre_call_count = nacl_pre_calls;
  unsigned int local_post_call_count = nacl_pre_calls;

  /* A set of nonsense arguments to keep from having a bunch
   * of literal values below.
   */
  const int fd = -1;
  void* ptr = NULL;
  const size_t size = 0;

  /* Test all syscalls to make sure we are wrapping all the
   * syscalls we are trying to wrap. We don't care about the
   * args or return values as long as the syscall is made.
   */
  CHECK_SYSCALL_PRE();
  read(fd, ptr, size);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  write(fd, ptr, size);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  nacl_dyncode_create(ptr, ptr, size);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  nacl_dyncode_modify(ptr, ptr, size);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  nacl_dyncode_delete(ptr, size);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  nanosleep(ptr, ptr);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  open(ptr, 0, O_RDWR);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  sched_yield();
  CHECK_SYSCALL_WRAPPED();

  /*
   * This initializes __nc_irt_mutex, __nc_irt_cond and __nc_irt_sem
   * as a side effect.
   */
  struct nacl_irt_thread irt_thread;
  __nc_initialize_interfaces(&irt_thread);

  /* Check the IRT's mutex interface */

  int mutex_handle;
  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_mutex.mutex_create(&mutex_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_mutex.mutex_lock(mutex_handle) == 0);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_mutex.mutex_trylock(mutex_handle) == EBUSY);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_mutex.mutex_unlock(mutex_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_mutex.mutex_destroy(mutex_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  /* Check the IRT's condvar interface */

  int cond_handle;
  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_cond.cond_create(&cond_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_cond.cond_signal(cond_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_cond.cond_broadcast(cond_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK(__nc_irt_mutex.mutex_create(&mutex_handle) == 0);
  CHECK(__nc_irt_mutex.mutex_lock(mutex_handle) == 0);
  struct timespec abstime = { 0, 0 };
  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_cond.cond_timed_wait_abs(cond_handle, mutex_handle,
                                          &abstime) == ETIMEDOUT);
  CHECK_SYSCALL_WRAPPED();
  CHECK(__nc_irt_mutex.mutex_unlock(mutex_handle) == 0);
  CHECK(__nc_irt_mutex.mutex_destroy(mutex_handle) == 0);

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_cond.cond_destroy(cond_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  /* Check the IRT's semaphore interface */

  /* Semaphore with value 1 (we're the only user of it) */
  int sem_handle;
  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_sem.sem_create(&sem_handle, 1) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_sem.sem_wait(sem_handle) == 0);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_sem.sem_post(sem_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  CHECK(__nc_irt_sem.sem_destroy(sem_handle) == 0);
  CHECK_SYSCALL_NOT_WRAPPED();
}

/* Make sure the function doesn't inline so we still get a prologue */
void check_func_instrumentation(unsigned int arg) __attribute__((noinline));

void check_func_instrumentation(unsigned int arg) {
  CHECK(arg < thread_suspend_if_needed_count);
}

void test_compiler_instrumentation(void) {
  int i;
  /* Test function prologue instrumentation */
  unsigned int local_thread_suspend_count = thread_suspend_if_needed_count;
  check_func_instrumentation(local_thread_suspend_count);
  /* Check back-branch instrumentation */
  local_thread_suspend_count = thread_suspend_if_needed_count;
  i = 0;
  while (1) {
    /* Break after one iteration */
    if (i > 0)
      break;
    i++;
  }
  CHECK(local_thread_suspend_count < thread_suspend_if_needed_count);
}

int main(int argcc, char *argv[]) {
  main_thread_id = &cheap_thread_id;

  nacl_register_gc_hooks(nacl_pre_gc_hook, nacl_post_gc_hook);
  test_syscall_wrappers();
  test_compiler_instrumentation();
  /* test reregistering hooks */
  nacl_register_gc_hooks(nacl_pre_gc_hook, nacl_post_gc_hook);

  if (0 == num_errors) {
    printf("All tests PASSED!\n");
    exit(0);
  } else {
    printf("One ore more tests FAILED!\n");
    exit(-1);
  }

  return 0;
}
