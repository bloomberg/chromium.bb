/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_IRT_EXT_THREADING_H
#define NATIVE_CLIENT_TESTS_IRT_EXT_THREADING_H

struct threading_environment {
  int num_threads_created;
  int num_threads_exited;
  int last_set_thread_nice;
};

void init_threading_module(void);

void init_threading_environment(struct threading_environment *env);

void activate_threading_env(struct threading_environment *env);
void deactivate_threading_env(void);

#endif /* NATIVE_CLIENT_TESTS_IRT_EXT_THREADING_H */
