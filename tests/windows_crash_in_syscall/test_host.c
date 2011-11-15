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
#include "native_client/tests/windows_crash_in_syscall/test_syscalls.h"

/*
 * This test case checks that an exception handler registered via
 * Windows' SetUnhandledExceptionFilter() API gets called if a crash
 * occurs inside a NaCl syscall handler.  On x86-64, we have to ensure
 * that the stack is set up in a way that Windows likes, otherwise
 * this exception handler does not get called.  For background, see
 * http://code.google.com/p/nativeclient/issues/detail?id=2237.
 */


static int32_t NotImplementedDecoder(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  printf("Error: entered an unexpected syscall!\n");
  fflush(stdout);
  _exit(1);
}

static int32_t CrashyTestSyscall(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  printf("Inside custom test 'invoke' syscall\n");
  fflush(stdout);

  /* Cause a crash.  This should cause ExceptionHandler() to be called. */
  *(volatile int *) 0 = 0;

  /* Should not reach here. */
  _exit(1);
}

static LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *exc_info) {
  printf("Inside exception handler, as expected\n");
  fflush(stdout);
  /*
   * Continuing is what Breakpad does, but this should cause the
   * process to exit with an exit status that is appropriate for the
   * type of exception.  We want to test that ExceptionHandler() does
   * not get called twice, since that does not work with Chrome's
   * embedding of Breakpad.
   */
  return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv) {
  struct NaClApp app;
  struct GioMemoryFileSnapshot gio_file;
  /* sel_ldr_standard.c currently requires at least 1 argument. */
  char *untrusted_argv[] = {"blah"};
  struct NaClSyscallTableEntry syscall_table[NACL_MAX_SYSCALLS] = {{0}};
  int index;
  for (index = 0; index < NACL_MAX_SYSCALLS; index++) {
    syscall_table[index].handler = NotImplementedDecoder;
  }
  syscall_table[CRASHY_TEST_SYSCALL].handler = CrashyTestSyscall;

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClAllModulesInit();

  NaClFileNameForValgrind(argv[1]);
  if (GioMemoryFileSnapshotCtor(&gio_file, argv[1]) == 0) {
    NaClLog(LOG_FATAL, "GioMemoryFileSnapshotCtor() failed\n");
  }

  if (!NaClAppWithSyscallTableCtor(&app, syscall_table)) {
    NaClLog(LOG_FATAL, "NaClAppCtor() failed\n");
  }

  if (NaClAppLoadFile((struct Gio *) &gio_file, &app) != LOAD_OK) {
    NaClLog(LOG_FATAL, "NaClAppLoadFile() failed\n");
  }

  if (NaClAppPrepareToLaunch(&app) != LOAD_OK) {
    NaClLog(LOG_FATAL, "NaClAppPrepareToLaunch() failed\n");
  }

  SetUnhandledExceptionFilter(ExceptionHandler);

  if (!NaClCreateMainThread(&app, 1, untrusted_argv, NULL)) {
    NaClLog(LOG_FATAL, "NaClCreateMainThread() failed\n");
  }

  NaClWaitForMainThreadToExit(&app);

  NaClLog(LOG_FATAL, "The exit syscall is not supposed to be callable\n");

  return 1;
}
