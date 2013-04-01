/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <signal.h>
#include <string.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/load_file.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension_unwind.h"
#include "native_client/tests/common/register_set.h"
#include "native_client/tests/signal_handler_single_step/step_test_common.h"


/*
 * This test case checks that NaCl thread suspension reports the
 * correct untrusted register values for a thread at all points during
 * the context switches between trusted and untrusted code for
 * handling NaCl syscalls.
 *
 * This is similar to step_test_host.c, except that:
 *
 *  1) step_test_host.c merely checks that NaCl's Linux signal handler
 *     can be entered successfully during context switches.  It does
 *     not check register values comprehensively.
 *
 *  2) Unlike step_test_host.c, this test does not cover the initial
 *     context switch to untrusted code that occurs when starting an
 *     untrusted thread.  That is because the register values for the
 *     initial context switch are different from the
 *     more-easily-recorded register values for the system calls made
 *     during this test.
 */

static const int kNumberOfCallsToTest = 5;
static const int kFastPathSyscallsToTest = 2;

static int g_call_count = 0;
static int g_in_untrusted_code = 0;
static int g_context_switch_count = 0;
static struct NaClAppThread *g_natp;
static struct RegsTestShm *g_test_shm;


static int32_t TestSyscall(struct NaClAppThread *natp) {
  NaClCopyDropLock(natp->nap);

  if (g_call_count == 0) {
    g_natp = natp;
    SetTrapFlag();
  }
  /* Check that the trap flag has not been unset by anything unexpected. */
  CHECK(GetTrapFlag());

  if (++g_call_count == kNumberOfCallsToTest) {
    UnsetTrapFlag();
    NaClReportExitStatus(natp->nap, 0);
    NaClAppThreadTeardown(natp);
  }
  return 0;
}

static void TrapSignalHandler(int signal, void *ucontext) {
  struct NaClSignalContext context;
  uint32_t prog_ctr;
  int is_inside_trampoline;
  int is_untrusted;
  char buf[100];
  int len;
  struct NaClSignalContext *expected_regs = &g_test_shm->expected_regs;

  if (signal != SIGTRAP) {
    SignalSafeLogStringLiteral("Error: Received unexpected signal\n");
    _exit(1);
  }

  NaClSignalContextFromHandler(&context, ucontext);
  /* Get the prog_ctr value relative to untrusted address space. */
  prog_ctr = (uint32_t) context.prog_ctr;
  /*
   * The trampoline code is not untrusted code because it is fixed by
   * the TCB.  We don't want thread suspension to report prog_ctr as
   * being inside the trampoline code because we are not providing any
   * DWARF unwind info for the trampoline code.  We want the CALL
   * instruction that jumps to a syscall trampoline to appear to be
   * atomic from the point of view of thread suspension: prog_ctr
   * should be reported as either at the CALL or after the CALL.
   *
   * TODO(mseaborn): Move this range check into the non-test part of
   * the thread suspension code.
   */
  is_inside_trampoline = (prog_ctr >= NACL_TRAMPOLINE_START &&
                          prog_ctr < NACL_TRAMPOLINE_END);
  is_untrusted = (NaClSignalContextIsUntrustedForCurrentThread(&context) &&
                  !is_inside_trampoline);
  if (g_in_untrusted_code != is_untrusted) {
    g_context_switch_count++;
    g_in_untrusted_code = is_untrusted;
  }

  if (!*(uint32_t *) NaClUserToSys(g_natp->nap,
                                   (uintptr_t) g_test_shm->regs_should_match))
    return;

  len = snprintf(buf, sizeof(buf), "prog_ctr=0x%"NACL_PRIxNACL_REG": ",
                 context.prog_ctr);
  SignalSafeWrite(buf, len);

  if (is_untrusted) {
    SignalSafeLogStringLiteral("Untrusted context\n");
    RegsUnsetNonCalleeSavedRegisters(&context);
    /*
     * Don't compare prog_ctr if we are executing untrusted code.
     * Untrusted code executes a small loop for calling the syscall,
     * so there are multiple values that prog_ctr can have here.
     */
    context.prog_ctr = expected_regs->prog_ctr;
    RegsAssertEqual(&context, expected_regs);
  } else if ((g_natp->suspend_state & NACL_APP_THREAD_TRUSTED) != 0) {
    SignalSafeLogStringLiteral("Trusted (syscall) context\n");
    NaClThreadContextToSignalContext(&g_natp->user, &context);
    RegsAssertEqual(&context, expected_regs);
  } else {
    enum NaClUnwindCase unwind_case = 0;
    const char *str;

    SignalSafeLogStringLiteral("Inside a context switch: ");

    NaClGetRegistersForContextSwitch(g_natp, &context, &unwind_case);

    str = NaClUnwindCaseToString(unwind_case);
    CHECK(str != NULL);
    SignalSafeWrite(str, strlen(str));
    SignalSafeLogStringLiteral("\n");

    RegsAssertEqual(&context, expected_regs);
  }
}

int main(int argc, char **argv) {
  struct NaClApp app;
  uint32_t mmap_addr;
  char arg_string[32];
  char *args[] = {"prog_name", arg_string};

  NaClAllModulesInit();

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClAddSyscall(NACL_sys_test_syscall_1, TestSyscall);

  CHECK(NaClAppCtor(&app));
  CHECK(NaClAppLoadFileFromFilename(&app, argv[1]) == LOAD_OK);
  NaClAppInitialDescriptorHookup(&app);
  CHECK(NaClAppPrepareToLaunch(&app) == LOAD_OK);

  NaClSignalHandlerInit();
  NaClSignalHandlerSet(TrapSignalHandler);

  /*
   * Allocate some space in untrusted address space.  We pass the
   * address to the guest program so that it can write a register
   * snapshot for us to compare against.
   */
  mmap_addr = NaClSysMmapIntern(
      &app, NULL, sizeof(*g_test_shm),
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_PRIVATE | NACL_ABI_MAP_ANONYMOUS, -1, 0);
  g_test_shm = (struct RegsTestShm *) NaClUserToSys(&app, mmap_addr);
  SNPRINTF(arg_string, sizeof(arg_string), "0x%x", (unsigned int) mmap_addr);

  CHECK(NaClCreateMainThread(&app, 2, args, NULL));
  CHECK(NaClWaitForMainThreadToExit(&app) == 0);

  CHECK(!g_in_untrusted_code);
  ASSERT_EQ(g_context_switch_count,
            (kNumberOfCallsToTest + kFastPathSyscallsToTest - 1) * 2);

  /*
   * Avoid calling exit() because it runs process-global destructors
   * which might break code that is running in our unjoined threads.
   */
  NaClExit(0);
  return 0;
}
