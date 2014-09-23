/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
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

static void *mutex_thread(void *arg) {
  pthread_mutex_t *mutex = arg;

  int ret = pthread_mutex_lock(mutex);
  if (0 != ret)
    return (void *) ((uintptr_t) ret);

  return (void *) ((uintptr_t) pthread_mutex_unlock(mutex));
}

/* Basic pthread tests. */
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

/* Do futex tests. */
static int do_mutex_test(struct threading_environment *env) {
  if (env->num_futex_wait_calls != 0 || env->num_futex_wake_calls != 0) {
    irt_ext_test_print("do_mutex_test: Threading env not initialized.\n");
    return 1;
  }

  pthread_mutex_t mutex;
  int ret = pthread_mutex_init(&mutex, NULL);
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: pthread_mutex_init failed: %s\n",
                       strerror(ret));
    return 1;
  }

  ret = pthread_mutex_lock(&mutex);
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: pthread_mutex_lock failed: %s\n",
                       strerror(ret));
    return 1;
  }

  /* Obtain current mutex_state and be sure we get it before thread creation. */
  const int locked_mutex_state = mutex.mutex_state;
  __sync_synchronize();

  pthread_t thread_id;
  ret = pthread_create(&thread_id, NULL, mutex_thread, &mutex);
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: pthread_create failed: %s\n",
                       strerror(ret));
    return 1;
  }

  /*
   * Unfortunately in order to test if the futex calls are being called
   * properly, we need to rely on the internals of mutex to wait for the
   * other thread to actually begin waiting on the lock. Without it, there is
   * no way to guarantee that the other thread waiting to be signalled. We are
   * using extra knowledge that mutex_state will be changed when the other
   * thread calls pthread_mutex_lock().
   */
  while (locked_mutex_state == mutex.mutex_state) {
    sched_yield();
  }

  ret = pthread_mutex_unlock(&mutex);
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: pthread_mutex_unlock failed: %s\n",
                       strerror(ret));
    return 1;
  }

  /*
   * Another unfortunate fact, we cannot use pthread_join to check if the
   * thread is done, this will trigger futex calls which will interfere
   * with our test. We will get around this by locking the mutex again to
   * make sure the mutex is at least no longer being used.
   */
  ret = pthread_mutex_lock(&mutex);
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: pthread_mutex_lock failed: %s\n",
                       strerror(ret));
    return 1;
  }

  ret = pthread_mutex_unlock(&mutex);
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: pthread_mutex_unlock failed: %s\n",
                       strerror(ret));
    return 1;
  }

  ret = pthread_mutex_destroy(&mutex);
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: pthread_mutex_destroy failed: %s\n",
                       strerror(ret));
    return 1;
  }

  /*
   * It's also unfortunate that we cannot guarantee how implementations will
   * actually call the futex commands. Even the wait call may or may not be
   * called when the mutex_state transforms (for example, as of this writing
   * our current implementation of nc_mutex.c does an extra iteration of the
   * loop checking if the mutex is waiting on anyone, this makes it likely that
   * that we would skip the futex wait call here). The only guarantee
   * is that when we are waking up the other thread, the main thread
   * will at least "attempt" to wake up the other thread, so we can test for
   * at least 1 wake call.
   */
  if (env->num_futex_wake_calls == 0) {
    irt_ext_test_print("do_mutex_test: pthread_mutex_trylock() futex calls not"
                       " properly intercepted. Expected at least 1 wake"
                       " call.\n");
    return 1;
  }

  void *thread_status = NULL;
  ret = pthread_join(thread_id, &thread_status);
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: pthread_join failed: %s\n",
                       strerror(ret));
    return 1;
  }

  ret = (int) thread_status;
  if (0 != ret) {
    irt_ext_test_print("do_mutex_test: mutex lock thread failed: %s\n",
                       strerror(ret));
    return 1;
  }

  return 0;
}

static const TYPE_thread_test g_thread_tests[] = {
  /* Basic pthread tests. */
  do_thread_creation_test,
  do_thread_priority_test,

  /* Do futex tests. */
  do_mutex_test,
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
