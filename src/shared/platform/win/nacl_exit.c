/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * be found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"


void NaClAbort(void) {
  /*
   * We crash the process with a HLT instruction so that the Breakpad
   * crash reporter will be invoked when we are running inside Chrome.
   *
   * This has the disadvantage that an untrusted-code crash will not
   * be distinguishable from a trusted-code NaClAbort() based on the
   * process's exit status alone
   *
   * While we could use the INT3 breakpoint instruction to exit (via
   * __debugbreak()), that does not work if NaCl's debug exception
   * handler is attached, because that always resumes breakpoints (see
   * http://code.google.com/p/nativeclient/issues/detail?id=2772).
   */
  while (1) {
    __halt();
  }
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
