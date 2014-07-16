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

using namespace sandbox::bpf_dsl;

// Helper macro to assert that expression |expr| returns -1 and sets
// errno to |err|.
#define BPF_ASSERT_ERROR(err, expr) \
  do {                              \
    errno = 0;                      \
    BPF_ASSERT_EQ(-1, expr);        \
    BPF_ASSERT_EQ(err, errno);      \
  } while (0)

namespace sandbox {
namespace {

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
  BPF_ASSERT_ERROR(EPERM, getpgid(0));
  BPF_ASSERT_ERROR(EINVAL, getpgid(1));
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
  BPF_ASSERT_ERROR(EPERM, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
  BPF_ASSERT_ERROR(EPERM, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));

  // Combinations that are invalid for only one reason; should return EINVAL.
  BPF_ASSERT_ERROR(EINVAL, socketpair(AF_INET, SOCK_STREAM, 0, sv));
  BPF_ASSERT_ERROR(EINVAL, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv));
  BPF_ASSERT_ERROR(EINVAL, socketpair(AF_UNIX, SOCK_STREAM, IPPROTO_TCP, sv));

  // Completely unacceptable combination; should also return EINVAL.
  BPF_ASSERT_ERROR(EINVAL,
                   socketpair(AF_INET, SOCK_SEQPACKET, IPPROTO_UDP, sv));
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
  BPF_ASSERT_ERROR(EPERM, setresuid(0, 5, 5));
  BPF_ASSERT_ERROR(EPERM, setresuid(5, 0, 5));
  BPF_ASSERT_ERROR(EPERM, setresuid(5, 5, 0));

  // Expect EAGAIN if all set to 1.
  BPF_ASSERT_ERROR(EAGAIN, setresuid(1, 1, 1));

  // Expect EINVAL for anything else.
  BPF_ASSERT_ERROR(EINVAL, setresuid(5, 1, 1));
  BPF_ASSERT_ERROR(EINVAL, setresuid(1, 5, 1));
  BPF_ASSERT_ERROR(EINVAL, setresuid(1, 1, 5));
  BPF_ASSERT_ERROR(EINVAL, setresuid(3, 4, 5));
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
  BPF_ASSERT_EQ(0, uname(&buf));

  BPF_ASSERT_ERROR(EPERM,
                   uname(reinterpret_cast<struct utsname*>(kDeadBeefAddr)));
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
  BPF_ASSERT_EQ(1, uname(NULL));
  BPF_ASSERT_EQ(2, uname(NULL));
  BPF_ASSERT_EQ(3, uname(NULL));
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
    BPF_ASSERT_ERROR(expect_errno, setuid(uid));
  }

  for (gid_t gid = 0; gid < 0x100; ++gid) {
    const int expect_errno = (gid & 0xf0) == 0xf0 ? EINVAL : EACCES;
    BPF_ASSERT_ERROR(expect_errno, setgid(gid));
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
  BPF_ASSERT_EQ(0, setuid(0));

  BPF_ASSERT_ERROR(EINVAL, setuid(0x0001));
  BPF_ASSERT_ERROR(EINVAL, setuid(0x0002));

  BPF_ASSERT_ERROR(EEXIST, setuid(0x0011));
  BPF_ASSERT_ERROR(EEXIST, setuid(0x0022));

  BPF_ASSERT_ERROR(EACCES, setuid(0x0111));
  BPF_ASSERT_ERROR(EACCES, setuid(0x0222));
}

}  // namespace
}  // namespace sandbox
