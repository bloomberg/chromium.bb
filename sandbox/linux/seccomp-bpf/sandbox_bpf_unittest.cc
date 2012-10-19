// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace playground2;

namespace {

const int kExpectedReturnValue = 42;

// This test should execute no matter whether we have kernel support. So,
// we make it a TEST() instead of a BPF_TEST().
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

SANDBOX_TEST(SandboxBpf, CallSupportsTwice) {
  Sandbox::supportsSeccompSandbox(-1);
  Sandbox::supportsSeccompSandbox(-1);
}

// A simple blacklist test

ErrorCode BlacklistNanosleepPolicy(int sysno) {
  if (!Sandbox::isValidSyscallNumber(sysno)) {
    // FIXME: we should really not have to do that in a trivial policy
    return ErrorCode(ENOSYS);
  }

  switch (sysno) {
    case __NR_nanosleep:
      return ErrorCode(EACCES);
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(SandboxBpf, ApplyBasicBlacklistPolicy, BlacklistNanosleepPolicy) {
  // nanosleep() should be denied
  const struct timespec ts = {0, 0};
  errno = 0;
  BPF_ASSERT(syscall(__NR_nanosleep, &ts, NULL) == -1);
  BPF_ASSERT(errno == EACCES);
}

// Now do a simple whitelist test

ErrorCode WhitelistGetpidPolicy(int sysno) {
  switch (sysno) {
    case __NR_getpid:
    case __NR_exit_group:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    default:
      return ErrorCode(ENOMEM);
  }
}

BPF_TEST(SandboxBpf, ApplyBasicWhitelistPolicy, WhitelistGetpidPolicy) {
  // getpid() should be allowed
  errno = 0;
  BPF_ASSERT(syscall(__NR_getpid) > 0);
  BPF_ASSERT(errno == 0);

  // getpgid() should be denied
  BPF_ASSERT(getpgid(0) == -1);
  BPF_ASSERT(errno == ENOMEM);
}

// A simple blacklist policy, with a SIGSYS handler

// TODO: provide an API to provide the auxiliary data pointer
// to the evaluator

static int BlacklistNanosleepPolicySigsysAuxData;

intptr_t EnomemHandler(const struct arch_seccomp_data& args, void *aux) {
  // We also check that the auxiliary data is correct
  SANDBOX_ASSERT(aux);
  *(static_cast<int*>(aux)) = kExpectedReturnValue;
  return -ENOMEM;
}

ErrorCode BlacklistNanosleepPolicySigsys(int sysno) {
  if (!Sandbox::isValidSyscallNumber(sysno)) {
    // FIXME: we should really not have to do that in a trivial policy
    return ErrorCode(ENOSYS);
  }

  switch (sysno) {
    case __NR_nanosleep:
      return Sandbox::Trap(EnomemHandler,
                 static_cast<void *>(&BlacklistNanosleepPolicySigsysAuxData));
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(SandboxBpf, BasicBlacklistWithSigsys,
         BlacklistNanosleepPolicySigsys) {
  // getpid() should work properly
  errno = 0;
  BPF_ASSERT(syscall(__NR_getpid) > 0);
  BPF_ASSERT(errno == 0);

  // Our Auxiliary Data, should be reset by the signal handler
  BlacklistNanosleepPolicySigsysAuxData = -1;
  const struct timespec ts = {0, 0};
  BPF_ASSERT(syscall(__NR_nanosleep, &ts, NULL) == -1);
  BPF_ASSERT(errno == ENOMEM);

  // We expect the signal handler to modify AuxData
  BPF_ASSERT(
      BlacklistNanosleepPolicySigsysAuxData == kExpectedReturnValue);
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

ErrorCode SyntheticPolicy(int sysno) {
  if (!Sandbox::isValidSyscallNumber(sysno)) {
    // FIXME: we should really not have to do that in a trivial policy
    return ErrorCode(ENOSYS);
  }

// TODO(jorgelo): remove this once the new code generator lands.
#if defined(__arm__)
  if (sysno > static_cast<int>(MAX_PUBLIC_SYSCALL)) {
    return ErrorCode(ENOSYS);
  }
#endif

  if (sysno == __NR_exit_group || sysno == __NR_write) {
    // exit_group() is special, we really need it to work.
    // write() is needed for BPF_ASSERT() to report a useful error message.
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  } else {
    return ErrorCode(SysnoToRandomErrno(sysno));
  }
}

BPF_TEST(SandboxBpf, SyntheticPolicy, SyntheticPolicy) {
  // Ensure that that kExpectedReturnValue + syscallnumber + 1 does not int
  // overflow.
  BPF_ASSERT(
   std::numeric_limits<int>::max() - kExpectedReturnValue - 1 >=
   static_cast<int>(MAX_PUBLIC_SYSCALL));

  for (int syscall_number =  static_cast<int>(MIN_SYSCALL);
           syscall_number <= static_cast<int>(MAX_PUBLIC_SYSCALL);
         ++syscall_number) {
    if (syscall_number == __NR_exit_group ||
        syscall_number == __NR_write) {
      // exit_group() is special
      continue;
    }
    errno = 0;
    BPF_ASSERT(syscall(syscall_number) == -1);
    BPF_ASSERT(errno == SysnoToRandomErrno(syscall_number));
  }
}

#if defined(__arm__)
// A simple policy that tests whether ARM private system calls are supported
// by our BPF compiler and by the BPF interpreter in the kernel.

// For ARM private system calls, return an errno equal to their offset from
// MIN_PRIVATE_SYSCALL plus 1 (to avoid NUL errno).
int ArmPrivateSysnoToErrno(int sysno) {
  if (sysno >= static_cast<int>(MIN_PRIVATE_SYSCALL) &&
      sysno <= static_cast<int>(MAX_PRIVATE_SYSCALL)) {
    return (sysno - MIN_PRIVATE_SYSCALL) + 1;
  } else {
    return ENOSYS;
  }
}

ErrorCode ArmPrivatePolicy(int sysno) {
  if (!Sandbox::isValidSyscallNumber(sysno)) {
    // FIXME: we should really not have to do that in a trivial policy.
    return ErrorCode(ENOSYS);
  }

  // Start from |__ARM_NR_set_tls + 1| so as not to mess with actual
  // ARM private system calls.
  if (sysno >= static_cast<int>(__ARM_NR_set_tls + 1) &&
      sysno <= static_cast<int>(MAX_PRIVATE_SYSCALL)) {
    return ErrorCode(ArmPrivateSysnoToErrno(sysno));
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(SandboxBpf, ArmPrivatePolicy, ArmPrivatePolicy) {
  for (int syscall_number =  static_cast<int>(__ARM_NR_set_tls + 1);
           syscall_number <= static_cast<int>(MAX_PRIVATE_SYSCALL);
         ++syscall_number) {
    errno = 0;
    BPF_ASSERT(syscall(syscall_number) == -1);
    BPF_ASSERT(errno == ArmPrivateSysnoToErrno(syscall_number));
  }
}
#endif  // defined(__arm__)

} // namespace
