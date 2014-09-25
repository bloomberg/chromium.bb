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
#include <fcntl.h>
#include <linux/filter.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <limits>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/instruction.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "sandbox/linux/services/linux_syscalls.h"

namespace sandbox {

namespace {

const int kExpectedExitCode = 100;

#if defined(__i386__) || defined(__x86_64__)
const bool kIsIntel = true;
#else
const bool kIsIntel = false;
#endif
#if defined(__x86_64__) && defined(__ILP32__)
const bool kIsX32 = true;
#else
const bool kIsX32 = false;
#endif

const int kSyscallsRequiredForUnsafeTraps[] = {
  __NR_rt_sigprocmask,
  __NR_rt_sigreturn,
#if defined(__NR_sigprocmask)
  __NR_sigprocmask,
#endif
#if defined(__NR_sigreturn)
  __NR_sigreturn,
#endif
};

bool HasExactlyOneBit(uint64_t x) {
  // Common trick; e.g., see http://stackoverflow.com/a/108329.
  return x != 0 && (x & (x - 1)) == 0;
}

#if !defined(NDEBUG)
void WriteFailedStderrSetupMessage(int out_fd) {
  const char* error_string = strerror(errno);
  static const char msg[] =
      "You have reproduced a puzzling issue.\n"
      "Please, report to crbug.com/152530!\n"
      "Failed to set up stderr: ";
  if (HANDLE_EINTR(write(out_fd, msg, sizeof(msg) - 1)) > 0 && error_string &&
      HANDLE_EINTR(write(out_fd, error_string, strlen(error_string))) > 0 &&
      HANDLE_EINTR(write(out_fd, "\n", 1))) {
  }
}
#endif  // !defined(NDEBUG)

// We define a really simple sandbox policy. It is just good enough for us
// to tell that the sandbox has actually been activated.
class ProbePolicy : public SandboxBPFPolicy {
 public:
  ProbePolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysnum) const OVERRIDE {
    switch (sysnum) {
      case __NR_getpid:
        // Return EPERM so that we can check that the filter actually ran.
        return ErrorCode(EPERM);
      case __NR_exit_group:
        // Allow exit() with a non-default return code.
        return ErrorCode(ErrorCode::ERR_ALLOWED);
      default:
        // Make everything else fail in an easily recognizable way.
        return ErrorCode(EINVAL);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProbePolicy);
};

void ProbeProcess(void) {
  if (syscall(__NR_getpid) < 0 && errno == EPERM) {
    syscall(__NR_exit_group, static_cast<intptr_t>(kExpectedExitCode));
  }
}

class AllowAllPolicy : public SandboxBPFPolicy {
 public:
  AllowAllPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysnum) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysnum));
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AllowAllPolicy);
};

void TryVsyscallProcess(void) {
  time_t current_time;
  // time() is implemented as a vsyscall. With an older glibc, with
  // vsyscall=emulate and some versions of the seccomp BPF patch
  // we may get SIGKILL-ed. Detect this!
  if (time(&current_time) != static_cast<time_t>(-1)) {
    syscall(__NR_exit_group, static_cast<intptr_t>(kExpectedExitCode));
  }
}

bool IsSingleThreaded(int proc_fd) {
  if (proc_fd < 0) {
    // Cannot determine whether program is single-threaded. Hope for
    // the best...
    return true;
  }

  struct stat sb;
  int task = -1;
  if ((task = openat(proc_fd, "self/task", O_RDONLY | O_DIRECTORY)) < 0 ||
      fstat(task, &sb) != 0 || sb.st_nlink != 3 || IGNORE_EINTR(close(task))) {
    if (task >= 0) {
      if (IGNORE_EINTR(close(task))) {
      }
    }
    return false;
  }
  return true;
}

bool IsDenied(const ErrorCode& code) {
  return (code.err() & SECCOMP_RET_ACTION) == SECCOMP_RET_TRAP ||
         (code.err() >= (SECCOMP_RET_ERRNO + ErrorCode::ERR_MIN_ERRNO) &&
          code.err() <= (SECCOMP_RET_ERRNO + ErrorCode::ERR_MAX_ERRNO));
}

// Function that can be passed as a callback function to CodeGen::Traverse().
// Checks whether the "insn" returns an UnsafeTrap() ErrorCode. If so, it
// sets the "bool" variable pointed to by "aux".
void CheckForUnsafeErrorCodes(Instruction* insn, void* aux) {
  bool* is_unsafe = static_cast<bool*>(aux);
  if (!*is_unsafe) {
    if (BPF_CLASS(insn->code) == BPF_RET && insn->k > SECCOMP_RET_TRAP &&
        insn->k - SECCOMP_RET_TRAP <= SECCOMP_RET_DATA) {
      if (!Trap::IsSafeTrapId(insn->k & SECCOMP_RET_DATA)) {
        *is_unsafe = true;
      }
    }
  }
}

// A Trap() handler that returns an "errno" value. The value is encoded
// in the "aux" parameter.
intptr_t ReturnErrno(const struct arch_seccomp_data&, void* aux) {
  // TrapFnc functions report error by following the native kernel convention
  // of returning an exit code in the range of -1..-4096. They do not try to
  // set errno themselves. The glibc wrapper that triggered the SIGSYS will
  // ultimately do so for us.
  int err = reinterpret_cast<intptr_t>(aux) & SECCOMP_RET_DATA;
  return -err;
}

