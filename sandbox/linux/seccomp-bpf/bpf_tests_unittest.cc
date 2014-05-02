// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/bpf_tests.h"

#include <errno.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

ErrorCode EmptyPolicy(SandboxBPF* sandbox, int sysno, void* aux) {
  // |aux| should always be NULL since a type was not specified as an argument
  // to BPF_TEST.
  BPF_ASSERT(NULL == aux);
  if (!SandboxBPF::IsValidSyscallNumber(sysno)) {
    return ErrorCode(ENOSYS);
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(BPFTest, BPFAUXIsNull, EmptyPolicy) {
  // Check that the implicit BPF_AUX argument is NULL when we
  // don't specify a fourth parameter to BPF_TEST.
  BPF_ASSERT(NULL == BPF_AUX);
}

class FourtyTwo {
 public:
  static const int kMagicValue = 42;
  FourtyTwo() : value_(kMagicValue) {}
  int value() { return value_; }

 private:
  int value_;
  DISALLOW_COPY_AND_ASSIGN(FourtyTwo);
};

ErrorCode EmptyPolicyTakesClass(SandboxBPF* sandbox,
                                int sysno,
                                FourtyTwo* fourty_two) {
  // |aux| should point to an instance of FourtyTwo.
  BPF_ASSERT(fourty_two);
  BPF_ASSERT(FourtyTwo::kMagicValue == fourty_two->value());
  if (!SandboxBPF::IsValidSyscallNumber(sysno)) {
    return ErrorCode(ENOSYS);
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(BPFTest,
         BPFAUXPointsToClass,
         EmptyPolicyTakesClass,
         FourtyTwo /* *BPF_AUX */) {
  // BPF_AUX should point to an instance of FourtyTwo.
  BPF_ASSERT(BPF_AUX);
  BPF_ASSERT(FourtyTwo::kMagicValue == BPF_AUX->value());
}

}  // namespace

}  // namespace sandbox
