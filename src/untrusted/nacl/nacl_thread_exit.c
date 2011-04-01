/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

void nacl_thread_exit(int32_t *stack_flag) {
  (void) NACL_SYSCALL(thread_exit)(stack_flag);
  /*NOTREACHED*/
  abort();
}
