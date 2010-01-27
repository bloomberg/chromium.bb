/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stat function.
 */

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int stat(const char *file, struct stat *st) {
  int retval = NACL_SYSCALL(stat)(file, st);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return 0;
}
