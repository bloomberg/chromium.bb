/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "native_client/tests/irt_ext/libc/libc_test.h"
#include "native_client/tests/irt_ext/threading.h"

typedef int (*TYPE_thread_test)(struct threading_environment *env);

static void *nop_thread(void *arg) {
  return NULL;
}

static void *priority_thread(void *arg) {
  const int prio = (int) arg;
  pthread_t this_thread = pthread_self();
  pthread_setschedprio(this_thread, prio);
  return NULL;
}

static int do_thread_creation_test(struct threading_environment *env) {
  if (env->num_threads_created != 0 || env->num_threads_exited != 0) {
    irt_ext_test_print("do_thread_creation_test: Env is not initialized.\n");
    return 1;
  }

  pthread_t thread_id;
  int ret = pthread_create(&thread_id, NULL, nop_thread, NULL);
  if (0 != ret) {
    irt_ext_test_print("do_thread_creation_test: pthread_create failed: %s\n",
                       strerror(ret));
    return 1;
  }

  ret = pthread_join(thread_id, NULL);
  if (0 != ret) {
    irt_ext_test_print("do_thread_creation_test: pthread_join failed: %s\n",
                       strerror(ret));
    return 1;
  }

  if (env->num_threads_created != 1) {
    irt_ext_test_print("do_thread_creation_test: Thread Creation was not"
                       " recorded in the environment.\n");
    return 1;
  }

  if (env->num_threads_exited != 1) {
    irt_ext_test_print("do_thread_creation_test: Thread Exit was not"
                       " recorded in the environment.\n");
    return 1;
  }

  return 0;
}

static int do_thread_priority_test(struct threading_environment *env) {
  if (env->last_set_thread_nice != 0) {
    irt_ext_test_print("do_thread_priority_test: Env is not initialized.\n");
    return 1;
  }

  pthread_t thread_id;
  int ret = pthread_create(&thread_id, NULL, priority_thread,
                           (void *) NICE_REALTIME);
  if (0 != ret) {
    irt_ext_test_print("do_thread_priority_test: pthread_create failed: %s\n",
                       strerror(ret));
    return 1;
  }

  ret = pthread_join(thread_id, NULL);
  if (0 != ret) {
    irt_ext_test_print("do_thread_priority_test: pthread_join failed: %s\n",
                       strerror(ret));
    return 1;
  }

  if (env->last_set_thread_nice != NICE_REALTIME) {
    irt_ext_test_print("do_thread_priority_test: Thread Priority was not"
                       " recorded in the environment.\n");
    return 1;
  }

  return 0;
}

static const TYPE_thread_test g_thread_tests[] = {
  /* Basic pthread tests. */
  do_thread_creation_test,
  do_thread_priority_test,
};

static void setup(struct threading_environment *env) {
  init_threading_environment(env);
  activate_threading_env(env);
}

static void teardown(void) {
  deactivate_threading_env();
}

DEFINE_TEST(Thread, g_thread_tests, struct threading_environment,
            setup, teardown)
