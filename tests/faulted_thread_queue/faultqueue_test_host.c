/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/osx/mach_exception_handler.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"
#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"
#include "native_client/tests/common/register_set.h"


/*
 * This test program checks that we get notification of a fault that
 * happens in untrusted code in faultqueue_test_guest.c.
 */

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
static const int kBreakInstructionSize = 1;
static const int kBreakInstructionSignal = NACL_ABI_SIGSEGV;
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
static const int kBreakInstructionSize = 4;
static const int kBreakInstructionSignal = NACL_ABI_SIGTRAP;
#else
# error Unsupported architecture
#endif

void WaitForThreadToFault(struct NaClApp *nap) {
  /*
   * Wait until a faulted thread is reported.
   * TODO(mseaborn): We should not busy-wait here.
   * See http://code.google.com/p/nativeclient/issues/detail?id=2952
   */
  while (*(volatile Atomic32 *) &nap->faulted_thread_count == 0) {
    /* Do nothing. */
  }
  ASSERT_EQ(nap->faulted_thread_count, 1);
}

/* This must be called with the mutex nap->threads_mu held. */
struct NaClAppThread *GetOnlyThread(struct NaClApp *nap) {
  struct NaClAppThread *found_thread = NULL;
  size_t index;
  for (index = 0; index < nap->threads.num_entries; index++) {
    struct NaClAppThread *natp = NaClGetThreadMu(nap, (int) index);
    if (natp != NULL) {
      CHECK(found_thread == NULL);
      found_thread = natp;
    }
  }
  CHECK(found_thread != NULL);
  return found_thread;
}

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86

/*
 * This function is entered with threads suspended, and it exits with
 * threads suspended too.
 */
void TestSingleStepping(struct NaClAppThread *natp) {
  struct NaClApp *nap = natp->nap;
  struct NaClSignalContext regs;
  struct NaClSignalContext expected_regs;
  int instr_size;

  /* Set the trap flag to enable single-stepping. */
  NaClAppThreadGetSuspendedRegisters(natp, &regs);
  regs.flags |= NACL_X86_TRAP_FLAG;
  expected_regs = regs;
  NaClAppThreadSetSuspendedRegisters(natp, &regs);

  for (instr_size = 1; instr_size <= 5; instr_size++) {
    int signal;

    fprintf(stderr, "Now expecting to single-step through "
            "an instruction %i bytes in size at address 0x%x\n",
            instr_size, (int) expected_regs.prog_ctr);
    expected_regs.prog_ctr += instr_size;

    NaClUntrustedThreadsResumeAll(nap);
    WaitForThreadToFault(nap);
    NaClUntrustedThreadsSuspendAll(nap, /* save_registers= */ 1);
    ASSERT_EQ(GetOnlyThread(nap), natp);
    ASSERT_EQ(NaClAppThreadUnblockIfFaulted(natp, &signal), 1);
    /*
     * TODO(mseaborn): Move ExceptionToSignal() out of debug_stub and
     * enable this check on Windows too.
     */
    if (!NACL_WINDOWS) {
      ASSERT_EQ(signal, NACL_ABI_SIGTRAP);
    }

    NaClAppThreadGetSuspendedRegisters(natp, &regs);
    if (NACL_WINDOWS) {
      /*
       * On Windows, but not elsewhere, the trap flag gets unset after
       * each instruction, so we must set it again.
       */
      regs.flags |= NACL_X86_TRAP_FLAG;
      NaClAppThreadSetSuspendedRegisters(natp, &regs);
    }
    RegsAssertEqual(&regs, &expected_regs);
  }

  /* Unset the trap flag so that the thread can continue. */
  regs.flags &= ~NACL_X86_TRAP_FLAG;
  NaClAppThreadSetSuspendedRegisters(natp, &regs);
}

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

/* ARM does not have hardware single-stepping, so nothing to do here. */
void TestSingleStepping(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
}

#else
# error Unsupported architecture
#endif

int main(int argc, char **argv) {
  struct NaClApp app;
  struct NaClApp *nap = &app;
  struct GioMemoryFileSnapshot gio_file;
  char arg_string[32];
  char *args[] = {"prog_name", arg_string};
  uint32_t mmap_addr;
  struct NaClSignalContext *expected_regs;
  struct NaClSignalContext regs;
  struct NaClAppThread *natp;
  int signal = 0;
  int dummy_signal;

  NaClDebugExceptionHandlerStandaloneHandleArgs(argc, argv);
  NaClHandleBootstrapArgs(&argc, &argv);

  /* Turn off buffering to aid debugging. */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  NaClAllModulesInit();

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClFileNameForValgrind(argv[1]);
  CHECK(GioMemoryFileSnapshotCtor(&gio_file, argv[1]));
  CHECK(NaClAppCtor(nap));
  CHECK(NaClAppLoadFile((struct Gio *) &gio_file, nap) == LOAD_OK);
  NaClAppInitialDescriptorHookup(nap);
  CHECK(NaClAppPrepareToLaunch(nap) == LOAD_OK);

#if NACL_LINUX
  NaClSignalHandlerInit();
#elif NACL_OSX
  CHECK(NaClInterceptMachExceptions());
#elif NACL_WINDOWS
  nap->attach_debug_exception_handler_func =
      NaClDebugExceptionHandlerStandaloneAttach;
#else
# error Unknown host OS
#endif
  CHECK(NaClFaultedThreadQueueEnable(nap));

  /*
   * Allocate some space in untrusted address space.  We pass the
   * address to the guest program so that we can share data with it.
   */
  mmap_addr = NaClCommonSysMmapIntern(
      nap, NULL, sizeof(*expected_regs),
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_PRIVATE | NACL_ABI_MAP_ANONYMOUS,
      -1, 0);
  SNPRINTF(arg_string, sizeof(arg_string), "0x%x", (unsigned int) mmap_addr);
  expected_regs = (struct NaClSignalContext *) NaClUserToSys(nap, mmap_addr);

  CHECK(NaClCreateMainThread(nap, NACL_ARRAY_SIZE(args), args, NULL));

  WaitForThreadToFault(nap);
  NaClUntrustedThreadsSuspendAll(nap, /* save_registers= */ 1);
  natp = GetOnlyThread(nap);
  ASSERT_EQ(NaClAppThreadUnblockIfFaulted(natp, &signal), 1);
  /*
   * TODO(mseaborn): Move ExceptionToSignal() out of debug_stub and
   * enable this check on Windows too.
   */
  if (!NACL_WINDOWS) {
    ASSERT_EQ(signal, kBreakInstructionSignal);
  }

  /* Check that faulted_thread_count is updated correctly. */
  ASSERT_EQ(nap->faulted_thread_count, 0);
  /*
   * Check that NaClAppThreadUnblockIfFaulted() returns false when
   * called on a thread that is not blocked.
   */
  ASSERT_EQ(NaClAppThreadUnblockIfFaulted(natp, &dummy_signal), 0);

  NaClAppThreadGetSuspendedRegisters(natp, &regs);
  RegsAssertEqual(&regs, expected_regs);

  /* Skip over the instruction that faulted. */
  regs.prog_ctr += kBreakInstructionSize;
  NaClAppThreadSetSuspendedRegisters(natp, &regs);

  TestSingleStepping(natp);

  NaClUntrustedThreadsResumeAll(nap);
  CHECK(NaClWaitForMainThreadToExit(nap) == 0);

  /*
   * Avoid calling exit() because it runs process-global destructors
   * which might break code that is running in our unjoined threads.
   */
  NaClExit(0);
}
