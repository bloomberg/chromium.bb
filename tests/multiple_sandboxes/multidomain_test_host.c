/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This test program creates two NaCl sandboxes within the same host process.
 */

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


#define SEND_DESC 3
#define RECEIVE_DESC 3


int main(int argc, char **argv) {
  struct NaClApp app[2];
  struct GioMemoryFileSnapshot gio_file;
  NaClHandle handle_pair[2];
  int i;
  char *domain1_args[] = {"prog", "domain1"};
  char *domain2_args[] = {"prog", "domain2"};
  int return_code;

  if (argc != 2)
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");

  NaClAllModulesInit();

  /* Enable signal handling to get more information in the event of a crash. */
  NaClSignalHandlerInit();

  NaClFileNameForValgrind(argv[1]);
  CHECK(GioMemoryFileSnapshotCtor(&gio_file, argv[1]));

  for (i = 0; i < 2; i++) {
    CHECK(NaClAppCtor(&app[i]));

    /* Use a smaller guest address space size, because 32-bit Windows
       does not let us allocate 2GB of address space.  We don't do this
       for x86-64 because there is an assertion in NaClAllocateSpace()
       that requires 4GB. */
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
    app[i].addr_bits = 29; /* 512MB per process */
#endif

    CHECK(NaClAppLoadFile((struct Gio *) &gio_file, &app[i]) == LOAD_OK);
    NaClAppInitialDescriptorHookup(&app[i]);
    CHECK(NaClAppPrepareToLaunch(&app[i]) == LOAD_OK);
  }

  /* Set up an IMC connection between the two guests.  This allows us
     to test communications between the two and also synchronise the
     output for the purpose of checking against the golden file. */
  CHECK(NaClSocketPair(handle_pair) == 0);
  NaClAddImcHandle(&app[0], handle_pair[0], SEND_DESC);
  NaClAddImcHandle(&app[1], handle_pair[1], RECEIVE_DESC);

  CHECK(NaClCreateMainThread(&app[0], 2, domain1_args, NULL));
  CHECK(NaClCreateMainThread(&app[1], 2, domain2_args, NULL));

  return_code = NaClWaitForMainThreadToExit(&app[0]);
  CHECK(return_code == 1001);
  return_code = NaClWaitForMainThreadToExit(&app[1]);
  CHECK(return_code == 1002);

  /*
   * Avoid calling exit() because it runs process-global destructors
   * which might break code that is running in our unjoined threads.
   */
  NaClExit(0);
}
