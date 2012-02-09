/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

/*
 * This test verifies that if exceptions have not been enabled, that all of
 * the exception related syscalls return ENOSYS.
 */

int main() {
  int rc;

  rc = NACL_SYSCALL(exception_handler)(NULL, NULL);
  assert(rc == -ENOSYS);

  rc = NACL_SYSCALL(exception_stack)(NULL, 1024);
  assert(rc == -ENOSYS);

  rc = NACL_SYSCALL(exception_clear_flag)();
  assert(rc == -ENOSYS);

  fprintf(stderr, "** intended_exit_status=0\n");
  return 0;
}
