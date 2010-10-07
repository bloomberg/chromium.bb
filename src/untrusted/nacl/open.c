/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Wrapper for syscall.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/nacl_syscalls.h>
#include <stdarg.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int open(char const *pathname, int flags, ...) {
  int retval;
  va_list ap;
  mode_t mode;
  va_start(ap, flags);
  mode = va_arg(ap, mode_t);
  retval = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(open)(pathname, flags, mode));
  va_end(ap);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}
