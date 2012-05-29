/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/service_runtime/include/sys/nacl_test_crash.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


int look_up_crash_type(const char *name) {
#define MAP_NAME(type) if (strcmp(name, #type) == 0) { return type; }
  MAP_NAME(NACL_TEST_CRASH_MEMORY);
  MAP_NAME(NACL_TEST_CRASH_LOG_FATAL);
  MAP_NAME(NACL_TEST_CRASH_CHECK_FAILURE);
#undef MAP_NAME

  fprintf(stderr, "Unknown crash type: \"%s\"\n", name);
  exit(1);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <crash-type>\n", argv[0]);
    return 1;
  }
  NACL_SYSCALL(test_crash)(look_up_crash_type(argv[1]));
  return 1;
}
