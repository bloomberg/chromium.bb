// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
#define SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__

#include "sandbox/linux/tests/unit_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"


namespace sandbox {

// A BPF_DEATH_TEST is just the same as a BPF_TEST, but it assumes that the
// test will fail with a particular known error condition. Use the DEATH_XXX()
// macros from unit_tests.h to specify the expected error condition.
#define BPF_DEATH_TEST(test_case_name, test_name, death, policy, aux...)      \
  void BPF_TEST_##test_name(sandbox::BpfTests<aux>::AuxType& BPF_AUX);        \
  TEST(test_case_name, test_name) {                                           \
    sandbox::BpfTests<aux>::TestArgs arg(BPF_TEST_##test_name, policy);       \
    sandbox::BpfTests<aux>::RunTestInProcess(                                 \
                                   sandbox::BpfTests<aux>::TestWrapper, &arg, \
                                   death);                                    \
  }                                                                           \
  void BPF_TEST_##test_name(sandbox::BpfTests<aux>::AuxType& BPF_AUX)

// BPF_TEST() is a special version of SANDBOX_TEST(). It turns into a no-op,
// if the host does not have kernel support for running BPF filters.
// Also, it takes advantage of the Die class to avoid calling LOG(FATAL), from
// inside our tests, as we don't need or even want all the error handling that
// LOG(FATAL) would do.
// BPF_TEST() takes a C++ data type as an optional fourth parameter. If
// present, this sets up a variable that can be accessed as "BPF_AUX". This
// variable will be passed as an argument to the "policy" function. Policies
// would typically use it as an argument to Sandbox::Trap(), if they want to
// communicate data between the BPF_TEST() and a Trap() function.
#define BPF_TEST(test_case_name, test_name, policy, aux...)                   \
  BPF_DEATH_TEST(test_case_name, test_name, DEATH_SUCCESS(), policy, aux)


// Assertions are handled exactly the same as with a normal SANDBOX_TEST()
#define BPF_ASSERT SANDBOX_ASSERT


// The "Aux" type is optional. We use an "empty" type by default, so that if
// the caller doesn't provide any type, all the BPF_AUX related data compiles
// to nothing.
template<class Aux = int[0]>
class BpfTests : public UnitTests {
 public:
  typedef Aux AuxType;

  class TestArgs {
   public:
    TestArgs(void (*t)(AuxType&), playground2::Sandbox::EvaluateSyscall p)
        : test_(t),
          policy_(p),
          aux_() {
    }

    void (*test() const)(AuxType&) { return test_; }
    playground2::Sandbox::EvaluateSyscall policy() const { return policy_; }

   private:
    friend class BpfTests;

    void (*test_)(AuxType&);
    playground2::Sandbox::EvaluateSyscall policy_;
    AuxType aux_;
  };

  static void TestWrapper(void *void_arg) {
    TestArgs *arg = reinterpret_cast<TestArgs *>(void_arg);
    playground2::Die::EnableSimpleExit();
    if (playground2::Sandbox::SupportsSeccompSandbox(-1) ==
        playground2::Sandbox::STATUS_AVAILABLE) {
      // Ensure the the sandbox is actually available at this time
      int proc_fd;
      BPF_ASSERT((proc_fd = open("/proc", O_RDONLY|O_DIRECTORY)) >= 0);
      BPF_ASSERT(playground2::Sandbox::SupportsSeccompSandbox(proc_fd) ==
                 playground2::Sandbox::STATUS_AVAILABLE);

      // Initialize and then start the sandbox with our custom policy
      playground2::Sandbox::set_proc_fd(proc_fd);
      playground2::Sandbox::SetSandboxPolicy(arg->policy(), &arg->aux_);
      playground2::Sandbox::StartSandbox();

      arg->test()(arg->aux_);
    } else {
      // Call the compiler and verify the policy. That's the least we can do,
      // if we don't have kernel support.
      playground2::Sandbox::SetSandboxPolicy(arg->policy(), &arg->aux_);
      playground2::Sandbox::Program *program =
          playground2::Sandbox::AssembleFilter();
      playground2::Sandbox::VerifyProgram(*program);
      delete program;
      sandbox::UnitTests::IgnoreThisTest();
    }
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BpfTests);
};

}  // namespace

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