// Function that can be passed as a callback function to CodeGen::Traverse().
// Checks whether the "insn" returns an errno value from a BPF filter. If so,
// it rewrites the instruction to instead call a Trap() handler that does
// the same thing. "aux" is ignored.
void RedirectToUserspace(Instruction* insn, void* aux) {
  // When inside an UnsafeTrap() callback, we want to allow all system calls.
  // This means, we must conditionally disable the sandbox -- and that's not
  // something that kernel-side BPF filters can do, as they cannot inspect
  // any state other than the syscall arguments.
  // But if we redirect all error handlers to user-space, then we can easily
  // make this decision.
  // The performance penalty for this extra round-trip to user-space is not
  // actually that bad, as we only ever pay it for denied system calls; and a
  // typical program has very few of these.
  SandboxBPF* sandbox = static_cast<SandboxBPF*>(aux);
  if (BPF_CLASS(insn->code) == BPF_RET &&
      (insn->k & SECCOMP_RET_ACTION) == SECCOMP_RET_ERRNO) {
    insn->k = sandbox->Trap(ReturnErrno,
        reinterpret_cast<void*>(insn->k & SECCOMP_RET_DATA)).err();
  }
}

// This wraps an existing policy and changes its behavior to match the changes
// made by RedirectToUserspace(). This is part of the framework that allows BPF
// evaluation in userland.
// TODO(markus): document the code inside better.
class RedirectToUserSpacePolicyWrapper : public SandboxBPFPolicy {
 public:
  explicit RedirectToUserSpacePolicyWrapper(
      const SandboxBPFPolicy* wrapped_policy)
      : wrapped_policy_(wrapped_policy) {
    DCHECK(wrapped_policy_);
  }

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE {
    ErrorCode err =
        wrapped_policy_->EvaluateSyscall(sandbox_compiler, system_call_number);
    ChangeErrnoToTraps(&err, sandbox_compiler);
    return err;
  }

  virtual ErrorCode InvalidSyscall(
      SandboxBPF* sandbox_compiler) const OVERRIDE {
    return ReturnErrnoViaTrap(sandbox_compiler, ENOSYS);
  }

 private:
  ErrorCode ReturnErrnoViaTrap(SandboxBPF* sandbox_compiler, int err) const {
    return sandbox_compiler->Trap(ReturnErrno, reinterpret_cast<void*>(err));
  }

  // ChangeErrnoToTraps recursivly iterates through the ErrorCode
  // converting any ERRNO to a userspace trap
  void ChangeErrnoToTraps(ErrorCode* err, SandboxBPF* sandbox_compiler) const {
    if (err->error_type() == ErrorCode::ET_SIMPLE &&
        (err->err() & SECCOMP_RET_ACTION) == SECCOMP_RET_ERRNO) {
      // Have an errno, need to change this to a trap
      *err =
          ReturnErrnoViaTrap(sandbox_compiler, err->err() & SECCOMP_RET_DATA);
      return;
    } else if (err->error_type() == ErrorCode::ET_COND) {
      // Need to explore both paths
      ChangeErrnoToTraps((ErrorCode*)err->passed(), sandbox_compiler);
      ChangeErrnoToTraps((ErrorCode*)err->failed(), sandbox_compiler);
      return;
    } else if (err->error_type() == ErrorCode::ET_TRAP) {
      return;
    } else if (err->error_type() == ErrorCode::ET_SIMPLE &&
               (err->err() & SECCOMP_RET_ACTION) == SECCOMP_RET_ALLOW) {
      return;
    }
    NOTREACHED();
  }

  const SandboxBPFPolicy* wrapped_policy_;
  DISALLOW_COPY_AND_ASSIGN(RedirectToUserSpacePolicyWrapper);
};

intptr_t BPFFailure(const struct arch_seccomp_data&, void* aux) {
  SANDBOX_DIE(static_cast<char*>(aux));
}

}  // namespace

SandboxBPF::SandboxBPF()
    : quiet_(false),
      proc_fd_(-1),
      conds_(new Conds),
      sandbox_has_started_(false) {}

SandboxBPF::~SandboxBPF() {
  // It is generally unsafe to call any memory allocator operations or to even
  // call arbitrary destructors after having installed a new policy. We just
  // have no way to tell whether this policy would allow the system calls that
  // the constructors can trigger.
  // So, we normally destroy all of our complex state prior to starting the
  // sandbox. But this won't happen, if the Sandbox object was created and
  // never actually used to set up a sandbox. So, just in case, we are
  // destroying any remaining state.
  // The "if ()" statements are technically superfluous. But let's be explicit
  // that we really don't want to run any code, when we already destroyed
  // objects before setting up the sandbox.
  if (conds_) {
    delete conds_;
  }
}

bool SandboxBPF::IsValidSyscallNumber(int sysnum) {
  return SyscallIterator::IsValid(sysnum);
}

