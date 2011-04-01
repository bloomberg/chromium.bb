/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int nacl_sem_create(int *sem_handle, int32_t value) {
  int rv = NACL_SYSCALL(sem_create)(value);
  if (rv < 0) return -rv;
  *sem_handle = rv;
  return 0;
}
