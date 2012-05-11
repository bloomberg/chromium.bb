/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/thread_suspension/suspend_test.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


static void MutatorThread(struct SuspendTestShm *test_shm) {
  uint32_t next_val = 0;
  while (!test_shm->should_exit) {
    test_shm->var = next_val++;
  }
}

static void SyscallInvokerThread(struct SuspendTestShm *test_shm) {
  uint32_t next_val = 0;
  while (!test_shm->should_exit) {
    NACL_SYSCALL(null)();
    test_shm->var = next_val++;
  }
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Expected 2 arguments: <test-type> <memory-address>\n");
    return 1;
  }
  char *test_type = argv[1];

  char *end;
  struct SuspendTestShm *test_shm =
      (struct SuspendTestShm *) strtoul(argv[2], &end, 0);
  assert(*end == '\0');

  if (strcmp(test_type, "MutatorThread") == 0) {
    MutatorThread(test_shm);
  } else if (strcmp(test_type, "SyscallInvokerThread") == 0) {
    SyscallInvokerThread(test_shm);
  } else {
    fprintf(stderr, "Unknown test type: %s\n", test_type);
    return 1;
  }
  return 0;
}