bool SandboxBPF::RunFunctionInPolicy(void (*code_in_sandbox)(),
                                     scoped_ptr<SandboxBPFPolicy> policy) {
  // Block all signals before forking a child process. This prevents an
  // attacker from manipulating our test by sending us an unexpected signal.
  sigset_t old_mask, new_mask;
  if (sigfillset(&new_mask) || sigprocmask(SIG_BLOCK, &new_mask, &old_mask)) {
    SANDBOX_DIE("sigprocmask() failed");
  }
  int fds[2];
  if (pipe2(fds, O_NONBLOCK | O_CLOEXEC)) {
    SANDBOX_DIE("pipe() failed");
  }

  if (fds[0] <= 2 || fds[1] <= 2) {
    SANDBOX_DIE("Process started without standard file descriptors");
  }

  // This code is using fork() and should only ever run single-threaded.
  // Most of the code below is "async-signal-safe" and only minor changes
  // would be needed to support threads.
  DCHECK(IsSingleThreaded(proc_fd_));
  pid_t pid = fork();
  if (pid < 0) {
    // Die if we cannot fork(). We would probably fail a little later
    // anyway, as the machine is likely very close to running out of
    // memory.
    // But what we don't want to do is return "false", as a crafty
    // attacker might cause fork() to fail at will and could trick us
    // into running without a sandbox.
    sigprocmask(SIG_SETMASK, &old_mask, NULL);  // OK, if it fails
    SANDBOX_DIE("fork() failed unexpectedly");
  }

  // In the child process
  if (!pid) {
    // Test a very simple sandbox policy to verify that we can
    // successfully turn on sandboxing.
    Die::EnableSimpleExit();

    errno = 0;
    if (IGNORE_EINTR(close(fds[0]))) {
      // This call to close() has been failing in strange ways. See
      // crbug.com/152530. So we only fail in debug mode now.
#if !defined(NDEBUG)
      WriteFailedStderrSetupMessage(fds[1]);
      SANDBOX_DIE(NULL);
#endif
    }
    if (HANDLE_EINTR(dup2(fds[1], 2)) != 2) {
      // Stderr could very well be a file descriptor to .xsession-errors, or
      // another file, which could be backed by a file system that could cause
      // dup2 to fail while trying to close stderr. It's important that we do
      // not fail on trying to close stderr.
      // If dup2 fails here, we will continue normally, this means that our
      // parent won't cause a fatal failure if something writes to stderr in
      // this child.
#if !defined(NDEBUG)
      // In DEBUG builds, we still want to get a report.
      WriteFailedStderrSetupMessage(fds[1]);
      SANDBOX_DIE(NULL);
#endif
    }
    if (IGNORE_EINTR(close(fds[1]))) {
      // This call to close() has been failing in strange ways. See
      // crbug.com/152530. So we only fail in debug mode now.
#if !defined(NDEBUG)
      WriteFailedStderrSetupMessage(fds[1]);
      SANDBOX_DIE(NULL);
#endif
    }

    SetSandboxPolicy(policy.release());
    if (!StartSandbox(PROCESS_SINGLE_THREADED)) {
      SANDBOX_DIE(NULL);
    }

    // Run our code in the sandbox.
    code_in_sandbox();

    // code_in_sandbox() is not supposed to return here.
    SANDBOX_DIE(NULL);
  }

  // In the parent process.
  if (IGNORE_EINTR(close(fds[1]))) {
    SANDBOX_DIE("close() failed");
  }
  if (sigprocmask(SIG_SETMASK, &old_mask, NULL)) {
    SANDBOX_DIE("sigprocmask() failed");
  }
  int status;
  if (HANDLE_EINTR(waitpid(pid, &status, 0)) != pid) {
    SANDBOX_DIE("waitpid() failed unexpectedly");
  }
  bool rc = WIFEXITED(status) && WEXITSTATUS(status) == kExpectedExitCode;

  // If we fail to support sandboxing, there might be an additional
  // error message. If so, this was an entirely unexpected and fatal
  // failure. We should report the failure and somebody must fix
  // things. This is probably a security-critical bug in the sandboxing
  // code.
  if (!rc) {
    char buf[4096];
    ssize_t len = HANDLE_EINTR(read(fds[0], buf, sizeof(buf) - 1));
    if (len > 0) {
      while (len > 1 && buf[len - 1] == '\n') {
        --len;
      }
      buf[len] = '\000';
      SANDBOX_DIE(buf);
    }
  }
  if (IGNORE_EINTR(close(fds[0]))) {
    SANDBOX_DIE("close() failed");
  }

  return rc;
}

bool SandboxBPF::KernelSupportSeccompBPF() {
  return RunFunctionInPolicy(ProbeProcess,
                             scoped_ptr<SandboxBPFPolicy>(new ProbePolicy())) &&
         RunFunctionInPolicy(
             TryVsyscallProcess,
             scoped_ptr<SandboxBPFPolicy>(new AllowAllPolicy()));
}

