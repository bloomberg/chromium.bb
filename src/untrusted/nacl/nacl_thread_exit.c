/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"

void nacl_thread_exit(int32_t *stack_flag) {
  (void) __libnacl_irt_thread.thread_exit(stack_flag);
  while (1) (*(void (*)(void)) 0)();  /* Crash.  */
}
