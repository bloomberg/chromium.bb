// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/errorcode.h"

#include <errno.h>

#include "base/macros.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/bpf_dsl/test_trap_registry.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace bpf_dsl {
namespace {

class DummyPolicy : public Policy {
 public:
  DummyPolicy() {}
  ~DummyPolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override { return Allow(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyPolicy);
};

class ErrorCodeTest : public ::testing::Test {
 protected:
  ErrorCodeTest()
      : policy_(), trap_registry_(), compiler_(&policy_, &trap_registry_) {}
  ~ErrorCodeTest() override {}

  DummyPolicy policy_;
  TestTrapRegistry trap_registry_;
  PolicyCompiler compiler_;

  DISALLOW_COPY_AND_ASSIGN(ErrorCodeTest);
};

TEST_F(ErrorCodeTest, ErrnoConstructor) {
  ErrorCode e0;
  EXPECT_EQ(SECCOMP_RET_INVALID, e0.err());

  ErrorCode e1(ErrorCode::ERR_ALLOWED);
  EXPECT_EQ(SECCOMP_RET_ALLOW, e1.err());

  ErrorCode e2(EPERM);
  EXPECT_EQ(SECCOMP_RET_ERRNO + EPERM, e2.err());

  ErrorCode e3 = compiler_.Trap(NULL, NULL, true /* safe */);
  EXPECT_EQ(SECCOMP_RET_TRAP, (e3.err() & SECCOMP_RET_ACTION));

  uint16_t data = 0xdead;
  ErrorCode e4(ErrorCode::ERR_TRACE + data);
  EXPECT_EQ(SECCOMP_RET_TRACE + data, e4.err());
}

TEST_F(ErrorCodeTest, InvalidSeccompRetTrace) {
  // Should die if the trace data does not fit in 16 bits.
  ASSERT_DEATH(ErrorCode e(ErrorCode::ERR_TRACE + (1 << 16)),
               "Invalid use of ErrorCode object");
}

TEST_F(ErrorCodeTest, Trap) {
  ErrorCode e0 = compiler_.Trap(NULL, "a", true /* safe */);
  ErrorCode e1 = compiler_.Trap(NULL, "b", true /* safe */);
  EXPECT_EQ((e0.err() & SECCOMP_RET_DATA) + 1, e1.err() & SECCOMP_RET_DATA);

  ErrorCode e2 = compiler_.Trap(NULL, "a", true /* safe */);
  EXPECT_EQ(e0.err() & SECCOMP_RET_DATA, e2.err() & SECCOMP_RET_DATA);
}

TEST_F(ErrorCodeTest, Equals) {
  ErrorCode e1(ErrorCode::ERR_ALLOWED);
  ErrorCode e2(ErrorCode::ERR_ALLOWED);
  EXPECT_TRUE(e1.Equals(e1));
  EXPECT_TRUE(e1.Equals(e2));
  EXPECT_TRUE(e2.Equals(e1));

  ErrorCode e3(EPERM);
  EXPECT_FALSE(e1.Equals(e3));

  ErrorCode e4 = compiler_.Trap(NULL, "a", true /* safe */);
  ErrorCode e5 = compiler_.Trap(NULL, "b", true /* safe */);
  ErrorCode e6 = compiler_.Trap(NULL, "a", true /* safe */);
  EXPECT_FALSE(e1.Equals(e4));
  EXPECT_FALSE(e3.Equals(e4));
  EXPECT_FALSE(e5.Equals(e4));
  EXPECT_TRUE(e6.Equals(e4));
}

TEST_F(ErrorCodeTest, LessThan) {
  ErrorCode e1(ErrorCode::ERR_ALLOWED);
  ErrorCode e2(ErrorCode::ERR_ALLOWED);
  EXPECT_FALSE(e1.LessThan(e1));
  EXPECT_FALSE(e1.LessThan(e2));
  EXPECT_FALSE(e2.LessThan(e1));

  ErrorCode e3(EPERM);
  EXPECT_FALSE(e1.LessThan(e3));
  EXPECT_TRUE(e3.LessThan(e1));

  ErrorCode e4 = compiler_.Trap(NULL, "a", true /* safe */);
  ErrorCode e5 = compiler_.Trap(NULL, "b", true /* safe */);
  ErrorCode e6 = compiler_.Trap(NULL, "a", true /* safe */);
  EXPECT_TRUE(e1.LessThan(e4));
  EXPECT_TRUE(e3.LessThan(e4));
  EXPECT_TRUE(e4.LessThan(e5));
  EXPECT_FALSE(e4.LessThan(e6));
  EXPECT_FALSE(e6.LessThan(e4));
}

}  // namespace
}  // namespace bpf_dsl
}  // namespace sandbox
