/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


int nacl_dyncode_copy(void *dest, const void *src, size_t size) {
  int retval = NACL_SYSCALL(dyncode_copy)(dest, src, size);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}