// static
SandboxBPF::SandboxStatus SandboxBPF::SupportsSeccompSandbox(int proc_fd) {
  // It the sandbox is currently active, we clearly must have support for
  // sandboxing.
  if (status_ == STATUS_ENABLED) {
    return status_;
  }

  // Even if the sandbox was previously available, something might have
  // changed in our run-time environment. Check one more time.
  if (status_ == STATUS_AVAILABLE) {
    if (!IsSingleThreaded(proc_fd)) {
      status_ = STATUS_UNAVAILABLE;
    }
    return status_;
  }

  if (status_ == STATUS_UNAVAILABLE && IsSingleThreaded(proc_fd)) {
    // All state transitions resulting in STATUS_UNAVAILABLE are immediately
    // preceded by STATUS_AVAILABLE. Furthermore, these transitions all
    // happen, if and only if they are triggered by the process being multi-
    // threaded.
    // In other words, if a single-threaded process is currently in the
    // STATUS_UNAVAILABLE state, it is safe to assume that sandboxing is
    // actually available.
    status_ = STATUS_AVAILABLE;
    return status_;
  }

  // If we have not previously checked for availability of the sandbox or if
  // we otherwise don't believe to have a good cached value, we have to
  // perform a thorough check now.
  if (status_ == STATUS_UNKNOWN) {
    // We create our own private copy of a "Sandbox" object. This ensures that
    // the object does not have any policies configured, that might interfere
    // with the tests done by "KernelSupportSeccompBPF()".
    SandboxBPF sandbox;

    // By setting "quiet_ = true" we suppress messages for expected and benign
    // failures (e.g. if the current kernel lacks support for BPF filters).
    sandbox.quiet_ = true;
    sandbox.set_proc_fd(proc_fd);
    status_ = sandbox.KernelSupportSeccompBPF() ? STATUS_AVAILABLE
                                                : STATUS_UNSUPPORTED;

    // As we are performing our tests from a child process, the run-time
    // environment that is visible to the sandbox is always guaranteed to be
    // single-threaded. Let's check here whether the caller is single-
    // threaded. Otherwise, we mark the sandbox as temporarily unavailable.
    if (status_ == STATUS_AVAILABLE && !IsSingleThreaded(proc_fd)) {
      status_ = STATUS_UNAVAILABLE;
    }
  }
  return status_;
}

// static
SandboxBPF::SandboxStatus
SandboxBPF::SupportsSeccompThreadFilterSynchronization() {
  // Applying NO_NEW_PRIVS, a BPF filter, and synchronizing the filter across
  // the thread group are all handled atomically by this syscall.
  const int rv = syscall(
      __NR_seccomp, SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, NULL);

  if (rv == -1 && errno == EFAULT) {
    return STATUS_AVAILABLE;
  } else {
    // TODO(jln): turn these into DCHECK after 417888 is considered fixed.
    CHECK_EQ(-1, rv);
    CHECK(ENOSYS == errno || EINVAL == errno);
    return STATUS_UNSUPPORTED;
  }
}

void SandboxBPF::set_proc_fd(int proc_fd) { proc_fd_ = proc_fd; }

bool SandboxBPF::StartSandbox(SandboxThreadState thread_state) {
  CHECK(thread_state == PROCESS_SINGLE_THREADED ||
        thread_state == PROCESS_MULTI_THREADED);

  if (status_ == STATUS_UNSUPPORTED || status_ == STATUS_UNAVAILABLE) {
    SANDBOX_DIE(
        "Trying to start sandbox, even though it is known to be "
        "unavailable");
    return false;
  } else if (sandbox_has_started_ || !conds_) {
    SANDBOX_DIE(
        "Cannot repeatedly start sandbox. Create a separate Sandbox "
        "object instead.");
    return false;
  }
  if (proc_fd_ < 0) {
    proc_fd_ = open("/proc", O_RDONLY | O_DIRECTORY);
  }
  if (proc_fd_ < 0) {
    // For now, continue in degraded mode, if we can't access /proc.
    // In the future, we might want to tighten this requirement.
  }

  bool supports_tsync =
      SupportsSeccompThreadFilterSynchronization() == STATUS_AVAILABLE;

  if (thread_state == PROCESS_SINGLE_THREADED) {
    if (!IsSingleThreaded(proc_fd_)) {
      SANDBOX_DIE("Cannot start sandbox; process is already multi-threaded");
      return false;
    }
  } else if (thread_state == PROCESS_MULTI_THREADED) {
    if (IsSingleThreaded(proc_fd_)) {
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
  if (proc_fd_ >= 0) {
    if (IGNORE_EINTR(close(proc_fd_))) {
      SANDBOX_DIE("Failed to close file descriptor for /proc");
      return false;
    }
    proc_fd_ = -1;
  }

  // Install the filters.
  InstallFilter(supports_tsync || thread_state == PROCESS_MULTI_THREADED);

  // We are now inside the sandbox.
  status_ = STATUS_ENABLED;

  return true;
}

void SandboxBPF::PolicySanityChecks(SandboxBPFPolicy* policy) {
  if (!IsDenied(policy->InvalidSyscall(this))) {
    SANDBOX_DIE("Policies should deny invalid system calls.");
  }
  return;
}

// Don't take a scoped_ptr here, polymorphism make their use awkward.
void SandboxBPF::SetSandboxPolicy(SandboxBPFPolicy* policy) {
  DCHECK(!policy_);
  if (sandbox_has_started_ || !conds_) {
    SANDBOX_DIE("Cannot change policy after sandbox has started");
  }
  PolicySanityChecks(policy);
  policy_.reset(policy);
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
  Program* program = AssembleFilter(false /* force_verification */);

  struct sock_filter bpf[program->size()];
  const struct sock_fprog prog = {static_cast<unsigned short>(program->size()),
                                  bpf};
  memcpy(bpf, &(*program)[0], sizeof(bpf));
  delete program;

  // Make an attempt to release memory that is no longer needed here, rather
  // than in the destructor. Try to avoid as much as possible to presume of
  // what will be possible to do in the new (sandboxed) execution environment.
  delete conds_;
  conds_ = NULL;
  policy_.reset();

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    SANDBOX_DIE(quiet_ ? NULL : "Kernel refuses to enable no-new-privs");
  }

  // Install BPF filter program. If the thread state indicates multi-threading
  // support, then the kernel hass the seccomp system call. Otherwise, fall
  // back on prctl, which requires the process to be single-threaded.
  if (must_sync_threads) {
    int rv = syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER,
        SECCOMP_FILTER_FLAG_TSYNC, reinterpret_cast<const char*>(&prog));
    if (rv) {
      SANDBOX_DIE(quiet_ ? NULL :
          "Kernel refuses to turn on and synchronize threads for BPF filters");
    }
  } else {
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
      SANDBOX_DIE(quiet_ ? NULL : "Kernel refuses to turn on BPF filters");
    }
  }

  sandbox_has_started_ = true;
}

