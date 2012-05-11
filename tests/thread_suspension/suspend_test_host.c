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
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


/*
 * These tests mirror thread_suspension_test.cc, but they operate on
 * threads running real untrusted code rather than on mock untrusted
 * threads.
 */

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

    NaClUntrustedThreadsSuspendAll(nap);
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

    NaClUntrustedThreadsSuspendAll(nap);
    NaClUntrustedThreadsResumeAll(nap);

    /* Wait for guest program to make some progress. */
    snapshot = test_shm->var;
    while (test_shm->var == snapshot) { /* do nothing */ }
  }
  test_shm->should_exit = 1;
  CHECK(NaClWaitForMainThreadToExit(nap) == 0);
}

int main(int argc, char **argv) {
  struct NaClApp app;
  struct GioMemoryFileSnapshot gio_file;

  /* Turn off buffering to aid debugging. */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  NaClAllModulesInit();

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

  printf("Running TrySuspendingSyscallInvokerThread...\n");
  TrySuspendingSyscallInvokerThread(&app);

  return 0;
}
