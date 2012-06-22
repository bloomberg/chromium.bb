// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace playground2;

namespace {

TEST(SandboxBpf, CallSupports) {
  // We check that we don't crash, but it's ok if the kernel doesn't
  // support it.
  Sandbox::supportsSeccompSandbox(-1);
}

TEST(SandboxBpf, CallSupportsTwice) {
  Sandbox::supportsSeccompSandbox(-1);
  Sandbox::supportsSeccompSandbox(-1);
}

static Sandbox::ErrorCode NanosleepEvaluator(int sysno) {
  if (sysno < (int) MIN_SYSCALL || sysno > (int) MAX_SYSCALL) {
    // FIXME: we should really not have to do that in a trivial policy
    return ENOSYS;
  }
  switch (sysno) {
    case __NR_nanosleep:
      return EACCES;
    default:
      return Sandbox::SB_ALLOWED;
  }
}

void BasicPolicyProcess(void) {
  int proc_fd = open("/proc", O_RDONLY|O_DIRECTORY);
  if (proc_fd < 0)
    exit(-1);
  if(Sandbox::supportsSeccompSandbox(proc_fd) !=
     Sandbox::STATUS_AVAILABLE)
    exit(-1);
  Sandbox::setProcFd(proc_fd);
  Sandbox::setSandboxPolicy(NanosleepEvaluator, NULL);
  Sandbox::startSandbox();
  const struct timespec ts = {0, 0};
  if(nanosleep(&ts, NULL) != -1 || errno != EACCES)
    exit(-1);
  exit(0);
}

TEST(SandboxBpf, CanApplyBasicPolicy) {
  // This test will pass if seccomp-bpf is not supported
  if(Sandbox::supportsSeccompSandbox(-1) ==
     Sandbox::STATUS_AVAILABLE) {
    // TODO: find a way to use the testing testing framework inside
    // BasicPolicyProcess or at the very least to surface errors
    EXPECT_EXIT(BasicPolicyProcess(), ::testing::ExitedWithCode(0), "");
  }
}

} // namespace
