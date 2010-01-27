/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Wrapper for syscall.
 */

#include <errno.h>
#include <sys/times.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int gettimeofday(struct timeval *tv, void *tz) {
  int retval = NACL_SYSCALL(gettimeofday)(tv, tz);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return 0;
}
