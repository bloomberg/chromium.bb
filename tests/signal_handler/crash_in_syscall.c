/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <sys/nacl_syscalls.h>

#if defined(IN_BROWSER)
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_ppapi_plugin_internal.h"
#endif

int main() {
  /*
   * There are two possible kinds of fault in trusted code:
   *  1) Faults legitimately triggered by untrusted code.  These are
   *     safe and do not indicate a bug.
   *  2) Faults caused by bugs in trusted code.
   *
   * Currently the signal handler does not distinguish between the
   * two.  Ultimately, we do want to distinguish between them in order
   * to avoid misleading bug reports.
   * See http://code.google.com/p/nativeclient/issues/detail?id=579
   *
   * Below, we trigger a memory access fault in trusted code, inside a
   * syscall handler.  The imc_recvmsg() implementation does a
   * memcpy() from the address we give it.  This is an instance of
   * (1).  If we later extend the system to distinguish (1) and (2),
   * we will want to add a separate test case for (2).
   *
   * We use 0x1000 because we know that is unmapped.  (If this changes
   * and the call fails to fault, the test runner will catch that.)
   * We don't use NULL because the syscall checks for NULL.
   */
  fprintf(stderr, "Entered main\n"); fflush(stderr);
#if defined(IN_BROWSER)
  if (!NaClSrpcModuleInit())
    return -1;
  NaClPluginLowLevelInitializationComplete();
#endif

  fprintf(stderr, "About to crash\n"); fflush(stderr);
  int rc = imc_recvmsg(0, (struct NaClImcMsgHdr *) 0x1000, 0);
  if (rc < 0) {
    perror("imc_recvmsg");
  }
  printf("We do not expect to reach here.\n");
  return 1;
}
