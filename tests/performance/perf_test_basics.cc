/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// We need <sys/types.h> first to work around a nacl-newlib bug.
// See https://code.google.com/p/nativeclient/issues/detail?id=677
#include <sys/types.h>
#include <sys/mman.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/tests/performance/perf_test_runner.h"


// This measures the overhead of the test framework and a virtual
// function call.
class TestNull : public PerfTest {
 public:
  virtual void run() {
  }
};
PERF_TEST_DECLARE(TestNull)

class TestNaClSyscall : public PerfTest {
 public:
  virtual void run() {
    NACL_SYSCALL(null)();
  }
};
PERF_TEST_DECLARE(TestNaClSyscall)

// Measure the overhead of the clock_gettime() call that the test
// framework uses.  This is also an example of a not-quite-trivial
// NaCl syscall which writes to untrusted address space and might do a
// host OS syscall.
class TestClockGetTime : public PerfTest {
 public:
  virtual void run() {
    struct timespec time;
    ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, &time), 0);
  }
};
PERF_TEST_DECLARE(TestClockGetTime)

// We declare this as "volatile" in an attempt to prevent the compiler
// from optimizing accesses away.
__thread volatile int g_tls_var = 123;

class TestTlsVariable : public PerfTest {
 public:
  virtual void run() {
    ASSERT_EQ(g_tls_var, 123);
  }
};
PERF_TEST_DECLARE(TestTlsVariable)

class TestMmapAnonymous : public PerfTest {
 public:
  virtual void run() {
    size_t size = 0x10000;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    ASSERT_NE(addr, MAP_FAILED);
    ASSERT_EQ(munmap(addr, size), 0);
  }
};
PERF_TEST_DECLARE(TestMmapAnonymous)