SandboxBPF::Program* SandboxBPF::AssembleFilter(bool force_verification) {
#if !defined(NDEBUG)
  force_verification = true;
#endif

  // Verify that the user pushed a policy.
  DCHECK(policy_);

  // Assemble the BPF filter program.
  CodeGen* gen = new CodeGen();
  if (!gen) {
    SANDBOX_DIE("Out of memory");
  }

  bool has_unsafe_traps;
  Instruction* head = CompilePolicy(gen, &has_unsafe_traps);

  // Turn the DAG into a vector of instructions.
  Program* program = new Program();
  gen->Compile(head, program);
  delete gen;

  // Make sure compilation resulted in BPF program that executes
  // correctly. Otherwise, there is an internal error in our BPF compiler.
  // There is really nothing the caller can do until the bug is fixed.
  if (force_verification) {
    // Verification is expensive. We only perform this step, if we are
    // compiled in debug mode, or if the caller explicitly requested
    // verification.
    VerifyProgram(*program, has_unsafe_traps);
  }

  return program;
}

Instruction* SandboxBPF::CompilePolicy(CodeGen* gen, bool* has_unsafe_traps) {
  // A compiled policy consists of three logical parts:
  //   1. Check that the "arch" field matches the expected architecture.
  //   2. If the policy involves unsafe traps, check if the syscall was
  //      invoked by Syscall::Call, and then allow it unconditionally.
  //   3. Check the system call number and jump to the appropriate compiled
  //      system call policy number.
  return CheckArch(
      gen, MaybeAddEscapeHatch(gen, has_unsafe_traps, DispatchSyscall(gen)));
}

Instruction* SandboxBPF::CheckArch(CodeGen* gen, Instruction* passed) {
  // If the architecture doesn't match SECCOMP_ARCH, disallow the
  // system call.
  return gen->MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      SECCOMP_ARCH_IDX,
      gen->MakeInstruction(
          BPF_JMP + BPF_JEQ + BPF_K,
          SECCOMP_ARCH,
          passed,
          RetExpression(gen,
                        Kill("Invalid audit architecture in BPF filter"))));
}

Instruction* SandboxBPF::MaybeAddEscapeHatch(CodeGen* gen,
                                             bool* has_unsafe_traps,
                                             Instruction* rest) {
  // If there is at least one UnsafeTrap() in our program, the entire sandbox
  // is unsafe. We need to modify the program so that all non-
  // SECCOMP_RET_ALLOW ErrorCodes are handled in user-space. This will then
  // allow us to temporarily disable sandboxing rules inside of callbacks to
  // UnsafeTrap().
  *has_unsafe_traps = false;
  gen->Traverse(rest, CheckForUnsafeErrorCodes, has_unsafe_traps);
  if (!*has_unsafe_traps) {
    // If no unsafe traps, then simply return |rest|.
    return rest;
  }

  // If our BPF program has unsafe jumps, enable support for them. This
  // test happens very early in the BPF filter program. Even before we
  // consider looking at system call numbers.
  // As support for unsafe jumps essentially defeats all the security
  // measures that the sandbox provides, we print a big warning message --
  // and of course, we make sure to only ever enable this feature if it
  // is actually requested by the sandbox policy.
  if (Syscall::Call(-1) == -1 && errno == ENOSYS) {
    SANDBOX_DIE(
        "Support for UnsafeTrap() has not yet been ported to this "
        "architecture");
  }

  for (size_t i = 0; i < arraysize(kSyscallsRequiredForUnsafeTraps); ++i) {
    if (!policy_->EvaluateSyscall(this, kSyscallsRequiredForUnsafeTraps[i])
             .Equals(ErrorCode(ErrorCode::ERR_ALLOWED))) {
      SANDBOX_DIE(
          "Policies that use UnsafeTrap() must unconditionally allow all "
          "required system calls");
    }
  }

  if (!Trap::EnableUnsafeTrapsInSigSysHandler()) {
    // We should never be able to get here, as UnsafeTrap() should never
    // actually return a valid ErrorCode object unless the user set the
    // CHROME_SANDBOX_DEBUGGING environment variable; and therefore,
    // "has_unsafe_traps" would always be false. But better double-check
    // than enabling dangerous code.
    SANDBOX_DIE("We'd rather die than enable unsafe traps");
  }
  gen->Traverse(rest, RedirectToUserspace, this);

  // Allow system calls, if they originate from our magic return address
  // (which we can query by calling Syscall::Call(-1)).
  uint64_t syscall_entry_point =
      static_cast<uint64_t>(static_cast<uintptr_t>(Syscall::Call(-1)));
  uint32_t low = static_cast<uint32_t>(syscall_entry_point);
  uint32_t hi = static_cast<uint32_t>(syscall_entry_point >> 32);

  // BPF cannot do native 64-bit comparisons, so we have to compare
  // both 32-bit halves of the instruction pointer. If they match what
  // we expect, we return ERR_ALLOWED. If either or both don't match,
  // we continue evalutating the rest of the sandbox policy.
  //
  // For simplicity, we check the full 64-bit instruction pointer even
  // on 32-bit architectures.
  return gen->MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      SECCOMP_IP_LSB_IDX,
      gen->MakeInstruction(
          BPF_JMP + BPF_JEQ + BPF_K,
          low,
          gen->MakeInstruction(
              BPF_LD + BPF_W + BPF_ABS,
              SECCOMP_IP_MSB_IDX,
              gen->MakeInstruction(
                  BPF_JMP + BPF_JEQ + BPF_K,
                  hi,
                  RetExpression(gen, ErrorCode(ErrorCode::ERR_ALLOWED)),
                  rest)),
          rest));
}

