// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

// Some headers on Android are missing cdefs: crbug.com/172337.
// (We can't use OS_ANDROID here since build_config.h is not included).
#if defined(ANDROID)
#include <sys/cdefs.h>
#endif

#include <errno.h>
#include <linux/filter.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/third_party/valgrind/valgrind.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/bpf_dsl/dump_bpf.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/bpf_dsl/syscall_set.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {

namespace {

bool IsRunningOnValgrind() { return RUNNING_ON_VALGRIND; }

bool IsSingleThreaded(int proc_fd) {
  return ThreadHelpers::IsSingleThreaded(proc_fd);
}

// Check if the kernel supports seccomp-filter (a.k.a. seccomp mode 2) via
// prctl().
bool KernelSupportsSeccompBPF() {
  errno = 0;
  const int rv = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, nullptr);

  if (rv == -1 && EFAULT == errno) {
    return true;
  }
  return false;
}

// Check if the kernel supports seccomp-filter via the seccomp system call
// and the TSYNC feature to enable seccomp on all threads.
bool KernelSupportsSeccompTsync() {
  errno = 0;
  const int rv =
      sys_seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, nullptr);

  if (rv == -1 && errno == EFAULT) {
    return true;
  } else {
    // TODO(jln): turn these into DCHECK after 417888 is considered fixed.
    CHECK_EQ(-1, rv);
    CHECK(ENOSYS == errno || EINVAL == errno);
    return false;
  }
}

uint64_t EscapePC() {
  intptr_t rv = Syscall::Call(-1);
  if (rv == -1 && errno == ENOSYS) {
    return 0;
  }
  return static_cast<uint64_t>(static_cast<uintptr_t>(rv));
}

}  // namespace

SandboxBPF::SandboxBPF(bpf_dsl::Policy* policy)
    : proc_fd_(), sandbox_has_started_(false), policy_(policy) {
}

SandboxBPF::~SandboxBPF() {
}

// static
bool SandboxBPF::SupportsSeccompSandbox(SeccompLevel level) {
  // Never pretend to support seccomp with Valgrind, as it
  // throws the tool off.
  if (IsRunningOnValgrind()) {
    return false;
  }

  switch (level) {
    case SeccompLevel::SINGLE_THREADED:
      return KernelSupportsSeccompBPF();
    case SeccompLevel::MULTI_THREADED:
      return KernelSupportsSeccompTsync();
  }
  NOTREACHED();
  return false;
}

bool SandboxBPF::StartSandbox(SeccompLevel seccomp_level) {
  DCHECK(policy_);
  CHECK(seccomp_level == SeccompLevel::SINGLE_THREADED ||
        seccomp_level == SeccompLevel::MULTI_THREADED);

  if (sandbox_has_started_) {
    SANDBOX_DIE(
        "Cannot repeatedly start sandbox. Create a separate Sandbox "
        "object instead.");
    return false;
  }

  if (!proc_fd_.is_valid()) {
    SetProcFd(ProcUtil::OpenProc());
  }

  const bool supports_tsync = KernelSupportsSeccompTsync();

  if (seccomp_level == SeccompLevel::SINGLE_THREADED) {
    // Wait for /proc/self/task/ to update if needed and assert the
    // process is single threaded.
    ThreadHelpers::AssertSingleThreaded(proc_fd_.get());
  } else if (seccomp_level == SeccompLevel::MULTI_THREADED) {
    if (IsSingleThreaded(proc_fd_.get())) {
      SANDBOX_DIE("Cannot start sandbox; "
                  "process may be single-threaded when reported as not");
      return false;
    }
    if (!supports_tsync) {
      SANDBOX_DIE("Cannot start sandbox; kernel does not support synchronizing "
                  "filters for a threadgroup");
      return false;
    }
  }

  // We no longer need access to any files in /proc. We want to do this
  // before installing the filters, just in case that our policy denies
  // close().
  if (proc_fd_.is_valid()) {
    proc_fd_.reset();
  }

  // Install the filters.
  InstallFilter(supports_tsync ||
                seccomp_level == SeccompLevel::MULTI_THREADED);

  return true;
}

