/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Wrapper for syscall.
 */

#include <sys/types.h>
#include <sys/nacl_syscalls.h>
#include <errno.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int read(int desc, void *buf, size_t count) {
  int retval = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(read)(desc, buf, count));
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}
