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

off_t lseek(int desc, off_t offset, int whence) {
  int retval = NACL_SYSCALL(lseek)(desc,
                                   &offset,
                                   whence);
  if ((int) retval < 0) {
    errno = -(int) retval;
    return (off_t) -1;
  }
  return offset;
}
