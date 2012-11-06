/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int nacl_nameservice(int *desc_in_out) {
  int retval = NACL_SYSCALL(nameservice)(desc_in_out);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return 0;
}
