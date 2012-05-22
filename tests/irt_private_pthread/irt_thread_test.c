/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"


void __pthread_initialize(void);

__thread int tls_var = 123;

static pthread_t g_initial_thread_id;

/* This flag is set to zero by the IRT's thread_exit() function. */
static int32_t g_thread_flag;


static void wait_for_thread_exit(void) {
  /* Using a mutex+condvar is a hassle to set up, so wait by spinning. */
  while (g_thread_flag != 0) {
    sched_yield();
  }
}

static char *allocate_stack(void) {
  size_t stack_size = 0x10000;
  char *stack = malloc(stack_size);
  assert(stack != NULL);
  /* We assume that the stack grows downwards. */
  return stack + stack_size;
}


static void user_thread_func(void) {
  /* Check that IRT-internal TLS variables work inside user threads. */
  assert(tls_var == 123);
  tls_var = 456;

  /* NULL is not a valid thread ID in our pthreads implementation. */
  assert(pthread_self() != NULL);

  /* pthread_equal() is not available to the IRT at the moment. */
  assert(pthread_self() != g_initial_thread_id);

  nacl_irt_thread.thread_exit(&g_thread_flag);
}

void test_user_thread(void) {
  g_initial_thread_id = pthread_self();
  /* NULL is not a valid thread ID in our pthreads implementation. */
  assert(g_initial_thread_id != NULL);

  g_thread_flag = 1;
  void *dummy_tls = (void *) 0x1234;
  int rc = nacl_irt_thread.thread_create(
      (void *) (uintptr_t) user_thread_func, allocate_stack(), dummy_tls);
  assert(rc == 0);
  wait_for_thread_exit();
  /* The assignment should not have affected our copy of tls_var. */
  assert(tls_var == 123);
}


/* This waits for the initial thread to exit. */
static void initial_thread_exit_helper(void) {
  wait_for_thread_exit();
  _exit(0);
}

void test_exiting_initial_thread(void) {
  g_thread_flag = 1;
  void *dummy_tls = (void *) 0x1234;
  int rc = nacl_irt_thread.thread_create(
      (void *) (uintptr_t) initial_thread_exit_helper,
      allocate_stack(), dummy_tls);
  assert(rc == 0);
  nacl_irt_thread.thread_exit(&g_thread_flag);
  /* Should not reach here. */
  abort();
}


int main(void) {
  printf("Running test_user_thread...\n");
  test_user_thread();

  printf("Running test_exiting_initial_thread...\n");
  test_exiting_initial_thread();
  /* This last test does not return. */
  return 1;
}


void _start(uint32_t *info) {
  __pthread_initialize();
  _exit(main());
}
