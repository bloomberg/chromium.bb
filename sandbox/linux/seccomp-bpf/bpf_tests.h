// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
#define SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/debug/leak_annotations.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_compatibility_policy.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

// A BPF_DEATH_TEST is just the same as a BPF_TEST, but it assumes that the
// test will fail with a particular known error condition. Use the DEATH_XXX()
// macros from unit_tests.h to specify the expected error condition.
// A BPF_DEATH_TEST is always disabled under ThreadSanitizer, see
// crbug.com/243968.
#define BPF_DEATH_TEST(test_case_name, test_name, death, policy, aux...) \
  void BPF_TEST_##test_name(sandbox::BPFTests<aux>::AuxType* BPF_AUX);   \
  TEST(test_case_name, DISABLE_ON_TSAN(test_name)) {                     \
    sandbox::BPFTests<aux>::TestArgs arg(BPF_TEST_##test_name, policy);  \
    sandbox::BPFTests<aux>::RunTestInProcess(                            \
        sandbox::BPFTests<aux>::TestWrapper, &arg, death);               \
  }                                                                      \
  void BPF_TEST_##test_name(sandbox::BPFTests<aux>::AuxType* BPF_AUX)

// BPF_TEST() is a special version of SANDBOX_TEST(). It turns into a no-op,
// if the host does not have kernel support for running BPF filters.
// Also, it takes advantage of the Die class to avoid calling LOG(FATAL), from
// inside our tests, as we don't need or even want all the error handling that
// LOG(FATAL) would do.
// BPF_TEST() takes a C++ data type as an optional fourth parameter. If
// present, this sets up a variable that can be accessed as "BPF_AUX". This
// variable will be passed as an argument to the "policy" function. Policies
// would typically use it as an argument to SandboxBPF::Trap(), if they want to
// communicate data between the BPF_TEST() and a Trap() function. The life-time
// of this object is the same as the life-time of the process.
// The type specified in |aux| and the last parameter of the policy function
// must be compatible. If |aux| is not specified, the policy function must
// take a void* as its last parameter (that is, must have the EvaluateSyscall
// type).
#define BPF_TEST(test_case_name, test_name, policy, aux...) \
  BPF_DEATH_TEST(test_case_name, test_name, DEATH_SUCCESS(), policy, aux)

// Assertions are handled exactly the same as with a normal SANDBOX_TEST()
#define BPF_ASSERT SANDBOX_ASSERT
#define BPF_ASSERT_EQ(x, y) BPF_ASSERT((x) == (y))
#define BPF_ASSERT_NE(x, y) BPF_ASSERT((x) != (y))
#define BPF_ASSERT_LT(x, y) BPF_ASSERT((x) < (y))
#define BPF_ASSERT_GT(x, y) BPF_ASSERT((x) > (y))
#define BPF_ASSERT_LE(x, y) BPF_ASSERT((x) <= (y))
#define BPF_ASSERT_GE(x, y) BPF_ASSERT((x) >= (y))

// The "Aux" type is optional. void is used by default and all pointers
// provided to the tests will be NULL.
template <class Aux = void>
class BPFTests : public UnitTests {
 public:
  typedef Aux AuxType;

  class TestArgs {
   public:
    TestArgs(void (*t)(AuxType*),
             typename CompatibilityPolicy<AuxType>::SyscallEvaluator p)
        : test_(t), policy_function_(p) {}

    void (*test() const)(AuxType*) { return test_; }

   private:
    friend class BPFTests;

    void (*test_)(AuxType*);
    typename CompatibilityPolicy<AuxType>::SyscallEvaluator policy_function_;
  };

  static void TestWrapper(void* void_arg) {
    TestArgs* arg = reinterpret_cast<TestArgs*>(void_arg);
    sandbox::Die::EnableSimpleExit();

    // This will be NULL iff AuxType is void.
    AuxType* aux_pointer_for_policy = NewAux();

    scoped_ptr<CompatibilityPolicy<AuxType> > policy(
        new CompatibilityPolicy<AuxType>(arg->policy_function_,
                                         aux_pointer_for_policy));

    if (sandbox::SandboxBPF::SupportsSeccompSandbox(-1) ==
        sandbox::SandboxBPF::STATUS_AVAILABLE) {
      // Ensure the the sandbox is actually available at this time
      int proc_fd;
      BPF_ASSERT((proc_fd = open("/proc", O_RDONLY | O_DIRECTORY)) >= 0);
      BPF_ASSERT(sandbox::SandboxBPF::SupportsSeccompSandbox(proc_fd) ==
                 sandbox::SandboxBPF::STATUS_AVAILABLE);

      // Initialize and then start the sandbox with our custom policy
      sandbox::SandboxBPF sandbox;
      sandbox.set_proc_fd(proc_fd);
      sandbox.SetSandboxPolicy(policy.release());
      BPF_ASSERT(
          sandbox.StartSandbox(sandbox::SandboxBPF::PROCESS_SINGLE_THREADED));

      // Run the actual test.
      arg->test()(aux_pointer_for_policy);

      // Once a BPF policy is engaged, there is no going back. A SIGSYS handler
      // can make use of aux, this can happen even in _exit(). This object's
      // ownership has been passed to the kernel by engaging the sandbox and it
      // will be destroyed with the process.
      ANNOTATE_LEAKING_OBJECT_PTR(aux_pointer_for_policy);
    } else {
      printf("This BPF test is not fully running in this configuration!\n");
      // Android and Valgrind are the only configurations where we accept not
      // having kernel BPF support.
      if (!IsAndroid() && !IsRunningOnValgrind()) {
        const bool seccomp_bpf_is_supported = false;
        BPF_ASSERT(seccomp_bpf_is_supported);
      }
      // Call the compiler and verify the policy. That's the least we can do,
      // if we don't have kernel support.
      sandbox::SandboxBPF sandbox;
      sandbox.SetSandboxPolicy(policy.release());
      sandbox::SandboxBPF::Program* program =
          sandbox.AssembleFilter(true /* force_verification */);
      delete program;
      DeleteAux(aux_pointer_for_policy);
      sandbox::UnitTests::IgnoreThisTest();
    }
  }

 private:
  // Allocate an object of type Aux. This is specialized to return NULL when
  // trying to allocate a void.
  static Aux* NewAux() { return new Aux(); }
  static void DeleteAux(Aux* aux) { delete aux; }

  DISALLOW_IMPLICIT_CONSTRUCTORS(BPFTests);
};

// Specialization of NewAux that returns NULL;
template <>
void* BPFTests<void>::NewAux();
template <>
void BPFTests<void>::DeleteAux(void* aux);

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
