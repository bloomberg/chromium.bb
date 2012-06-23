// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace playground2;

namespace {

const int kExpectedReturnValue = 42;

TEST(SandboxBpf, CallSupports) {
  // We check that we don't crash, but it's ok if the kernel doesn't
  // support it.
  Sandbox::supportsSeccompSandbox(-1);
}

TEST(SandboxBpf, CallSupportsTwice) {
  Sandbox::supportsSeccompSandbox(-1);
  Sandbox::supportsSeccompSandbox(-1);
}

__attribute__((noreturn)) void DoCrash() {
  // Cause a #PF. This only works if we assume that we have the default
  // SIGSEGV handler.
  *(reinterpret_cast<volatile char*>(NULL)) = '\0';
  for (;;) {
    // If we didn't manage to crash there is really nothing we can do reliably
    // but spin.
  }
}

__attribute__((noreturn)) void ExitGroup(int status) {
  syscall(__NR_exit_group, status);
  // If exit_group() failed, there is a high likelihood this
  // happened due to a bug in the sandbox. We therefore cannot
  // blindly assume that the failure happened because we are
  // running on an old kernel that supports exit(), but not
  // exit_group(). We cannot even trust "errno" returning
  // "ENOSYS". So, calling exit() as the fallback for
  // exit_group() would at this point almost certainly be a
  // bug. Instead, try some more aggressive methods to make
  // the program stop.
  DoCrash();
}

// Helper function to start a sandbox with a policy specified in
// evaluator
void StartSandboxOrDie(Sandbox::EvaluateSyscall evaluator) {
  int proc_fd = open("/proc", O_RDONLY|O_DIRECTORY);
  if (proc_fd < 0 || !evaluator) {
    ExitGroup(1);
  }
  if (Sandbox::supportsSeccompSandbox(proc_fd) !=
     Sandbox::STATUS_AVAILABLE) {
    ExitGroup(1);
  }
  Sandbox::setProcFd(proc_fd);
  Sandbox::setSandboxPolicy(evaluator, NULL);
  Sandbox::startSandbox();
}

void RunInSandbox(Sandbox::EvaluateSyscall evaluator,
                      void (*SandboxedCode)()) {
  // TODO(jln): Implement IsEqual for ErrorCode
  // IsEqual(evaluator(__NR_exit_group), Sandbox::SB_ALLOWED) <<
  //    "You need to always allow exit_group() in your test policy";
  StartSandboxOrDie(evaluator);
  // TODO(jln): find a way to use the testing framework inside
  // SandboxedCode() or at the very least to surface errors
  SandboxedCode();
  // SandboxedCode() should have exited, this is a failure
  ExitGroup(1);
}

// evaluator should always allow ExitGroup
// SandboxedCode should ExitGroup(kExpectedReturnValue) if and only if
// things go as expected.
void TryPolicyInProcess(Sandbox::EvaluateSyscall evaluator,
                        void (*SandboxedCode)()) {
  // TODO(jln) figure out a way to surface whether we're actually testing
  // something or not.
  if (Sandbox::supportsSeccompSandbox(-1) == Sandbox::STATUS_AVAILABLE) {
    EXPECT_EXIT(RunInSandbox(evaluator, SandboxedCode),
                ::testing::ExitedWithCode(kExpectedReturnValue),
                "");
  }
}

// A simple blacklist test

Sandbox::ErrorCode BlacklistNanosleepPolicy(int sysno) {
  if (sysno < static_cast<int>(MIN_SYSCALL) ||
      sysno > static_cast<int>(MAX_SYSCALL)) {
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

void NanosleepProcess(void) {
  const struct timespec ts = {0, 0};
  errno = 0;
  if(syscall(__NR_nanosleep, &ts, NULL) != -1 || errno != EACCES) {
    ExitGroup(1);
  }
  ExitGroup(kExpectedReturnValue);
}

TEST(SandboxBpf, ApplyBasicBlacklistPolicy) {
  TryPolicyInProcess(BlacklistNanosleepPolicy, NanosleepProcess);
}

// Now do a simple whitelist test

Sandbox::ErrorCode WhitelistGetpidPolicy(int sysno) {
  switch (sysno) {
    case __NR_getpid:
    case __NR_exit_group:
      return Sandbox::SB_ALLOWED;
    default:
      return ENOMEM;
  }
}

void GetpidProcess(void) {
  errno = 0;
  // getpid() should be allowed
  if (syscall(__NR_getpid) < 0 || errno)
    ExitGroup(1);
  // getpgid() should be denied
  if (getpgid(0) != -1 || errno != ENOMEM)
    ExitGroup(1);
  ExitGroup(kExpectedReturnValue);
}

TEST(SandboxBpf, ApplyBasicWhitelistPolicy) {
  TryPolicyInProcess(WhitelistGetpidPolicy, GetpidProcess);
}

} // namespace
