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
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_test_runner.h"
#include "sandbox/linux/tests/sandbox_test_runner.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

// A BPF_DEATH_TEST is just the same as a BPF_TEST, but it assumes that the
// test will fail with a particular known error condition. Use the DEATH_XXX()
// macros from unit_tests.h to specify the expected error condition.
// A BPF_DEATH_TEST is always disabled under ThreadSanitizer, see
// crbug.com/243968.
#define BPF_DEATH_TEST(test_case_name, test_name, death, policy, aux...) \
  void BPF_TEST_##test_name(                                             \
      sandbox::BPFTesterSimpleDelegate<aux>::AuxType* BPF_AUX);          \
  TEST(test_case_name, DISABLE_ON_TSAN(test_name)) {                     \
    sandbox::SandboxBPFTestRunner bpf_test_runner(                       \
        new sandbox::BPFTesterSimpleDelegate<aux>(BPF_TEST_##test_name,  \
                                                  policy));              \
    sandbox::UnitTests::RunTestInProcess(&bpf_test_runner, death);       \
  }                                                                      \
  void BPF_TEST_##test_name(                                             \
      sandbox::BPFTesterSimpleDelegate<aux>::AuxType* BPF_AUX)

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

template <class Aux = void>
class BPFTesterSimpleDelegate : public BPFTesterDelegate {
 public:
  typedef Aux AuxType;
  BPFTesterSimpleDelegate(
      void (*test_function)(AuxType*),
      typename CompatibilityPolicy<AuxType>::SyscallEvaluator policy_function)
      : aux_pointer_for_policy_(NULL),
        test_function_(test_function),
        policy_function_(policy_function) {
    // This will be NULL iff AuxType is void.
    aux_pointer_for_policy_ = NewAux();
  }

  virtual ~BPFTesterSimpleDelegate() { DeleteAux(aux_pointer_for_policy_); }

  virtual scoped_ptr<SandboxBPFPolicy> GetSandboxBPFPolicy() OVERRIDE {
    // The current method is guaranteed to only run in the child process
    // running the test. In this process, the current object is guaranteed
    // to live forever. So it's ok to pass aux_pointer_for_policy_ to
    // the policy, which could in turn pass it to the kernel via Trap().
    return scoped_ptr<SandboxBPFPolicy>(new CompatibilityPolicy<AuxType>(
        policy_function_, aux_pointer_for_policy_));
  }

  virtual void RunTestFunction() OVERRIDE {
    // Run the actual test.
    // The current object is guaranteed to live forever in the child process
    // where this will run.
    test_function_(aux_pointer_for_policy_);
  }

 private:
  // Allocate an object of type Aux. This is specialized to return NULL when
  // trying to allocate a void.
  static Aux* NewAux() { return new Aux(); }
  static void DeleteAux(Aux* aux) { delete aux; }

  AuxType* aux_pointer_for_policy_;
  void (*test_function_)(AuxType*);
  typename CompatibilityPolicy<AuxType>::SyscallEvaluator policy_function_;
  DISALLOW_COPY_AND_ASSIGN(BPFTesterSimpleDelegate);
};

// Specialization of NewAux that returns NULL;
template <>
void* BPFTesterSimpleDelegate<void>::NewAux();
template <>
void BPFTesterSimpleDelegate<void>::DeleteAux(void* aux);

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
