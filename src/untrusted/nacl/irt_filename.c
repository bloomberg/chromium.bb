/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_open(const char *pathname, int oflag, mode_t cmode,
                         int *newfd) {
  int rv = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(open)(pathname, oflag, cmode));
  if (rv < 0)
    return -rv;
  *newfd = rv;
  return 0;
}

static int nacl_irt_stat(const char *pathname, struct stat *st) {
  return -NACL_SYSCALL(stat)(pathname, st);
}

const struct nacl_irt_filename nacl_irt_filename = {
  nacl_irt_open,
  nacl_irt_stat,
};
