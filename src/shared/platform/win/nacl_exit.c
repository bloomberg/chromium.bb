/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdlib.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"


void NaClAbort(void) {
#ifdef COVERAGE
  /* Give coverage runs a chance to flush coverage data */
  exit((-SIGABRT) & 0xFF);
#else
  /* Return an 8 bit value for SIGABRT */
  TerminateProcess(GetCurrentProcess(),(-SIGABRT) & 0xFF);
#endif
}

void NaClExit(int err_code) {
#ifdef COVERAGE
  /* Give coverage runs a chance to flush coverage data */
  exit(err_code);
#else
  TerminateProcess(GetCurrentProcess(), err_code);
#endif
}

