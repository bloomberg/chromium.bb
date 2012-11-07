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

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/nacl_syscalls.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


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
  int ret;

  unsigned int local_pre_call_count = nacl_pre_calls;
  unsigned int local_post_call_count = nacl_pre_calls;

  /* A set of nonsense arguments to keep from having a bunch
   * of literal values below.
   */
  const int fd = -1;
  void* ptr = NULL;
  const size_t size = 0;

  /* Mutex/semaphore for pthread lib syscalls */
  pthread_mutex_t mutex;
  sem_t sem;

  /* Test all syscalls to make sure we are wrapping all the
   * syscalls we are trying to wrap. We don't care about the
   * args or return values as long as the syscall is made.
   *
   * Only the pthread lib functions sanitize and require valid
   * arguments, so we'll initialize a mutex and semaphore below.
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

  /* Neglecting pthread_cond_* syscalls to avoid details of overhead */

  CHECK_SYSCALL_PRE();
  ret = pthread_mutex_init(&mutex, NULL);
  CHECK_SYSCALL_NOT_WRAPPED();
  CHECK(0 == ret);

  CHECK_SYSCALL_PRE();
  pthread_mutex_lock(&mutex);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  pthread_mutex_unlock(&mutex);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  if (0 == pthread_mutex_trylock(&mutex)) {
    CHECK_SYSCALL_WRAPPED();

    CHECK_SYSCALL_PRE();
    pthread_mutex_unlock(&mutex);
    CHECK_SYSCALL_NOT_WRAPPED();

  } else {
    CHECK_SYSCALL_WRAPPED();
  }

  CHECK_SYSCALL_PRE();
  pthread_mutex_destroy(&mutex);
  CHECK_SYSCALL_NOT_WRAPPED();

  /* Semaphore with value 1 (we're the only user of it) */
  CHECK_SYSCALL_PRE();
  ret = sem_init(&sem, 0, 1);
  CHECK_SYSCALL_NOT_WRAPPED();
  CHECK(0 == ret);

  CHECK_SYSCALL_PRE();
  sem_wait(&sem);
  CHECK_SYSCALL_WRAPPED();

  CHECK_SYSCALL_PRE();
  sem_post(&sem);
  CHECK_SYSCALL_NOT_WRAPPED();

  CHECK_SYSCALL_PRE();
  sem_destroy(&sem);
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
