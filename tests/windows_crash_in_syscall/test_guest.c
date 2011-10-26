/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/tests/windows_crash_in_syscall/test_syscalls.h"


#define UNTYPED_SYSCALL(s) ((int (*)()) NACL_SYSCALL_ADDR(s))


void SimpleAbort() {
  while (1) {
    /* Exit by causing a crash. */
    *(volatile int *) 0 = 0;
  }
}

void EntryPoint() {
  UNTYPED_SYSCALL(CRASHY_TEST_SYSCALL)();
  /* Should not reach here. */
  SimpleAbort();
}
