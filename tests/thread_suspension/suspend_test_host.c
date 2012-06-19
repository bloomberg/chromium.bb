/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/thread_suspension/suspend_test.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"


/*
 * These tests mirror thread_suspension_test.cc, but they operate on
 * threads running real untrusted code rather than on mock untrusted
 * threads.
 */

/* This must be called with the mutex nap->threads_mu held. */
static struct NaClAppThread *GetOnlyThread(struct NaClApp *nap) {
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

/*
 * This spins until any previous NaClAppThread has exited to the point
 * where it is removed from the thread array, so that it will not be
 * encountered by a subsequent call to GetOnlyThread().  This is
 * necessary because the threads hosting NaClAppThreads are unjoined.
 */
static void WaitForThreadToExitFully(struct NaClApp *nap) {
  int done;
  do {
    NaClXMutexLock(&nap->threads_mu);
    done = (nap->num_threads == 0);
    NaClXMutexUnlock(&nap->threads_mu);
  } while (!done);
}

static struct SuspendTestShm *StartGuestWithSharedMemory(
    struct NaClApp *nap, char *test_name) {
  char arg_string[32];
  char *args[] = {"prog_name", test_name, arg_string};
  uint32_t mmap_addr;

  /*
   * Allocate some space in untrusted address space.  We pass the address
   * to the guest program so that it can write to it, so that we can
   * observe its writes.
   */
  mmap_addr = NaClCommonSysMmapIntern(
      nap, NULL, sizeof(struct SuspendTestShm),
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_PRIVATE | NACL_ABI_MAP_ANONYMOUS,
      -1, 0);
  SNPRINTF(arg_string, sizeof(arg_string), "0x%x", (unsigned int) mmap_addr);

  WaitForThreadToExitFully(nap);