Instruction* SandboxBPF::DispatchSyscall(CodeGen* gen) {
  // Evaluate all possible system calls and group their ErrorCodes into
  // ranges of identical codes.
  Ranges ranges;
  FindRanges(&ranges);

  // Compile the system call ranges to an optimized BPF jumptable
  Instruction* jumptable = AssembleJumpTable(gen, ranges.begin(), ranges.end());

  // Grab the system call number, so that we can check it and then
  // execute the jump table.
  return gen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS,
                              SECCOMP_NR_IDX,
                              CheckSyscallNumber(gen, jumptable));
}

Instruction* SandboxBPF::CheckSyscallNumber(CodeGen* gen, Instruction* passed) {
  if (kIsIntel) {
    // On Intel architectures, verify that system call numbers are in the
    // expected number range.
    Instruction* invalidX32 =
        RetExpression(gen, Kill("Illegal mixing of system call ABIs"));
    if (kIsX32) {
      // The newer x32 API always sets bit 30.
      return gen->MakeInstruction(
          BPF_JMP + BPF_JSET + BPF_K, 0x40000000, passed, invalidX32);
    } else {
      // The older i386 and x86-64 APIs clear bit 30 on all system calls.
      return gen->MakeInstruction(
          BPF_JMP + BPF_JSET + BPF_K, 0x40000000, invalidX32, passed);
    }
  }

  // TODO(mdempsky): Similar validation for other architectures?
  return passed;
}

void SandboxBPF::VerifyProgram(const Program& program, bool has_unsafe_traps) {
  // If we previously rewrote the BPF program so that it calls user-space
  // whenever we return an "errno" value from the filter, then we have to
  // wrap our system call evaluator to perform the same operation. Otherwise,
  // the verifier would also report a mismatch in return codes.
  scoped_ptr<const RedirectToUserSpacePolicyWrapper> redirected_policy(
      new RedirectToUserSpacePolicyWrapper(policy_.get()));

  const char* err = NULL;
  if (!Verifier::VerifyBPF(this,
                           program,
                           has_unsafe_traps ? *redirected_policy : *policy_,
                           &err)) {
    CodeGen::PrintProgram(program);
    SANDBOX_DIE(err);
  }
}

void SandboxBPF::FindRanges(Ranges* ranges) {
  // Please note that "struct seccomp_data" defines system calls as a signed
  // int32_t, but BPF instructions always operate on unsigned quantities. We
  // deal with this disparity by enumerating from MIN_SYSCALL to MAX_SYSCALL,
  // and then verifying that the rest of the number range (both positive and
  // negative) all return the same ErrorCode.
  const ErrorCode invalid_err = policy_->InvalidSyscall(this);
  uint32_t old_sysnum = 0;
  ErrorCode old_err = IsValidSyscallNumber(old_sysnum)
                          ? policy_->EvaluateSyscall(this, old_sysnum)
                          : invalid_err;

  for (SyscallIterator iter(false); !iter.Done();) {
    uint32_t sysnum = iter.Next();
    ErrorCode err =
        IsValidSyscallNumber(sysnum)
            ? policy_->EvaluateSyscall(this, static_cast<int>(sysnum))
            : invalid_err;
    if (!err.Equals(old_err) || iter.Done()) {
      ranges->push_back(Range(old_sysnum, sysnum - 1, old_err));
      old_sysnum = sysnum;
      old_err = err;
    }
  }
}

