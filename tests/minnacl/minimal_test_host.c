/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/tests/minnacl/minimal_test_syscalls.h"

static int32_t NotImplementedDecoder(struct NaClAppThread *natp) {
  NaClCopyInDropLock(natp->nap);
  printf("Error: entered an unexpected syscall!\n");
  fflush(stdout);
  _exit(1);
}

static int32_t MySyscallInvoke(struct NaClAppThread *natp) {
  NaClCopyInDropLock(natp->nap);
  printf("Inside custom test 'invoke' syscall\n");
  fflush(stdout);
  /* Return a value that the test guest program checks for. */
  return 123;
}

static int32_t MySyscallExit(struct NaClAppThread *natp) {
  NaClCopyInDropLock(natp->nap);
  printf("Inside custom test 'exit' syscall\n");
  fflush(stdout);
  _exit(0);
}

int main(int argc, char **argv) {
  struct NaClApp app;
  struct GioMemoryFileSnapshot gio_file;
  struct NaClSyscallTableEntry syscall_table[NACL_MAX_SYSCALLS] = {{0}};
  int index;
  for (index = 0; index < NACL_MAX_SYSCALLS; index++) {
    syscall_table[index].handler = NotImplementedDecoder;
  }
  syscall_table[TEST_SYSCALL_INVOKE].handler = MySyscallInvoke;
  syscall_table[TEST_SYSCALL_EXIT].handler = MySyscallExit;

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClAllModulesInit();

  NaClFileNameForValgrind(argv[1]);
  CHECK(GioMemoryFileSnapshotCtor(&gio_file, argv[1]));
  CHECK(NaClAppWithSyscallTableCtor(&app, syscall_table));
  CHECK(NaClAppLoadFile((struct Gio *) &gio_file, &app) == LOAD_OK);
  CHECK(NaClAppPrepareToLaunch(&app) == LOAD_OK);
  /*
   * TODO(mseaborn): It would be nice if we did not have to create a
   * separate thread for running the sandboxed code.
   */
  CHECK(NaClCreateMainThread(&app, 0, NULL, NULL));
  NaClWaitForMainThreadToExit(&app);

  NaClLog(LOG_FATAL, "The exit syscall is not supposed to be callable\n");

  return 1;
}
