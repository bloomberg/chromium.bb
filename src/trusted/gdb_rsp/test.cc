/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/trusted/gdb_rsp/test.h"
#include "native_client/src/trusted/port/platform.h"

//  Mock portability objects
namespace port {
void IPlatform::LogInfo(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}

void IPlatform::LogWarning(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}

void IPlatform::LogError(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}
}  // End of namespace port

int main(int argc, const char *argv[]) {
  int errs = 0;

  (void) argc;
  (void) argv;

  printf("Testing Utils.\n");
  errs += TestUtil();

  printf("Testing ABI.\n");
  errs += TestAbi();

  printf("Testing Packets.\n");
  errs += TestPacket();


  if (errs) printf("FAILED with %d errors.\n", errs);

  return errs;
}

