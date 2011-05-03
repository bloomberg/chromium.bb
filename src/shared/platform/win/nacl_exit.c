/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * be found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"


void NaClAbort(void) {
  NaClExit(-SIGABRT);
}

void NaClExit(int err_code) {
#ifdef COVERAGE
  /* Give coverage runs a chance to flush coverage data */
  exit(err_code);
#else
  /* If the process is scheduled for termination, wait for it.*/
  if (TerminateProcess(GetCurrentProcess(), err_code)) {
    while (1) {
      (void) SwitchToThread();
    }
  }

  /* Otherwise use the standard C process exit to bybass destructors. */
  ExitProcess(err_code);
#endif
}