Instruction* SandboxBPF::AssembleJumpTable(CodeGen* gen,
                                           Ranges::const_iterator start,
                                           Ranges::const_iterator stop) {
  // We convert the list of system call ranges into jump table that performs
  // a binary search over the ranges.
  // As a sanity check, we need to have at least one distinct ranges for us
  // to be able to build a jump table.
  if (stop - start <= 0) {
    SANDBOX_DIE("Invalid set of system call ranges");
  } else if (stop - start == 1) {
    // If we have narrowed things down to a single range object, we can
    // return from the BPF filter program.
    return RetExpression(gen, start->err);
  }

  // Pick the range object that is located at the mid point of our list.
  // We compare our system call number against the lowest valid system call
  // number in this range object. If our number is lower, it is outside of
  // this range object. If it is greater or equal, it might be inside.
  Ranges::const_iterator mid = start + (stop - start) / 2;

  // Sub-divide the list of ranges and continue recursively.
  Instruction* jf = AssembleJumpTable(gen, start, mid);
  Instruction* jt = AssembleJumpTable(gen, mid, stop);
  return gen->MakeInstruction(BPF_JMP + BPF_JGE + BPF_K, mid->from, jt, jf);
}

Instruction* SandboxBPF::RetExpression(CodeGen* gen, const ErrorCode& err) {
  switch (err.error_type()) {
    case ErrorCode::ET_COND:
      return CondExpression(gen, err);
    case ErrorCode::ET_SIMPLE:
    case ErrorCode::ET_TRAP:
      return gen->MakeInstruction(BPF_RET + BPF_K, err.err());
    default:
      SANDBOX_DIE("ErrorCode is not suitable for returning from a BPF program");
  }
}

Instruction* SandboxBPF::CondExpression(CodeGen* gen, const ErrorCode& cond) {
  // Sanity check that |cond| makes sense.
  if (cond.argno_ < 0 || cond.argno_ >= 6) {
    SANDBOX_DIE("sandbox_bpf: invalid argument number");
  }
  if (cond.width_ != ErrorCode::TP_32BIT &&
      cond.width_ != ErrorCode::TP_64BIT) {
    SANDBOX_DIE("sandbox_bpf: invalid argument width");
  }
  if (cond.mask_ == 0) {
    SANDBOX_DIE("sandbox_bpf: zero mask is invalid");
  }
  if ((cond.value_ & cond.mask_) != cond.value_) {
    SANDBOX_DIE("sandbox_bpf: value contains masked out bits");
  }
  if (cond.width_ == ErrorCode::TP_32BIT &&
      ((cond.mask_ >> 32) != 0 || (cond.value_ >> 32) != 0)) {
    SANDBOX_DIE("sandbox_bpf: test exceeds argument size");
  }
  // TODO(mdempsky): Reject TP_64BIT on 32-bit platforms. For now we allow it
  // because some SandboxBPF unit tests exercise it.

  Instruction* passed = RetExpression(gen, *cond.passed_);
  Instruction* failed = RetExpression(gen, *cond.failed_);

  // We want to emit code to check "(arg & mask) == value" where arg, mask, and
  // value are 64-bit values, but the BPF machine is only 32-bit. We implement
  // this by independently testing the upper and lower 32-bits and continuing to
  // |passed| if both evaluate true, or to |failed| if either evaluate false.
  return CondExpressionHalf(
      gen,
      cond,
      UpperHalf,
      CondExpressionHalf(gen, cond, LowerHalf, passed, failed),
      failed);
}

Instruction* SandboxBPF::CondExpressionHalf(CodeGen* gen,
                                            const ErrorCode& cond,
                                            ArgHalf half,
                                            Instruction* passed,
                                            Instruction* failed) {
  if (cond.width_ == ErrorCode::TP_32BIT && half == UpperHalf) {
    // Special logic for sanity checking the upper 32-bits of 32-bit system
    // call arguments.

    // TODO(mdempsky): Compile Unexpected64bitArgument() just per program.
    Instruction* invalid_64bit = RetExpression(gen, Unexpected64bitArgument());

    const uint32_t upper = SECCOMP_ARG_MSB_IDX(cond.argno_);
    const uint32_t lower = SECCOMP_ARG_LSB_IDX(cond.argno_);

    if (sizeof(void*) == 4) {
      // On 32-bit platforms, the upper 32-bits should always be 0:
      //   LDW  [upper]
      //   JEQ  0, passed, invalid
      return gen->MakeInstruction(
          BPF_LD + BPF_W + BPF_ABS,
          upper,
          gen->MakeInstruction(
              BPF_JMP + BPF_JEQ + BPF_K, 0, passed, invalid_64bit));
    }

    // On 64-bit platforms, the upper 32-bits may be 0 or ~0; but we only allow
    // ~0 if the sign bit of the lower 32-bits is set too:
    //   LDW  [upper]
    //   JEQ  0, passed, (next)
    //   JEQ  ~0, (next), invalid
    //   LDW  [lower]
    //   JSET (1<<31), passed, invalid
    //
    // TODO(mdempsky): The JSET instruction could perhaps jump to passed->next
    // instead, as the first instruction of passed should be "LDW [lower]".
    return gen->MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        upper,
        gen->MakeInstruction(
            BPF_JMP + BPF_JEQ + BPF_K,
            0,
            passed,
            gen->MakeInstruction(
                BPF_JMP + BPF_JEQ + BPF_K,
                std::numeric_limits<uint32_t>::max(),
                gen->MakeInstruction(
                    BPF_LD + BPF_W + BPF_ABS,
                    lower,
                    gen->MakeInstruction(BPF_JMP + BPF_JSET + BPF_K,
                                         1U << 31,
                                         passed,
                                         invalid_64bit)),
                invalid_64bit)));
  }

  const uint32_t idx = (half == UpperHalf) ? SECCOMP_ARG_MSB_IDX(cond.argno_)
                                           : SECCOMP_ARG_LSB_IDX(cond.argno_);
  const uint32_t mask = (half == UpperHalf) ? cond.mask_ >> 32 : cond.mask_;
  const uint32_t value = (half == UpperHalf) ? cond.value_ >> 32 : cond.value_;

  // Emit a suitable instruction sequence for (arg & mask) == value.

  // For (arg & 0) == 0, just return passed.
  if (mask == 0) {
    CHECK_EQ(0U, value);
    return passed;
  }

  // For (arg & ~0) == value, emit:
  //   LDW  [idx]
  //   JEQ  value, passed, failed
  if (mask == std::numeric_limits<uint32_t>::max()) {
    return gen->MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, value, passed, failed));
  }

  // For (arg & mask) == 0, emit:
  //   LDW  [idx]
  //   JSET mask, failed, passed
  // (Note: failed and passed are intentionally swapped.)
  if (value == 0) {
    return gen->MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen->MakeInstruction(BPF_JMP + BPF_JSET + BPF_K, mask, failed, passed));
  }

  // For (arg & x) == x where x is a single-bit value, emit:
  //   LDW  [idx]
  //   JSET mask, passed, failed
  if (mask == value && HasExactlyOneBit(mask)) {
    return gen->MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen->MakeInstruction(BPF_JMP + BPF_JSET + BPF_K, mask, passed, failed));
  }

  // Generic fallback:
  //   LDW  [idx]
  //   AND  mask
  //   JEQ  value, passed, failed
  return gen->MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      idx,
      gen->MakeInstruction(
          BPF_ALU + BPF_AND + BPF_K,
          mask,
          gen->MakeInstruction(
              BPF_JMP + BPF_JEQ + BPF_K, value, passed, failed)));
}

