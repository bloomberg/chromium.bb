// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
#define SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__

#include "sandbox/linux/tests/unit_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"


namespace sandbox {

// BPF_TEST() is a special version of SANDBOX_TEST(). It turns into a no-op,
// if the host does not have kernel support for running BPF filters.
// Also, it takes advantage of the Die class to avoid calling LOG(FATAL), from
// inside our tests, as we don't need or even want all the error handling that
// LOG(FATAL) would do.
#define BPF_TEST(test_case_name, test_name, policy)                           \
  void BPF_TEST_##test_name();                                                \
  TEST(test_case_name, test_name) {                                           \
    sandbox::BpfTests::TestArgs arg(BPF_TEST_##test_name, policy);            \
    sandbox::BpfTests::RunTestInProcess(sandbox::BpfTests::TestWrapper, &arg);\
  }                                                                           \
  void BPF_TEST_##test_name()

// Assertions are handled exactly the same as with a normal SANDBOX_TEST()
#define BPF_ASSERT SANDBOX_ASSERT


class BpfTests : public UnitTests {
 public:
  class TestArgs {
   public:
    TestArgs(void (*test)(), playground2::Sandbox::EvaluateSyscall policy)
        : test_(test),
          policy_(policy) {
    }

    void (*test() const)() { return test_; }
    playground2::Sandbox::EvaluateSyscall policy() const { return policy_; }

   private:
    void (*test_)();
    playground2::Sandbox::EvaluateSyscall policy_;
  };

  static void TestWrapper(void *void_arg);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BpfTests);
};

}  // namespace

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
