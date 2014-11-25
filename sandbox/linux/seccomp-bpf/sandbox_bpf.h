// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
struct arch_seccomp_data;
namespace bpf_dsl {
class Policy;
}

class SANDBOX_EXPORT SandboxBPF {
 public:
  enum class SeccompLevel {
    SINGLE_THREADED,
    MULTI_THREADED,
  };

  // Constructors and destructors.
  // NOTE: Setting a policy and starting the sandbox is a one-way operation.
  // The kernel does not provide any option for unloading a loaded sandbox. The
  // sandbox remains engaged even when the object is destructed.
  SandboxBPF();
  ~SandboxBPF();

  // Checks whether a particular system call number is valid on the current
  // architecture. E.g. on ARM there's a non-contiguous range of private
  // system calls.
  static bool IsValidSyscallNumber(int sysnum);

  // Detect if the kernel supports the specified seccomp level.
  // See StartSandbox() for a description of these.
  static bool SupportsSeccompSandbox(SeccompLevel level);

  // The sandbox needs to be able to access files in "/proc/self/task/". If
  // this directory is not accessible when "startSandbox()" gets called, the
  // caller must provide an already opened file descriptor by calling
  // "set_proc_task_fd()".
  // The sandbox becomes the new owner of this file descriptor and will
  // eventually close it when "StartSandbox()" executes.
  void set_proc_task_fd(int proc_task_fd);

  // Set the BPF policy as |policy|. Ownership of |policy| is transfered here
  // to the sandbox object.
  void SetSandboxPolicy(bpf_dsl::Policy* policy);

  // UnsafeTraps require some syscalls to always be allowed.
  // This helper function returns true for these calls.
  static bool IsRequiredForUnsafeTrap(int sysno);

  // From within an UnsafeTrap() it is often useful to be able to execute
  // the system call that triggered the trap. The ForwardSyscall() method
  // makes this easy. It is more efficient than calling glibc's syscall()
  // function, as it avoid the extra round-trip to the signal handler. And
  // it automatically does the correct thing to report kernel-style error
  // conditions, rather than setting errno. See the comments for TrapFnc for
  // details. In other words, the return value from ForwardSyscall() is
  // directly suitable as a return value for a trap handler.
  static intptr_t ForwardSyscall(const struct arch_seccomp_data& args);

  // This is the main public entry point. It sets up the resources needed by
  // the sandbox, and enters Seccomp mode.
  // The calling process must provide a |level| to tell the sandbox which type
  // of kernel support it should engage.
  // SINGLE_THREADED will only sandbox the calling thread. Since it would be a
  // security risk, the sandbox will also check that the current process is
  // single threaded.
  // MULTI_THREADED requires more recent kernel support and allows to sandbox
  // all the threads of the current process. Be mindful of potential races,
  // with other threads using disallowed system calls either before or after
  // the sandbox is engaged.
  //
  // It is possible to stack multiple sandboxes by creating separate "Sandbox"
  // objects and calling "StartSandbox()" on each of them. Please note, that
  // this requires special care, though, as newly stacked sandboxes can never
  // relax restrictions imposed by earlier sandboxes. Furthermore, installing
  // a new policy requires making system calls, that might already be
  // disallowed.
  // Finally, stacking does add more kernel overhead than having a single
  // combined policy. So, it should only be used if there are no alternatives.
  bool StartSandbox(SeccompLevel level) WARN_UNUSED_RESULT;

  // Assembles a BPF filter program from the current policy. After calling this
  // function, you must not call any other sandboxing function.
  // Typically, AssembleFilter() is only used by unit tests and by sandbox
  // internals. It should not be used by production code.
  // For performance reasons, we normally only run the assembled BPF program
  // through the verifier, iff the program was built in debug mode.
  // But by setting "force_verification", the caller can request that the
  // verifier is run unconditionally. This is useful for unittests.
  scoped_ptr<CodeGen::Program> AssembleFilter(bool force_verification);

 private:
  // Get a file descriptor pointing to "/proc", if currently available.
  int proc_task_fd() { return proc_task_fd_; }

  // Creates a subprocess and runs "code_in_sandbox" inside of the specified
  // policy. The caller has to make sure that "this" has not yet been
  // initialized with any other policies.
  bool RunFunctionInPolicy(void (*code_in_sandbox)(),
                           scoped_ptr<bpf_dsl::Policy> policy);

  // Assembles and installs a filter based on the policy that has previously
  // been configured with SetSandboxPolicy().
  void InstallFilter(bool must_sync_threads);

  // Verify the correctness of a compiled program by comparing it against the
  // current policy. This function should only ever be called by unit tests and
  // by the sandbox internals. It should not be used by production code.
  void VerifyProgram(const CodeGen::Program& program);

  bool quiet_;
  int proc_task_fd_;
  bool sandbox_has_started_;
  scoped_ptr<bpf_dsl::Policy> policy_;

  DISALLOW_COPY_AND_ASSIGN(SandboxBPF);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__
