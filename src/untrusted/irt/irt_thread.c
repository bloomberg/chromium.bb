/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_thread_create(void *start_user_address, void *stack,
                                  void *thread_ptr) {
  return -NACL_SYSCALL(thread_create)(start_user_address, stack, thread_ptr, 0);
}

static void nacl_irt_thread_exit(int32_t *stack_flag) {
  NACL_SYSCALL(thread_exit)(stack_flag);
  while (1) (*(void (*)(void)) 0)();  /* Crash.  */
}

static int nacl_irt_thread_nice(const int nice) {
  return -NACL_SYSCALL(thread_nice)(nice);
}

const struct nacl_irt_thread nacl_irt_thread = {
  nacl_irt_thread_create,
  nacl_irt_thread_exit,
  nacl_irt_thread_nice,
};
