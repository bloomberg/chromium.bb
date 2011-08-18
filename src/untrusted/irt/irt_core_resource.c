/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_open_resource(const char *pathname, int *newfd) {
  int rv = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(open)(pathname, O_RDONLY, 0));
  if (rv < 0)
    return -rv;
  *newfd = rv;
  return 0;
}

const struct nacl_irt_resource_open nacl_irt_resource_open = {
  nacl_irt_open_resource,
};