ErrorCode SandboxBPF::Unexpected64bitArgument() {
  return Kill("Unexpected 64bit argument detected");
}

ErrorCode SandboxBPF::Trap(Trap::TrapFnc fnc, const void* aux) {
  return ErrorCode(fnc, aux, true /* Safe Trap */);
}

ErrorCode SandboxBPF::UnsafeTrap(Trap::TrapFnc fnc, const void* aux) {
  return ErrorCode(fnc, aux, false /* Unsafe Trap */);
}

bool SandboxBPF::IsRequiredForUnsafeTrap(int sysno) {
  for (size_t i = 0; i < arraysize(kSyscallsRequiredForUnsafeTraps); ++i) {
    if (sysno == kSyscallsRequiredForUnsafeTraps[i]) {
      return true;
    }
  }
  return false;
}

intptr_t SandboxBPF::ForwardSyscall(const struct arch_seccomp_data& args) {
  return Syscall::Call(args.nr,
                       static_cast<intptr_t>(args.args[0]),
                       static_cast<intptr_t>(args.args[1]),
                       static_cast<intptr_t>(args.args[2]),
                       static_cast<intptr_t>(args.args[3]),
                       static_cast<intptr_t>(args.args[4]),
                       static_cast<intptr_t>(args.args[5]));
}

ErrorCode SandboxBPF::CondMaskedEqual(int argno,
                                      ErrorCode::ArgType width,
                                      uint64_t mask,
                                      uint64_t value,
                                      const ErrorCode& passed,
                                      const ErrorCode& failed) {
  return ErrorCode(argno,
                   width,
                   mask,
                   value,
                   &*conds_->insert(passed).first,
                   &*conds_->insert(failed).first);
}

ErrorCode SandboxBPF::Cond(int argno,
                           ErrorCode::ArgType width,
                           ErrorCode::Operation op,
                           uint64_t value,
                           const ErrorCode& passed,
                           const ErrorCode& failed) {
  // CondExpression() currently rejects mask==0 as invalid, but there are
  // SandboxBPF unit tests that (questionably) expect OP_HAS_{ANY,ALL}_BITS to
  // work with value==0. To keep those tests working for now, we specially
  // convert value==0 here.

  switch (op) {
    case ErrorCode::OP_EQUAL: {
      // Convert to "(arg & ~0) == value".
      const uint64_t mask = (width == ErrorCode::TP_64BIT)
                                ? std::numeric_limits<uint64_t>::max()
                                : std::numeric_limits<uint32_t>::max();
      return CondMaskedEqual(argno, width, mask, value, passed, failed);
    }

    case ErrorCode::OP_HAS_ALL_BITS:
      if (value == 0) {
        // Always passes.
        return passed;
      }
      // Convert to "(arg & value) == value".
      return CondMaskedEqual(argno, width, value, value, passed, failed);

    case ErrorCode::OP_HAS_ANY_BITS:
      if (value == 0) {
        // Always fails.
        return failed;
      }
      // Convert to "(arg & value) == 0", but swap passed and failed.
      return CondMaskedEqual(argno, width, value, 0, failed, passed);

    default:
      SANDBOX_DIE("Not implemented");
  }
}

ErrorCode SandboxBPF::Kill(const char* msg) {
  return Trap(BPFFailure, const_cast<char*>(msg));
}

SandboxBPF::SandboxStatus SandboxBPF::status_ = STATUS_UNKNOWN;

}  // namespace sandbox
