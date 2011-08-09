/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#if defined(IN_BROWSER)
#include <nacl/nacl_srpc.h>
#include "native_client/src/shared/srpc/nacl_srpc_ppapi_plugin_internal.h"
#endif

int main() {
#if defined(IN_BROWSER)
  if (!NaClSrpcModuleInit())
    return -1;
  NaClPluginLowLevelInitializationComplete();
#endif

  fprintf(stderr, "[CRASH_TEST] Causing crash in untrusted code...\n");
  /* We use "volatile" because otherwise LLVM gets too clever (at
     least on ARM) and generates an illegal instruction, which
     generates SIGILL instead of SIGSEGV, and LLVM also optimises away
     the rest of the function. */
  *(volatile int *) 0 = 0;
  fprintf(stderr, "[CRASH_TEST] FAIL: Survived crash attempt\n");
  return 1;
}
