/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/nacl_syscalls.h>
#include <stdarg.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int nanosleep(const struct timespec *req,
              struct timespec *rem) {
  int retval;
  retval = NACL_SYSCALL(nanosleep)(req, rem);
  if (retval < 0) {
    errno = -retval;
    retval = -1;
  }
  return retval;
}
