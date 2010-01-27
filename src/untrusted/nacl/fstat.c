/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stub routine for fstat for newlib support.
 */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int fstat(int file, struct stat *st) {
  int retval;
  retval = NACL_SYSCALL(fstat)(file, st);
  if (retval < 0) {
    errno = -retval;
    retval = -1;
  }
  return retval;
}
