// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"

using namespace sandbox::bpf_dsl;

// Helper macro to assert that invoking system call |sys| directly via
// Syscall::Call with arguments |...| returns |res|.
// Errors can be asserted by specifying a value like "-EINVAL".
#define ASSERT_SYSCALL_RESULT(res, sys, ...) \
  BPF_ASSERT_EQ(res, Stubs::sys(__VA_ARGS__))

namespace sandbox {
namespace {

// Type safe stubs for tested system calls.
class Stubs {
 public:
  static int getpgid(pid_t pid) { return Syscall::Call(__NR_getpgid, pid); }
  static int setuid(uid_t uid) { return Syscall::Call(__NR_setuid, uid); }
  static int setgid(gid_t gid) { return Syscall::Call(__NR_setgid, gid); }

  static int uname(struct utsname* buf) {
    return Syscall::Call(__NR_uname, buf);
  }

  static int setresuid(uid_t ruid, uid_t euid, uid_t suid) {
    return Syscall::Call(__NR_setresuid, ruid, euid, suid);
  }

#if !defined(ARCH_CPU_X86)
  static int socketpair(int domain, int type, int protocol, int sv[2]) {
    return Syscall::Call(__NR_socketpair, domain, type, protocol, sv);
  }
#endif
};

class BasicPolicy : public SandboxBPFDSLPolicy {
 public:
  BasicPolicy() {}
  virtual ~BasicPolicy() {}
  virtual ResultExpr EvaluateSyscall(int sysno) const OVERRIDE {
    if (sysno == __NR_getpgid) {
      const Arg<pid_t> pid(0);
      return If(pid == 0, Error(EPERM)).Else(Error(EINVAL));
    }
    return Allow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicPolicy);
};

BPF_TEST_C(BPFDSL, Basic, BasicPolicy) {
  ASSERT_SYSCALL_RESULT(-EPERM, getpgid, 0);
  ASSERT_SYSCALL_RESULT(-EINVAL, getpgid, 1);
}

/* On IA-32, socketpair() is implemented via socketcall(). :-( */
#if !defined(ARCH_CPU_X86)
class BooleanLogicPolicy : public SandboxBPFDSLPolicy {
 public:
  BooleanLogicPolicy() {}
  virtual ~BooleanLogicPolicy() {}
  virtual ResultExpr EvaluateSyscall(int sysno) const OVERRIDE {
    if (sysno == __NR_socketpair) {
      const Arg<int> domain(0), type(1), protocol(2);
      return If(domain == AF_UNIX &&
                    (type == SOCK_STREAM || type == SOCK_DGRAM) &&
                    protocol == 0,
                Error(EPERM)).Else(Error(EINVAL));
    }
    return Allow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BooleanLogicPolicy);
};

BPF_TEST_C(BPFDSL, BooleanLogic, BooleanLogicPolicy) {
  int sv[2];

  // Acceptable combinations that should return EPERM.
  ASSERT_SYSCALL_RESULT(-EPERM, socketpair, AF_UNIX, SOCK_STREAM, 0, sv);
  ASSERT_SYSCALL_RESULT(-EPERM, socketpair, AF_UNIX, SOCK_DGRAM, 0, sv);

  // Combinations that are invalid for only one reason; should return EINVAL.
  ASSERT_SYSCALL_RESULT(-EINVAL, socketpair, AF_INET, SOCK_STREAM, 0, sv);
  ASSERT_SYSCALL_RESULT(-EINVAL, socketpair, AF_UNIX, SOCK_SEQPACKET, 0, sv);
  ASSERT_SYSCALL_RESULT(
      -EINVAL, socketpair, AF_UNIX, SOCK_STREAM, IPPROTO_TCP, sv);

  // Completely unacceptable combination; should also return EINVAL.
  ASSERT_SYSCALL_RESULT(
      -EINVAL, socketpair, AF_INET, SOCK_SEQPACKET, IPPROTO_UDP, sv);
}
#endif  // !ARCH_CPU_X86

class MoreBooleanLogicPolicy : public SandboxBPFDSLPolicy {
 public:
  MoreBooleanLogicPolicy() {}
  virtual ~MoreBooleanLogicPolicy() {}
  virtual ResultExpr EvaluateSyscall(int sysno) const OVERRIDE {
    if (sysno == __NR_setresuid) {
      const Arg<uid_t> ruid(0), euid(1), suid(2);
      return If(ruid == 0 || euid == 0 || suid == 0, Error(EPERM))
          .ElseIf(ruid == 1 && euid == 1 && suid == 1, Error(EAGAIN))
          .Else(Error(EINVAL));
    }
    return Allow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MoreBooleanLogicPolicy);
};

BPF_TEST_C(BPFDSL, MoreBooleanLogic, MoreBooleanLogicPolicy) {
  // Expect EPERM if any set to 0.
  ASSERT_SYSCALL_RESULT(-EPERM, setresuid, 0, 5, 5);
  ASSERT_SYSCALL_RESULT(-EPERM, setresuid, 5, 0, 5);
  ASSERT_SYSCALL_RESULT(-EPERM, setresuid, 5, 5, 0);

  // Expect EAGAIN if all set to 1.
  ASSERT_SYSCALL_RESULT(-EAGAIN, setresuid, 1, 1, 1);

  // Expect EINVAL for anything else.
  ASSERT_SYSCALL_RESULT(-EINVAL, setresuid, 5, 1, 1);
  ASSERT_SYSCALL_RESULT(-EINVAL, setresuid, 1, 5, 1);
  ASSERT_SYSCALL_RESULT(-EINVAL, setresuid, 1, 1, 5);
  ASSERT_SYSCALL_RESULT(-EINVAL, setresuid, 3, 4, 5);
}

static const uintptr_t kDeadBeefAddr =
    static_cast<uintptr_t>(0xdeadbeefdeadbeefULL);

class ArgSizePolicy : public SandboxBPFDSLPolicy {
 public:
  ArgSizePolicy() {}
  virtual ~ArgSizePolicy() {}
  virtual ResultExpr EvaluateSyscall(int sysno) const OVERRIDE {
    if (sysno == __NR_uname) {
      const Arg<uintptr_t> addr(0);
      return If(addr == kDeadBeefAddr, Error(EPERM)).Else(Allow());
    }
    return Allow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArgSizePolicy);
};

BPF_TEST_C(BPFDSL, ArgSizeTest, ArgSizePolicy) {
  struct utsname buf;
  ASSERT_SYSCALL_RESULT(0, uname, &buf);
  ASSERT_SYSCALL_RESULT(
      -EPERM, uname, reinterpret_cast<struct utsname*>(kDeadBeefAddr));
}

class TrappingPolicy : public SandboxBPFDSLPolicy {
 public:
  TrappingPolicy() {}
  virtual ~TrappingPolicy() {}
  virtual ResultExpr EvaluateSyscall(int sysno) const OVERRIDE {
    if (sysno == __NR_uname) {
      return Trap(UnameTrap, &count_);
    }
    return Allow();
  }

