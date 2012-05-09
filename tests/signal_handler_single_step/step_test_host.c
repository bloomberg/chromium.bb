/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <signal.h>

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/tests/signal_handler_single_step/step_test_syscalls.h"


/*
 * This test case checks that NaCl's Linux signal handler can be
 * entered at any point during the context switches between trusted
 * and untrusted code.
 *
 * In particular, this tests that the signal handler correctly
 * restores %gs on x86-32.  This is tricky because NaCl's context
 * switch code must set %cs and %gs separately, so there is a small
 * window during which %gs is set to the untrusted-code value but %cs
 * is not.  The signal handler needs to work at this point if we are
 * to handle asynchronous signals correctly (such as for implementing
 * thread suspension).
 */

static const int kX86TrapFlag = 1 << 8;
/* This should be at least 2 so that we test the syscall return path. */
static const int kNumberOfCallsToTest = 5;

static int g_call_count = 0;
static int g_in_untrusted_code = 0;
static int g_context_switch_count = 0;


#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86

/* Start single-stepping by setting the trap flag (bit 8). */
static void SetTrapFlag() {
  __asm__("pushf\n"
#if NACL_BUILD_SUBARCH == 32
          "btsl $8, (%esp)\n"
#else
          "btsq $8, (%rsp)\n"
#endif
          "popf\n");
}

/* Stop single-stepping by unsetting the trap flag (bit 8). */
static void UnsetTrapFlag() {
  __asm__("pushf\n"
#if NACL_BUILD_SUBARCH == 32
          "btrl $8, (%esp)\n"
#else
          "btrq $8, (%rsp)\n"
#endif
          "popf\n");
}

static uintptr_t GetTrapFlag() {
  uintptr_t flags;
#if NACL_BUILD_SUBARCH == 32
  __asm__("pushf\n"
          "popl %0\n" : "=r"(flags));
#else
  __asm__("pushf\n"
          "popq %0\n" : "=r"(flags));
#endif
  return (flags & kX86TrapFlag) != 0;
}

#else
# error Unsupported architecture
#endif

/*
 * We use a custom NaCl syscall here partly because we need to avoid
 * making any Linux syscalls while the trap flag is set.  On x86-32
 * Linux, doing a syscall with the trap flag set will sometimes kill
 * the process with SIGTRAP rather than entering the signal handler.
 * This might be a kernel bug.  x86-64 processes do not have the same
 * problem.
 */
static int32_t TestSyscall(struct NaClAppThread *natp) {
  NaClCopyInDropLock(natp->nap);

  /* Check that the trap flag has not been unset by anything unexpected. */
  CHECK(GetTrapFlag());

  if (++g_call_count == kNumberOfCallsToTest) {
    UnsetTrapFlag();
    NaClReportExitStatus(natp->nap, 0);
    NaClAppThreadTeardown(natp);
  }
  return 0;
}

static enum NaClSignalResult TrapSignalHandler(int signal, void *ucontext) {
  if (signal == SIGTRAP) {
    struct NaClSignalContext context;
    int is_untrusted;

    NaClSignalContextFromHandler(&context, ucontext);
    is_untrusted = NaClSignalContextIsUntrusted(&context);
    if (g_in_untrusted_code != is_untrusted) {
      g_context_switch_count++;
      g_in_untrusted_code = is_untrusted;
    }
    return NACL_SIGNAL_RETURN;
  } else {
    const char msg[] = "Error: Received unexpected signal\n";
    if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) {
      /*
       * We cannot do anything useful with this error, but glibc's
       * warn_unused_result annotation requires us to check for it.
       */
    }
    _exit(1);
  }
}

static void ThreadCreateHook(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);

  SetTrapFlag();
}

static void ThreadExitHook(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
}

static void ProcessExitHook(int exit_status) {
  UNREFERENCED_PARAMETER(exit_status);
}

static const struct NaClDebugCallbacks debug_callbacks = {
  ThreadCreateHook,
  ThreadExitHook,
  ProcessExitHook
};

int main(int argc, char **argv) {
  struct NaClApp app;
  struct GioMemoryFileSnapshot gio_file;
  struct NaClSyscallTableEntry syscall_table[NACL_MAX_SYSCALLS] = {{0}};
  int index;
  for (index = 0; index < NACL_MAX_SYSCALLS; index++) {
    syscall_table[index].handler = NaClSysNotImplementedDecoder;
  }
  syscall_table[SINGLE_STEP_TEST_SYSCALL].handler = TestSyscall;

  NaClAllModulesInit();

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClFileNameForValgrind(argv[1]);
  CHECK(GioMemoryFileSnapshotCtor(&gio_file, argv[1]));
  CHECK(NaClAppWithSyscallTableCtor(&app, syscall_table));

  app.debug_stub_callbacks = &debug_callbacks;
  NaClSignalHandlerInit();
  NaClSignalHandlerAdd(TrapSignalHandler);

  CHECK(NaClAppLoadFile((struct Gio *) &gio_file, &app) == LOAD_OK);
  CHECK(NaClAppPrepareToLaunch(&app) == LOAD_OK);
  CHECK(NaClCreateMainThread(&app, 0, NULL, NULL));
  CHECK(NaClWaitForMainThreadToExit(&app) == 0);

  CHECK(!g_in_untrusted_code);
  CHECK(g_context_switch_count == kNumberOfCallsToTest * 2);

  /*
   * Avoid calling exit() because it runs process-global destructors
   * which might break code that is running in our unjoined threads.
   */
  NaClExit(0);
  return 0;
}
