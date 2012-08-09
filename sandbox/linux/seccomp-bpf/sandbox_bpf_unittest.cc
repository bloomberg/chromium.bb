// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace playground2;

namespace {

const int kExpectedReturnValue = 42;

TEST(SandboxBpf, CallSupports) {
  // We check that we don't crash, but it's ok if the kernel doesn't
  // support it.
  bool seccomp_bpf_supported =
      Sandbox::supportsSeccompSandbox(-1) == Sandbox::STATUS_AVAILABLE;
  // We want to log whether or not seccomp BPF is actually supported
  // since actual test coverage depends on it.
  RecordProperty("SeccompBPFSupported",
                 seccomp_bpf_supported ? "true." : "false.");
  std::cout << "Seccomp BPF supported: "
            << (seccomp_bpf_supported ? "true." : "false.")
            << "\n";
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
  // TODO(markus): Implement IsEqual for ErrorCode
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
  } else {
    // The sandbox is not available. We should still try to exercise what we
    // can.
    // TODO(markus): (crbug.com/141545) let us call the compiler from here.
    Sandbox::setSandboxPolicy(evaluator, NULL);
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

// A simple blacklist policy, with a SIGSYS handler

// TODO: provide an API to provide the auxiliary data pointer
// to the evaluator

static int BlacklistNanosleepPolicySigsysAuxData;

intptr_t EnomemHandler(const struct arch_seccomp_data& args, void *aux) {
  // We also check that the auxiliary data is correct
  if (!aux)
    ExitGroup(1);
  *(static_cast<int*>(aux)) = kExpectedReturnValue;
  return -ENOMEM;
}

Sandbox::ErrorCode BlacklistNanosleepPolicySigsys(int sysno) {
  if (sysno < static_cast<int>(MIN_SYSCALL) ||
      sysno > static_cast<int>(MAX_SYSCALL)) {
    // FIXME: we should really not have to do that in a trivial policy
    return ENOSYS;
  }
  switch (sysno) {
    case __NR_nanosleep:
      return Sandbox::ErrorCode(EnomemHandler,
                 static_cast<void *>(&BlacklistNanosleepPolicySigsysAuxData));
    default:
      return Sandbox::SB_ALLOWED;
  }
}

void NanosleepProcessSigsys(void) {
  const struct timespec ts = {0, 0};
  errno = 0;
  // getpid() should work properly
  if (syscall(__NR_getpid) < 0)
    ExitGroup(1);
  // Our Auxiliary Data, should be reset by the signal handler
  BlacklistNanosleepPolicySigsysAuxData = -1;
  errno = 0;
  if (syscall(__NR_nanosleep, &ts, NULL) != -1 || errno != ENOMEM)
    ExitGroup(1);
  // We expect the signal handler to modify AuxData
  if (BlacklistNanosleepPolicySigsysAuxData != kExpectedReturnValue)
    ExitGroup(1);
  else
    ExitGroup(kExpectedReturnValue);
}

TEST(SandboxBpf, BasicBlacklistWithSigsys) {
  TryPolicyInProcess(BlacklistNanosleepPolicySigsys, NanosleepProcessSigsys);
}

// A more complex, but synthetic policy. This tests the correctness of the BPF
// program by iterating through all syscalls and checking for an errno that
// depends on the syscall number. Unlike the Verifier, this exercises the BPF
// interpreter in the kernel.

// We try to make sure we exercise optimizations in the BPF compiler. We make
// sure that the compiler can have an opportunity to coalesce syscalls with
// contiguous numbers and we also make sure that disjoint sets can return the
// same errno.
int SysnoToRandomErrno(int sysno) {
  // Small contiguous sets of 3 system calls return an errno equal to the
  // index of that set + 1 (so that we never return a NUL errno).
  return ((sysno & ~3) >> 2) % 29 + 1;
}

Sandbox::ErrorCode SyntheticPolicy(int sysno) {
  if (sysno < static_cast<int>(MIN_SYSCALL) ||
      sysno > static_cast<int>(MAX_SYSCALL)) {
    // FIXME: we should really not have to do that in a trivial policy.
    return ENOSYS;
  }
  if (sysno == __NR_exit_group) {
    // exit_group() is special, we really need it to work.
    return Sandbox::SB_ALLOWED;
  } else {
    return SysnoToRandomErrno(sysno);
  }
}

void SyntheticProcess(void) {
  // Ensure that that kExpectedReturnValue + syscallnumber + 1 does not int
  // overflow.
  if (std::numeric_limits<int>::max() - kExpectedReturnValue - 1 <
      static_cast<int>(MAX_SYSCALL)) {
    ExitGroup(1);
  }
  for (int syscall_number =  static_cast<int>(MIN_SYSCALL);
           syscall_number <= static_cast<int>(MAX_SYSCALL);
         ++syscall_number) {
    if (syscall_number == __NR_exit_group) {
      // exit_group() is special
      continue;
    }
    errno = 0;
    if (syscall(syscall_number) != -1 ||
        errno != SysnoToRandomErrno(syscall_number)) {
      // Exit with a return value that is different than kExpectedReturnValue
      // to signal an error. Make it easy to see what syscall_number failed in
      // the test report.
      ExitGroup(kExpectedReturnValue + syscall_number + 1);
    }
  }
  ExitGroup(kExpectedReturnValue);
}

TEST(SandboxBpf, SyntheticPolicy) {
  TryPolicyInProcess(SyntheticPolicy, SyntheticProcess);
}

} // namespace
