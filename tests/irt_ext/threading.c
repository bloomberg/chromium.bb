/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_extension.h"
#include "native_client/tests/irt_ext/error_report.h"
#include "native_client/tests/irt_ext/threading.h"

static struct threading_environment *g_activated_env = NULL;
static const struct threading_environment g_empty_env = { 0 };
static struct nacl_irt_thread g_irt_thread = { NULL };

static int my_thread_create(void (*start_func)(void), void *stack,
                            void *thread_ptr) {
  if (g_activated_env) {
    g_activated_env->num_threads_created++;
  }

  return g_irt_thread.thread_create(start_func, stack, thread_ptr);
}

static void my_thread_exit(int32_t *stack_flag) {
  if (g_activated_env) {
    g_activated_env->num_threads_exited++;
  }

  g_irt_thread.thread_exit(stack_flag);
}

static int my_thread_nice(const int nice) {
  if (g_activated_env) {
    g_activated_env->last_set_thread_nice = nice;
  }

  return g_irt_thread.thread_nice(nice);
}

void init_threading_module(void) {
  size_t bytes = nacl_interface_query(NACL_IRT_THREAD_v0_1,
                                      &g_irt_thread, sizeof(g_irt_thread));
  IRT_EXT_ASSERT_MSG(bytes == sizeof(g_irt_thread),
                     "Could not query interface: " NACL_IRT_THREAD_v0_1);

  struct nacl_irt_thread thread_calls = {
    my_thread_create,
    my_thread_exit,
    my_thread_nice,
  };

  bytes = nacl_interface_ext_supply(NACL_IRT_THREAD_v0_1,
                                    &thread_calls, sizeof(thread_calls));
  IRT_EXT_ASSERT_MSG(bytes == sizeof(thread_calls),
                     "pthreads may not be linked in."
                     " Could not supply interface: " NACL_IRT_THREAD_v0_1);
}

void init_threading_environment(struct threading_environment *env) {
  *env = g_empty_env;
}

void activate_threading_env(struct threading_environment *env) {
  g_activated_env = env;
}

void deactivate_threading_env(void) {
  g_activated_env = NULL;
}
