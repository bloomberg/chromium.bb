/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * dup and dup2 functions.
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int dup(int oldfd) {
  int retval = NACL_SYSCALL(dup)(oldfd);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}

int dup2(int oldfd, int newfd) {
  int retval = NACL_SYSCALL(dup2)(oldfd, newfd);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}