  CHECK(NaClCreateMainThread(nap, 3, args, NULL));
  return (struct SuspendTestShm *) NaClUserToSys(nap, mmap_addr);
}

/*
 * This test checks that after NaClUntrustedThreadsSuspendAll() has
 * returned, untrusted threads are completely suspended.  We test this
 * by running a thread that writes to a memory location.  We check
 * that the memory location does not change after
 * NaClUntrustedThreadsSuspendAll() but does change after
 * NaClUntrustedThreadsResumeAll().
 *
 * This is based on TrySuspendingMutatorThread() in
 * thread_suspension_test.cc, which is a regression test for
 * http://code.google.com/p/nativeclient/issues/detail?id=2557.
 *
 * This is technically a stress test, but it was able to reproduce the
 * problem very reliably, at least on a multicore machine, as long as
 * MutatorThread() makes no NaCl syscalls.
 */
static void TrySuspendingMutatorThread(struct NaClApp *nap) {
  int iteration;
  struct SuspendTestShm *test_shm;

  test_shm = StartGuestWithSharedMemory(nap, "MutatorThread");

  /* Wait for guest program to start writing to the address. */
  while (test_shm->var == 0) { /* do nothing */ }

  for (iteration = 0; iteration < 100; iteration++) {
    uint32_t snapshot;
    int count;

    NaClUntrustedThreadsSuspendAll(nap, /* save_registers= */ 0);
    snapshot = test_shm->var;
    for (count = 0; count < 100000; count++) {
      uint32_t snapshot2 = test_shm->var;
      if (snapshot2 != snapshot) {
        NaClLog(LOG_FATAL,
                "Read %i but expected %i on try %i of iteration %i\n",
                (int) snapshot2, (int) snapshot, count, iteration);
      }
    }
    NaClUntrustedThreadsResumeAll(nap);
    /* Wait for guest program to resume writing. */
    while (test_shm->var == snapshot) { /* do nothing */ }
  }
  test_shm->should_exit = 1;
  CHECK(NaClWaitForMainThreadToExit(nap) == 0);
}

/* This implements a NaCl syscall. */
static int32_t SuspendTestSyscall(struct NaClAppThread *natp) {
  uint32_t test_shm_uptr;
  struct SuspendTestShm *test_shm;
  uint32_t next_val = 0;

  NaClCopyInDropLock(natp->nap);
  CHECK(NaClCopyInFromUser(natp->nap, &test_shm_uptr, natp->usr_syscall_args,
                           sizeof(test_shm_uptr)));
  test_shm = (struct SuspendTestShm *) NaClUserToSysAddrRange(
      natp->nap, test_shm_uptr, sizeof(*test_shm));

  while (!test_shm->should_exit) {
    /*
     * Assign a new value so that this syscall can be observed to be
     * executing, but avoid small values which have special meanings
     * in this test.
     */
    test_shm->var = 0x100 + (next_val++ & 0xff);
  }
  /* Indicate that we are exiting. */
  test_shm->var = 1;
  return 0;
}

/*
 * This test checks that a thread running a NaCl syscall will not be
 * suspended until the syscall returns to untrusted code.
 */
static void TrySuspendingSyscall(struct NaClApp *nap) {
  int iteration;
  uint32_t snapshot;
  struct SuspendTestShm *test_shm;

  NaClAddSyscall(NACL_sys_test_syscall_1, SuspendTestSyscall);
  test_shm = StartGuestWithSharedMemory(nap, "SyscallReturnThread");

  /* Wait for the syscall to be entered and stop. */
  while (test_shm->var == 0) { /* do nothing */ }
  NaClUntrustedThreadsSuspendAll(nap, /* save_registers= */ 0);
  /*
   * The syscall should continue to execute, so we should see the
   * value change.
   */
  snapshot = test_shm->var;
  while (test_shm->var == snapshot) { /* do nothing */ }
  /* Tell the syscall to return. */
  test_shm->should_exit = 1;
  /* Wait for the syscall to return. */
  while (test_shm->var != 1) { /* do nothing */ }
  /*
   * Once the syscall has returned, we should see no change to the
   * variable because the thread is suspended.
   */
  for (iteration = 0; iteration < 1000; iteration++) {
    CHECK(test_shm->var == 1);
  }
  /*
   * But once we resume the thread, untrusted code will run and set
   * the value.
   */
  NaClUntrustedThreadsResumeAll(nap);
  while (test_shm->var != 99) { /* do nothing */ }
}

/*
 * The test below checks that we do not get a deadlock when using
 * NaClUntrustedThreadsSuspendAll() on threads that cross between
 * untrusted and trusted code by invoking NaCl syscalls.
 *
 * This is a stress test, based on TrySuspendingSyscallInvokerThread()
 * in thread_suspension_test.cc, which is for testing
 * http://code.google.com/p/nativeclient/issues/detail?id=2569.
 */
static void TrySuspendingSyscallInvokerThread(struct NaClApp *nap) {
  int iteration;
  struct SuspendTestShm *test_shm;

  test_shm = StartGuestWithSharedMemory(nap, "SyscallInvokerThread");

  /* Wait for guest program to start writing to the address. */
  while (test_shm->var == 0) { /* do nothing */ }

  for (iteration = 0; iteration < 1000; iteration++) {
    uint32_t snapshot;

    NaClUntrustedThreadsSuspendAll(nap, /* save_registers= */ 0);
    NaClUntrustedThreadsResumeAll(nap);

    /* Wait for guest program to make some progress. */
    snapshot = test_shm->var;
    while (test_shm->var == snapshot) { /* do nothing */ }
  }
  test_shm->should_exit = 1;
  CHECK(NaClWaitForMainThreadToExit(nap) == 0);
}

static void TestGettingRegisterSnapshot(struct NaClApp *nap) {
  struct SuspendTestShm *test_shm;
  struct NaClAppThread *natp;
  struct NaClSignalContext *regs;

  test_shm = StartGuestWithSharedMemory(nap, "RegisterSetterThread");
  /*
   * Wait for the guest program to reach untrusted code and set
   * registers to known test values.
   */
  while (test_shm->var == 0) { /* do nothing */ }

  /*
   * Check that registers are not saved unless this is requested.
   * Currently this must come before calling
   * NaClUntrustedThreadsSuspendAll() with save_registers=1.
   */
  NaClUntrustedThreadsSuspendAll(nap, /* save_registers= */ 0);
  natp = GetOnlyThread(nap);
  CHECK(natp->suspended_registers == NULL);
  NaClUntrustedThreadsResumeAll(nap);

  NaClUntrustedThreadsSuspendAll(nap, /* save_registers= */ 1);
  /*
   * The previous natp is valid only while holding nap->threads_mu,
   * which NaClUntrustedThreadsSuspendAll() claims for us.  Re-get
   * natp in case the thread exited.
   */
  natp = GetOnlyThread(nap);
  regs = natp->suspended_registers;
  CHECK(regs != NULL);

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  CHECK(regs->eax == test_shm->var);
  CHECK(regs->ebx == regs->prog_ctr + 0x2000);
  CHECK(regs->ecx == regs->stack_ptr + 0x1000);
  CHECK(regs->edx == 0x10000001);
  CHECK(regs->ebp == 0x20000002);
  CHECK(regs->esi == 0x30000003);
  CHECK(regs->edi == 0x40000004);
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
  CHECK(regs->rax == test_shm->var);
  CHECK(regs->rbx == regs->prog_ctr + 0x2000);
  CHECK(regs->rcx == regs->stack_ptr + 0x1000);
  CHECK(regs->rbp == regs->r15 + 0x10000001);
  CHECK(regs->rdx == 0x2000000000000002);
  CHECK(regs->rsi == 0x3000000000000003);
  CHECK(regs->rdi == 0x4000000000000004);
  CHECK(regs->r8 == 0x8000000000000008);
  CHECK(regs->r9 == 0x9000000000000009);
  CHECK(regs->r10 == 0xa00000000000000a);
  CHECK(regs->r11 == 0xb00000000000000b);
  CHECK(regs->r12 == 0xc00000000000000c);
  CHECK(regs->r13 == 0xd00000000000000d);
  CHECK(regs->r14 == 0xe00000000000000e);
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
  CHECK(regs->r0 == test_shm->var);
  CHECK(regs->r1 == regs->prog_ctr + 0x2000);
  CHECK(regs->r2 == regs->stack_ptr + 0x1000);
  CHECK(regs->r3 == 0x30000003);
  CHECK(regs->r4 == 0x40000004);
  CHECK(regs->r5 == 0x50000005);
  CHECK(regs->r6 == 0x60000006);
  CHECK(regs->r7 == 0x70000007);
  CHECK(regs->r8 == 0x80000008);
  /* r9 is reserved for TLS and is read-only. */
  CHECK(regs->r9 == natp->user.r9);
  CHECK(regs->r10 == 0xa000000a);
  CHECK(regs->r11 == 0xb000000b);
  CHECK(regs->r12 == 0xc000000c);
  CHECK(regs->lr == 0xe000000e);
#else
# error Unsupported architecture
#endif
  /* For simplicity, we do not attempt to resume the thread. */
}

int main(int argc, char **argv) {
  struct NaClApp app;
  struct GioMemoryFileSnapshot gio_file;

  /* Turn off buffering to aid debugging. */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  NaClAllModulesInit();

#if NACL_LINUX
  NaClSignalHandlerInit();
#endif

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClFileNameForValgrind(argv[1]);
  CHECK(GioMemoryFileSnapshotCtor(&gio_file, argv[1]));
  CHECK(NaClAppCtor(&app));
  CHECK(NaClAppLoadFile((struct Gio *) &gio_file, &app) == LOAD_OK);
  CHECK(NaClAppPrepareToLaunch(&app) == LOAD_OK);

  /*
   * We reuse the same sandbox for both tests.
   *
   * TODO(mseaborn): It would be cleaner to create a new sandbox for
   * each test, but if we are to do that in a single process without
   * running out of address space on x86-32, we would need to
   * reinstate the code for deallocating a sandbox's address space.
   */

  printf("Running TrySuspendingMutatorThread...\n");
  TrySuspendingMutatorThread(&app);

  printf("Running TrySuspendingSyscall...\n");
  TrySuspendingSyscall(&app);

  printf("Running TrySuspendingSyscallInvokerThread...\n");
  TrySuspendingSyscallInvokerThread(&app);

  printf("Running TestGettingRegisterSnapshot...\n");
  TestGettingRegisterSnapshot(&app);

  /*
   * Avoid calling exit() because it runs process-global destructors
   * which might break code that is running in our unjoined threads.
   */
  NaClExit(0);
}
