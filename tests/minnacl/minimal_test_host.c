/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/tests/minnacl/minimal_test_syscalls.h"


struct NaClSyscallTableEntry nacl_syscall[NACL_MAX_SYSCALLS] = {{0}};

static int32_t NotImplementedDecoder(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  printf("Error: entered an unexpected syscall!\n");
  fflush(stdout);
  _exit(1);
}

static int32_t MySyscallInvoke(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  printf("Inside custom test 'invoke' syscall\n");
  fflush(stdout);
  /* Return a value that the test guest program checks for. */
  return 123;
}

static int32_t MySyscallExit(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  printf("Inside custom test 'exit' syscall\n");
  fflush(stdout);
  _exit(0);
}

/*
 * This overrides the function in service_runtime, which is somewhat hacky.
 * TODO(mseaborn): It would be cleaner if we could have per-sandbox syscall
 * tables.
 */
void NaClSyscallTableInit() {
  int index;
  for (index = 0; index < NACL_MAX_SYSCALLS; index++) {
    nacl_syscall[index].handler = NotImplementedDecoder;
  }
  nacl_syscall[TEST_SYSCALL_INVOKE].handler = MySyscallInvoke;
  nacl_syscall[TEST_SYSCALL_EXIT].handler = MySyscallExit;
}


int main(int argc, char **argv) {
  struct NaClApp app;
  struct GioMemoryFileSnapshot gio_file;
  /* sel_ldr_standard.c currently requires at least 1 argument. */
  char *untrusted_argv[] = {"blah"};

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClAllModulesInit();

  NaClFileNameForValgrind(argv[1]);
  if (GioMemoryFileSnapshotCtor(&gio_file, argv[1]) == 0) {
    NaClLog(LOG_FATAL, "GioMemoryFileSnapshotCtor() failed\n");
  }

  if (!NaClAppCtor(&app)) {
    NaClLog(LOG_FATAL, "NaClAppCtor() failed\n");
  }

  if (NaClAppLoadFile((struct Gio *) &gio_file, &app) != LOAD_OK) {
    NaClLog(LOG_FATAL, "NaClAppLoadFile() failed\n");
  }

  if (NaClAppPrepareToLaunch(&app) != LOAD_OK) {
    NaClLog(LOG_FATAL, "NaClAppPrepareToLaunch() failed\n");
  }

  /*
   * TODO(mseaborn): It would be nice if we did not have to create a
   * separate thread for running the sandboxed code.
   */
  if (!NaClCreateMainThread(&app, 1, untrusted_argv, NULL)) {
    NaClLog(LOG_FATAL, "NaClCreateMainThread() failed\n");
  }

  NaClWaitForMainThreadToExit(&app);

  NaClLog(LOG_FATAL, "The exit syscall is not supposed to be callable\n");

  return 1;
}