void SandboxBPF::SetProcFd(base::ScopedFD proc_fd) {
  proc_fd_.swap(proc_fd);
}

// static
bool SandboxBPF::IsValidSyscallNumber(int sysnum) {
  return SyscallSet::IsValid(sysnum);
}

// static
bool SandboxBPF::IsRequiredForUnsafeTrap(int sysno) {
  return bpf_dsl::PolicyCompiler::IsRequiredForUnsafeTrap(sysno);
}

// static
intptr_t SandboxBPF::ForwardSyscall(const struct arch_seccomp_data& args) {
  return Syscall::Call(
      args.nr, static_cast<intptr_t>(args.args[0]),
      static_cast<intptr_t>(args.args[1]), static_cast<intptr_t>(args.args[2]),
      static_cast<intptr_t>(args.args[3]), static_cast<intptr_t>(args.args[4]),
      static_cast<intptr_t>(args.args[5]));
}

scoped_ptr<CodeGen::Program> SandboxBPF::AssembleFilter(
    bool force_verification) {
#if !defined(NDEBUG)
  force_verification = true;
#endif
  DCHECK(policy_);
  bpf_dsl::PolicyCompiler compiler(policy_.get(), Trap::Registry());
  if (Trap::SandboxDebuggingAllowedByUser()) {
    compiler.DangerousSetEscapePC(EscapePC());
  }
  scoped_ptr<CodeGen::Program> program = compiler.Compile();

  // Make sure compilation resulted in a BPF program that executes
  // correctly. Otherwise, there is an internal error in our BPF compiler.
  // There is really nothing the caller can do until the bug is fixed.
  if (force_verification) {
    // Verification is expensive. We only perform this step, if we are
    // compiled in debug mode, or if the caller explicitly requested
    // verification.

    const char* err = NULL;
    if (!Verifier::VerifyBPF(&compiler, *program, *policy_, &err)) {
      bpf_dsl::DumpBPF::PrintProgram(*program);
      SANDBOX_DIE(err);
    }
  }

  return program.Pass();
}

void SandboxBPF::InstallFilter(bool must_sync_threads) {
  // We want to be very careful in not imposing any requirements on the
  // policies that are set with SetSandboxPolicy(). This means, as soon as
  // the sandbox is active, we shouldn't be relying on libraries that could
  // be making system calls. This, for example, means we should avoid
  // using the heap and we should avoid using STL functions.
  // Temporarily copy the contents of the "program" vector into a
  // stack-allocated array; and then explicitly destroy that object.
  // This makes sure we don't ex- or implicitly call new/delete after we
  // installed the BPF filter program in the kernel. Depending on the
  // system memory allocator that is in effect, these operators can result
  // in system calls to things like munmap() or brk().
  CodeGen::Program* program = AssembleFilter(false).release();

  struct sock_filter bpf[program->size()];
  const struct sock_fprog prog = {static_cast<unsigned short>(program->size()),
                                  bpf};
  memcpy(bpf, &(*program)[0], sizeof(bpf));
  delete program;

  // Make an attempt to release memory that is no longer needed here, rather
  // than in the destructor. Try to avoid as much as possible to presume of
  // what will be possible to do in the new (sandboxed) execution environment.
  policy_.reset();

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    SANDBOX_DIE("Kernel refuses to enable no-new-privs");
  }

  // Install BPF filter program. If the thread state indicates multi-threading
  // support, then the kernel hass the seccomp system call. Otherwise, fall
  // back on prctl, which requires the process to be single-threaded.
  if (must_sync_threads) {
    int rv =
        sys_seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &prog);
    if (rv) {
      SANDBOX_DIE(
          "Kernel refuses to turn on and synchronize threads for BPF filters");
    }
  } else {
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
      SANDBOX_DIE("Kernel refuses to turn on BPF filters");
    }
  }

  sandbox_has_started_ = true;
}

}  // namespace sandbox
