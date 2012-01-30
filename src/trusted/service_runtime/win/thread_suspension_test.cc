/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"

#include "gtest/gtest.h"


class ThreadSuspensionTest : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();
};

void ThreadSuspensionTest::SetUp() {
  NaClNrdAllModulesInit();
}

void ThreadSuspensionTest::TearDown() {
  NaClNrdAllModulesFini();
}


// This test checks that after NaClUntrustedThreadsSuspend() has
// returned, untrusted threads are completely suspended.  We test this
// by running a thread that writes to a memory location.  We check
// that the memory location does not change after
// NaClUntrustedThreadsSuspend() but does change after
// NaClUntrustedThreadsResume().
//
// This is a regression test for
// http://code.google.com/p/nativeclient/issues/detail?id=2557
// The gotcha in the Windows API is that SuspendThread() can return
// before the thread has really been suspended.
//
// This is technically a stress test, but it was able to reproduce the
// problem very reliably, at least on a multicore machine.

struct ThreadArgs {
  volatile uint32_t var;
  volatile bool should_exit;
};

static void WINAPI MutatorThread(void *arg) {
  struct ThreadArgs *args = (struct ThreadArgs *) arg;
  uint32_t next_val = 0;
  while (!args->should_exit) {
    args->var = next_val++;
  }
}

static void TrySuspendingThread(struct NaClApp *nap, volatile uint32_t *addr) {
  // Wait for guest program to start writing to the address.
  while (*addr == 0) { /* do nothing */ }

  for (int iteration = 0; iteration < 100; iteration++) {
    NaClUntrustedThreadsSuspend(nap);
    uint32_t snapshot = *addr;
    for (int count = 0; count < 100000; count++) {
      uint32_t snapshot2 = *addr;
      if (snapshot2 != snapshot) {
        NaClLog(LOG_FATAL,
                "Read %i but expected %i on try %i of iteration %i\n",
                (int) snapshot2, (int) snapshot, count, iteration);
      }
    }
    NaClUntrustedThreadsResume(nap);
    // Wait for guest program to resume writing.
    while (*addr == snapshot) { /* do nothing */ }
  }
}

TEST_F(ThreadSuspensionTest, ThreadSuspension) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);

  struct NaClAppThread app_thread;
  // Perform some minimal initialisation of the NaClAppThread based on
  // what we observe we need for the test.  Reusing
  // NaClAppThreadCtor() here is difficult because it launches an
  // untrusted thread.
  memset(&app_thread, 0xff, sizeof(app_thread));
  app_thread.suspend_state = NACL_APP_THREAD_UNTRUSTED;
  ASSERT_EQ(NaClMutexCtor(&app_thread.mu), 1);
  ASSERT_EQ(NaClCondVarCtor(&app_thread.cv), 1);

  struct ThreadArgs thread_args;
  thread_args.var = 0;
  thread_args.should_exit = false;
  ASSERT_EQ(NaClThreadCreateJoinable(&app_thread.thread, MutatorThread,
                                     &thread_args, NACL_KERN_STACK_SIZE), 1);
  ASSERT_EQ(NaClAddThread(&app, &app_thread), 0);
  TrySuspendingThread(&app, &thread_args.var);
  thread_args.should_exit = true;
  NaClThreadJoin(&app_thread.thread);
}