 private:
  static intptr_t count_;

  static intptr_t UnameTrap(const struct arch_seccomp_data& data, void* aux) {
    BPF_ASSERT_EQ(&count_, aux);
    return ++count_;
  }

  DISALLOW_COPY_AND_ASSIGN(TrappingPolicy);
};

intptr_t TrappingPolicy::count_;

BPF_TEST_C(BPFDSL, TrapTest, TrappingPolicy) {
  ASSERT_SYSCALL_RESULT(1, uname, NULL);
  ASSERT_SYSCALL_RESULT(2, uname, NULL);
  ASSERT_SYSCALL_RESULT(3, uname, NULL);
}

class MaskingPolicy : public SandboxBPFDSLPolicy {
 public:
  MaskingPolicy() {}
  virtual ~MaskingPolicy() {}
  virtual ResultExpr EvaluateSyscall(int sysno) const OVERRIDE {
    if (sysno == __NR_setuid) {
      const Arg<uid_t> uid(0);
      return If((uid & 0xf) == 0, Error(EINVAL)).Else(Error(EACCES));
    }
    if (sysno == __NR_setgid) {
      const Arg<gid_t> gid(0);
      return If((gid & 0xf0) == 0xf0, Error(EINVAL)).Else(Error(EACCES));
    }
    return Allow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaskingPolicy);
};

BPF_TEST_C(BPFDSL, MaskTest, MaskingPolicy) {
  for (uid_t uid = 0; uid < 0x100; ++uid) {
    const int expect_errno = (uid & 0xf) == 0 ? EINVAL : EACCES;
    ASSERT_SYSCALL_RESULT(-expect_errno, setuid, uid);
  }

  for (gid_t gid = 0; gid < 0x100; ++gid) {
    const int expect_errno = (gid & 0xf0) == 0xf0 ? EINVAL : EACCES;
    ASSERT_SYSCALL_RESULT(-expect_errno, setgid, gid);
  }
}

class ElseIfPolicy : public SandboxBPFDSLPolicy {
 public:
  ElseIfPolicy() {}
  virtual ~ElseIfPolicy() {}
  virtual ResultExpr EvaluateSyscall(int sysno) const OVERRIDE {
    if (sysno == __NR_setuid) {
      const Arg<uid_t> uid(0);
      return If((uid & 0xfff) == 0, Error(0))
          .ElseIf((uid & 0xff0) == 0, Error(EINVAL))
          .ElseIf((uid & 0xf00) == 0, Error(EEXIST))
          .Else(Error(EACCES));
    }
    return Allow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ElseIfPolicy);
};

BPF_TEST_C(BPFDSL, ElseIfTest, ElseIfPolicy) {
  ASSERT_SYSCALL_RESULT(0, setuid, 0);

  ASSERT_SYSCALL_RESULT(-EINVAL, setuid, 0x0001);
  ASSERT_SYSCALL_RESULT(-EINVAL, setuid, 0x0002);

  ASSERT_SYSCALL_RESULT(-EEXIST, setuid, 0x0011);
  ASSERT_SYSCALL_RESULT(-EEXIST, setuid, 0x0022);

  ASSERT_SYSCALL_RESULT(-EACCES, setuid, 0x0111);
  ASSERT_SYSCALL_RESULT(-EACCES, setuid, 0x0222);
}

}  // namespace
}  // namespace sandbox
