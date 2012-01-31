// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(halyavin): Decide where non-PPAPI chrome tests should go and move this
// test there. Issue http://code.google.com/p/chromium/issues/detail?id=112057.

#include "ppapi/tests/test_exception.h"

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include <sys/nacl_syscalls.h>
#include <setjmp.h>

#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Exception);

jmp_buf env;

TestException::TestException(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestException::Init() {
  return true;
}

void TestException::RunTests(const std::string& filter) {
#if !defined(__x86_64__) && defined(__native_client__)
  RUN_TEST(CatchException, filter);
#endif
}

void handler(int eip, int esp) {
  NACL_SYSCALL(exception_clear_flag)();
  longjmp(env, 1);
}

std::string TestException::TestCatchException() {
  if (setjmp(env)) {
    PASS();
  } else {
    NACL_SYSCALL(exception_handler)(handler, NULL);
    *((int*)0) = 0;
  }
  return "exception is ignored";
}