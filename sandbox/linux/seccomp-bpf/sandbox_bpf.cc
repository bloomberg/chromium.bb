// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#ifndef SECCOMP_BPF_STANDALONE
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#endif

#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"

// Android's signal.h doesn't define ucontext etc.
#if defined(OS_ANDROID) && defined(__arm__)
#include "sandbox/linux/services/android_arm_ucontext.h"
#endif

namespace {

void WriteFailedStderrSetupMessage(int out_fd) {
  const char* error_string = strerror(errno);
  static const char msg[] = "You have reproduced a puzzling issue.\n"
                            "Please, report to crbug.com/152530!\n"
                            "Failed to set up stderr: ";
  if (HANDLE_EINTR(write(out_fd, msg, sizeof(msg)-1)) > 0 && error_string &&
      HANDLE_EINTR(write(out_fd, error_string, strlen(error_string))) > 0 &&
      HANDLE_EINTR(write(out_fd, "\n", 1))) {
  }
}

// We need to tell whether we are performing a "normal" callback, or
// whether we were called recursively from within a UnsafeTrap() callback.
// This is a little tricky to do, because we need to somehow get access to
// per-thread data from within a signal context. Normal TLS storage is not
// safely accessible at this time. We could roll our own, but that involves
// a lot of complexity. Instead, we co-opt one bit in the signal mask.
// If BUS is blocked, we assume that we have been called recursively.
// There is a possibility for collision with other code that needs to do
// this, but in practice the risks are low.
// If SIGBUS turns out to be a problem, we could instead co-opt one of the
// realtime signals. There are plenty of them. Unfortunately, there is no
// way to mark a signal as allocated. So, the potential for collision is
// possibly even worse.
bool GetIsInSigHandler(const ucontext_t *ctx) {
  // Note: on Android, sigismember does not take a pointer to const.
  return sigismember(const_cast<sigset_t*>(&ctx->uc_sigmask), SIGBUS);
}

void SetIsInSigHandler() {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGBUS);
  sigprocmask(SIG_BLOCK, &mask, NULL);
}

}  // namespace

// The kernel gives us a sandbox, we turn it into a playground :-)
// This is version 2 of the playground; version 1 was built on top of
// pre-BPF seccomp mode.
namespace playground2 {

const int kExpectedExitCode = 100;

// We define a really simple sandbox policy. It is just good enough for us
// to tell that the sandbox has actually been activated.
ErrorCode Sandbox::ProbeEvaluator(int sysnum, void *) {
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

void Sandbox::ProbeProcess(void) {
  if (syscall(__NR_getpid) < 0 && errno == EPERM) {
    syscall(__NR_exit_group, static_cast<intptr_t>(kExpectedExitCode));
  }
}

bool Sandbox::IsValidSyscallNumber(int sysnum) {
  return SyscallIterator::IsValid(sysnum);
}

ErrorCode Sandbox::AllowAllEvaluator(int sysnum, void *) {
  if (!IsValidSyscallNumber(sysnum)) {
    return ErrorCode(ENOSYS);
  }
  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

void Sandbox::TryVsyscallProcess(void) {
  time_t current_time;
  // time() is implemented as a vsyscall. With an older glibc, with
  // vsyscall=emulate and some versions of the seccomp BPF patch
  // we may get SIGKILL-ed. Detect this!
  if (time(&current_time) != static_cast<time_t>(-1)) {
    syscall(__NR_exit_group, static_cast<intptr_t>(kExpectedExitCode));
  }
}

bool Sandbox::RunFunctionInPolicy(void (*code_in_sandbox)(),
                                  EvaluateSyscall syscall_evaluator,
                                  void *aux,
                                  int proc_fd) {
  // Block all signals before forking a child process. This prevents an
  // attacker from manipulating our test by sending us an unexpected signal.
  sigset_t old_mask, new_mask;
  if (sigfillset(&new_mask) ||
      sigprocmask(SIG_BLOCK, &new_mask, &old_mask)) {
    SANDBOX_DIE("sigprocmask() failed");
  }
  int fds[2];
  if (pipe2(fds, O_NONBLOCK|O_CLOEXEC)) {
    SANDBOX_DIE("pipe() failed");
  }

  if (fds[0] <= 2 || fds[1] <= 2) {
    SANDBOX_DIE("Process started without standard file descriptors");
  }

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
    if (HANDLE_EINTR(close(fds[0]))) {
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
    if (HANDLE_EINTR(close(fds[1]))) {
      // This call to close() has been failing in strange ways. See
      // crbug.com/152530. So we only fail in debug mode now.
#if !defined(NDEBUG)
      WriteFailedStderrSetupMessage(fds[1]);
      SANDBOX_DIE(NULL);
#endif
    }

    evaluators_.clear();
    SetSandboxPolicy(syscall_evaluator, aux);
    set_proc_fd(proc_fd);

    // By passing "quiet=true" to "startSandboxInternal()" we suppress
    // messages for expected and benign failures (e.g. if the current
    // kernel lacks support for BPF filters).
    StartSandboxInternal(true);

    // Run our code in the sandbox.
    code_in_sandbox();

    // code_in_sandbox() is not supposed to return here.
    SANDBOX_DIE(NULL);
  }

  // In the parent process.
  if (HANDLE_EINTR(close(fds[1]))) {
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
      while (len > 1 && buf[len-1] == '\n') {
        --len;
      }
      buf[len] = '\000';
      SANDBOX_DIE(buf);
    }
  }
  if (HANDLE_EINTR(close(fds[0]))) {
    SANDBOX_DIE("close() failed");
  }

  return rc;
}

bool Sandbox::KernelSupportSeccompBPF(int proc_fd) {
  return
    RunFunctionInPolicy(ProbeProcess, Sandbox::ProbeEvaluator, 0, proc_fd) &&
    RunFunctionInPolicy(TryVsyscallProcess, Sandbox::AllowAllEvaluator, 0,
                        proc_fd);
}

Sandbox::SandboxStatus Sandbox::SupportsSeccompSandbox(int proc_fd) {
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
    status_ = KernelSupportSeccompBPF(proc_fd)
      ? STATUS_AVAILABLE : STATUS_UNSUPPORTED;

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

void Sandbox::set_proc_fd(int proc_fd) {
  proc_fd_ = proc_fd;
}

void Sandbox::StartSandboxInternal(bool quiet) {
  if (status_ == STATUS_UNSUPPORTED || status_ == STATUS_UNAVAILABLE) {
    SANDBOX_DIE("Trying to start sandbox, even though it is known to be "
                "unavailable");
  } else if (status_ == STATUS_ENABLED) {
    SANDBOX_DIE("Cannot start sandbox recursively. Use multiple calls to "
                "setSandboxPolicy() to stack policies instead");
  }
  if (proc_fd_ < 0) {
    proc_fd_ = open("/proc", O_RDONLY|O_DIRECTORY);
  }
  if (proc_fd_ < 0) {
    // For now, continue in degraded mode, if we can't access /proc.
    // In the future, we might want to tighten this requirement.
  }
  if (!IsSingleThreaded(proc_fd_)) {
    SANDBOX_DIE("Cannot start sandbox, if process is already multi-threaded");
  }

  // We no longer need access to any files in /proc. We want to do this
  // before installing the filters, just in case that our policy denies
  // close().
  if (proc_fd_ >= 0) {
    if (HANDLE_EINTR(close(proc_fd_))) {
      SANDBOX_DIE("Failed to close file descriptor for /proc");
    }
    proc_fd_ = -1;
  }

  // Install the filters.
  InstallFilter(quiet);

  // We are now inside the sandbox.
  status_ = STATUS_ENABLED;
}

bool Sandbox::IsSingleThreaded(int proc_fd) {
  if (proc_fd < 0) {
    // Cannot determine whether program is single-threaded. Hope for
    // the best...
    return true;
  }

  struct stat sb;
  int task = -1;
  if ((task = openat(proc_fd, "self/task", O_RDONLY|O_DIRECTORY)) < 0 ||
      fstat(task, &sb) != 0 ||
      sb.st_nlink != 3 ||
      HANDLE_EINTR(close(task))) {
    if (task >= 0) {
      if (HANDLE_EINTR(close(task))) { }
    }
    return false;
  }
  return true;
}

bool Sandbox::IsDenied(const ErrorCode& code) {
  return (code.err() & SECCOMP_RET_ACTION) == SECCOMP_RET_TRAP ||
         (code.err() >= (SECCOMP_RET_ERRNO + ErrorCode::ERR_MIN_ERRNO) &&
          code.err() <= (SECCOMP_RET_ERRNO + ErrorCode::ERR_MAX_ERRNO));
}

void Sandbox::PolicySanityChecks(EvaluateSyscall syscall_evaluator,
                                 void *aux) {
  for (SyscallIterator iter(true); !iter.Done(); ) {
    uint32_t sysnum = iter.Next();
    if (!IsDenied(syscall_evaluator(sysnum, aux))) {
      SANDBOX_DIE("Policies should deny system calls that are outside the "
                  "expected range (typically MIN_SYSCALL..MAX_SYSCALL)");
    }
  }
  return;
}

void Sandbox::CheckForUnsafeErrorCodes(Instruction *insn, void *aux) {
  if (BPF_CLASS(insn->code) == BPF_RET &&
      insn->k >  SECCOMP_RET_TRAP &&
      insn->k - SECCOMP_RET_TRAP <= trap_array_size_) {
    const ErrorCode& err = trap_array_[insn->k - SECCOMP_RET_TRAP - 1];
    if (!err.safe_) {
      bool *is_unsafe = static_cast<bool *>(aux);
      *is_unsafe = true;
    }
  }
}

void Sandbox::RedirectToUserspace(Instruction *insn, void *) {
  // When inside an UnsafeTrap() callback, we want to allow all system calls.
  // This means, we must conditionally disable the sandbox -- and that's not
  // something that kernel-side BPF filters can do, as they cannot inspect
  // any state other than the syscall arguments.
  // But if we redirect all error handlers to user-space, then we can easily
  // make this decision.
  // The performance penalty for this extra round-trip to user-space is not
  // actually that bad, as we only ever pay it for denied system calls; and a
  // typical program has very few of these.
  if (BPF_CLASS(insn->code) == BPF_RET &&
      (insn->k & SECCOMP_RET_ACTION) == SECCOMP_RET_ERRNO) {
    insn->k = Trap(ReturnErrno,
                   reinterpret_cast<void *>(insn->k & SECCOMP_RET_DATA)).err();
  }
}

ErrorCode Sandbox::RedirectToUserspaceEvalWrapper(int sysnum, void *aux) {
  // We need to replicate the behavior of RedirectToUserspace(), so that our
  // Verifier can still work correctly.
  Evaluators *evaluators = reinterpret_cast<Evaluators *>(aux);
  const std::pair<EvaluateSyscall, void *>& evaluator = *evaluators->begin();
  ErrorCode err = evaluator.first(sysnum, evaluator.second);
  if ((err.err() & SECCOMP_RET_ACTION) == SECCOMP_RET_ERRNO) {
    return Trap(ReturnErrno,
                reinterpret_cast<void *>(err.err() & SECCOMP_RET_DATA));
  }
  return err;
}

void Sandbox::SetSandboxPolicy(EvaluateSyscall syscall_evaluator, void *aux) {
  if (status_ == STATUS_ENABLED) {
    SANDBOX_DIE("Cannot change policy after sandbox has started");
  }
  PolicySanityChecks(syscall_evaluator, aux);
  evaluators_.push_back(std::make_pair(syscall_evaluator, aux));
}

void Sandbox::InstallFilter(bool quiet) {
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
  Program *program = AssembleFilter();

  // Make sure compilation resulted in BPF program that executes
  // correctly. Otherwise, there is an internal error in our BPF compiler.
  // There is really nothing the caller can do until the bug is fixed.
#ifndef NDEBUG
  VerifyProgram(*program);
#endif

  struct sock_filter bpf[program->size()];
  const struct sock_fprog prog = {
    static_cast<unsigned short>(program->size()), bpf };
  memcpy(bpf, &(*program)[0], sizeof(bpf));
  delete program;

  // Release memory that is no longer needed
  evaluators_.clear();
  conds_.clear();

  // Install BPF filter program
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    SANDBOX_DIE(quiet ? NULL : "Kernel refuses to enable no-new-privs");
  } else {
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
      SANDBOX_DIE(quiet ? NULL : "Kernel refuses to turn on BPF filters");
    }
  }

  return;
}

Sandbox::Program *Sandbox::AssembleFilter() {
  // Verify that the user pushed a policy.
  if (evaluators_.empty()) {
  filter_failed:
    SANDBOX_DIE("Failed to configure system call filters");
  }

  // Set new SIGSYS handler
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = SigSys;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  if (sigaction(SIGSYS, &sa, NULL) < 0) {
    goto filter_failed;
  }

  // Unmask SIGSYS
  sigset_t mask;
  if (sigemptyset(&mask) ||
      sigaddset(&mask, SIGSYS) ||
      sigprocmask(SIG_UNBLOCK, &mask, NULL)) {
    goto filter_failed;
  }

  // We can't handle stacked evaluators, yet. We'll get there eventually
  // though. Hang tight.
  if (evaluators_.size() != 1) {
    SANDBOX_DIE("Not implemented");
  }

  // Assemble the BPF filter program.
  CodeGen *gen = new CodeGen();
  if (!gen) {
    SANDBOX_DIE("Out of memory");
  }

  // If the architecture doesn't match SECCOMP_ARCH, disallow the
  // system call.
  Instruction *tail;
  Instruction *head =
    gen->MakeInstruction(BPF_LD+BPF_W+BPF_ABS, SECCOMP_ARCH_IDX,
  tail =
    gen->MakeInstruction(BPF_JMP+BPF_JEQ+BPF_K, SECCOMP_ARCH,
                         NULL,
    gen->MakeInstruction(BPF_RET+BPF_K,
                         Kill("Invalid audit architecture in BPF filter"))));

  {
    // Evaluate all possible system calls and group their ErrorCodes into
    // ranges of identical codes.
    Ranges ranges;
    FindRanges(&ranges);

    // Compile the system call ranges to an optimized BPF jumptable
    Instruction *jumptable =
      AssembleJumpTable(gen, ranges.begin(), ranges.end());

    // If there is at least one UnsafeTrap() in our program, the entire sandbox
    // is unsafe. We need to modify the program so that all non-
    // SECCOMP_RET_ALLOW ErrorCodes are handled in user-space. This will then
    // allow us to temporarily disable sandboxing rules inside of callbacks to
    // UnsafeTrap().
    has_unsafe_traps_ = false;
    gen->Traverse(jumptable, CheckForUnsafeErrorCodes, &has_unsafe_traps_);

    // Grab the system call number, so that we can implement jump tables.
    Instruction *load_nr =
      gen->MakeInstruction(BPF_LD+BPF_W+BPF_ABS, SECCOMP_NR_IDX);

    // If our BPF program has unsafe jumps, enable support for them. This
    // test happens very early in the BPF filter program. Even before we
    // consider looking at system call numbers.
    // As support for unsafe jumps essentially defeats all the security
    // measures that the sandbox provides, we print a big warning message --
    // and of course, we make sure to only ever enable this feature if it
    // is actually requested by the sandbox policy.
    if (has_unsafe_traps_) {
      if (SandboxSyscall(-1) == -1 && errno == ENOSYS) {
        SANDBOX_DIE("Support for UnsafeTrap() has not yet been ported to this "
                    "architecture");
      }

      EvaluateSyscall evaluateSyscall = evaluators_.begin()->first;
      void *aux                       = evaluators_.begin()->second;
      if (!evaluateSyscall(__NR_rt_sigprocmask, aux).
            Equals(ErrorCode(ErrorCode::ERR_ALLOWED)) ||
          !evaluateSyscall(__NR_rt_sigreturn, aux).
            Equals(ErrorCode(ErrorCode::ERR_ALLOWED))
#if defined(__NR_sigprocmask)
       || !evaluateSyscall(__NR_sigprocmask, aux).
            Equals(ErrorCode(ErrorCode::ERR_ALLOWED))
#endif
#if defined(__NR_sigreturn)
       || !evaluateSyscall(__NR_sigreturn, aux).
            Equals(ErrorCode(ErrorCode::ERR_ALLOWED))
#endif
          ) {
        SANDBOX_DIE("Invalid seccomp policy; if using UnsafeTrap(), you must "
                    "unconditionally allow sigreturn() and sigprocmask()");
      }

      SANDBOX_INFO("WARNING! Disabling sandbox for debugging purposes");
      gen->Traverse(jumptable, RedirectToUserspace, NULL);

      // Allow system calls, if they originate from our magic return address
      // (which we can query by calling SandboxSyscall(-1)).
      uintptr_t syscall_entry_point =
        static_cast<uintptr_t>(SandboxSyscall(-1));
      uint32_t low = static_cast<uint32_t>(syscall_entry_point);
#if __SIZEOF_POINTER__ > 4
      uint32_t hi  = static_cast<uint32_t>(syscall_entry_point >> 32);
#endif

      // BPF cannot do native 64bit comparisons. On 64bit architectures, we
      // have to compare both 32bit halves of the instruction pointer. If they
      // match what we expect, we return ERR_ALLOWED. If either or both don't
      // match, we continue evalutating the rest of the sandbox policy.
      Instruction *escape_hatch =
        gen->MakeInstruction(BPF_LD+BPF_W+BPF_ABS, SECCOMP_IP_LSB_IDX,
        gen->MakeInstruction(BPF_JMP+BPF_JEQ+BPF_K, low,
#if __SIZEOF_POINTER__ > 4
        gen->MakeInstruction(BPF_LD+BPF_W+BPF_ABS, SECCOMP_IP_MSB_IDX,
        gen->MakeInstruction(BPF_JMP+BPF_JEQ+BPF_K, hi,
#endif
        gen->MakeInstruction(BPF_RET+BPF_K, ErrorCode(ErrorCode::ERR_ALLOWED)),
#if __SIZEOF_POINTER__ > 4
                             load_nr)),
#endif
                             load_nr));
      gen->JoinInstructions(tail, escape_hatch);
    } else {
      gen->JoinInstructions(tail, load_nr);
    }
    tail = load_nr;

    // On Intel architectures, verify that system call numbers are in the
    // expected number range. The older i386 and x86-64 APIs clear bit 30
    // on all system calls. The newer x32 API always sets bit 30.
#if defined(__i386__) || defined(__x86_64__)
    Instruction *invalidX32 =
      gen->MakeInstruction(BPF_RET+BPF_K,
                           Kill("Illegal mixing of system call ABIs").err_);
    Instruction *checkX32 =
#if defined(__x86_64__) && defined(__ILP32__)
      gen->MakeInstruction(BPF_JMP+BPF_JSET+BPF_K, 0x40000000, 0, invalidX32);
#else
      gen->MakeInstruction(BPF_JMP+BPF_JSET+BPF_K, 0x40000000, invalidX32, 0);
#endif
      gen->JoinInstructions(tail, checkX32);
      tail = checkX32;
#endif

    // Append jump table to our pre-amble
    gen->JoinInstructions(tail, jumptable);
  }

  // Turn the DAG into a vector of instructions.
  Program *program = new Program();
  gen->Compile(head, program);
  delete gen;

  return program;
}

void Sandbox::VerifyProgram(const Program& program) {
  // If we previously rewrote the BPF program so that it calls user-space
  // whenever we return an "errno" value from the filter, then we have to
  // wrap our system call evaluator to perform the same operation. Otherwise,
  // the verifier would also report a mismatch in return codes.
  Evaluators redirected_evaluators;
  redirected_evaluators.push_back(
      std::make_pair(RedirectToUserspaceEvalWrapper, &evaluators_));

  const char *err = NULL;
  if (!Verifier::VerifyBPF(
                       program,
                       has_unsafe_traps_ ? redirected_evaluators : evaluators_,
                       &err)) {
    SANDBOX_DIE(err);
  }
}

void Sandbox::FindRanges(Ranges *ranges) {
  // Please note that "struct seccomp_data" defines system calls as a signed
  // int32_t, but BPF instructions always operate on unsigned quantities. We
  // deal with this disparity by enumerating from MIN_SYSCALL to MAX_SYSCALL,
  // and then verifying that the rest of the number range (both positive and
  // negative) all return the same ErrorCode.
  EvaluateSyscall evaluate_syscall = evaluators_.begin()->first;
  void *aux                        = evaluators_.begin()->second;
  uint32_t old_sysnum              = 0;
  ErrorCode old_err                = evaluate_syscall(old_sysnum, aux);
  ErrorCode invalid_err            = evaluate_syscall(MIN_SYSCALL - 1, aux);
  for (SyscallIterator iter(false); !iter.Done(); ) {
    uint32_t sysnum = iter.Next();
    ErrorCode err = evaluate_syscall(static_cast<int>(sysnum), aux);
    if (!iter.IsValid(sysnum) && !invalid_err.Equals(err)) {
      // A proper sandbox policy should always treat system calls outside of
      // the range MIN_SYSCALL..MAX_SYSCALL (i.e. anything that returns
      // "false" for SyscallIterator::IsValid()) identically. Typically, all
      // of these system calls would be denied with the same ErrorCode.
      SANDBOX_DIE("Invalid seccomp policy");
    }
    if (!err.Equals(old_err) || iter.Done()) {
      ranges->push_back(Range(old_sysnum, sysnum - 1, old_err));
      old_sysnum = sysnum;
      old_err    = err;
    }
  }
}

Instruction *Sandbox::AssembleJumpTable(CodeGen *gen,
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
  Ranges::const_iterator mid = start + (stop - start)/2;

  // Sub-divide the list of ranges and continue recursively.
  Instruction *jf = AssembleJumpTable(gen, start, mid);
  Instruction *jt = AssembleJumpTable(gen, mid, stop);
  return gen->MakeInstruction(BPF_JMP+BPF_JGE+BPF_K, mid->from, jt, jf);
}

Instruction *Sandbox::RetExpression(CodeGen *gen, const ErrorCode& err) {
  if (err.error_type_ == ErrorCode::ET_COND) {
    return CondExpression(gen, err);
  } else {
    return gen->MakeInstruction(BPF_RET+BPF_K, err);
  }
}

Instruction *Sandbox::CondExpression(CodeGen *gen, const ErrorCode& cond) {
  // We can only inspect the six system call arguments that are passed in
  // CPU registers.
  if (cond.argno_ < 0 || cond.argno_ >= 6) {
    SANDBOX_DIE("Internal compiler error; invalid argument number "
                "encountered");
  }

  // BPF programs operate on 32bit entities. Load both halfs of the 64bit
  // system call argument and then generate suitable conditional statements.
  Instruction *msb_head =
    gen->MakeInstruction(BPF_LD+BPF_W+BPF_ABS,
                         SECCOMP_ARG_MSB_IDX(cond.argno_));
  Instruction *msb_tail = msb_head;
  Instruction *lsb_head =
    gen->MakeInstruction(BPF_LD+BPF_W+BPF_ABS,
                         SECCOMP_ARG_LSB_IDX(cond.argno_));
  Instruction *lsb_tail = lsb_head;

  // Emit a suitable comparison statement.
  switch (cond.op_) {
  case ErrorCode::OP_EQUAL:
    // Compare the least significant bits for equality
    lsb_tail = gen->MakeInstruction(BPF_JMP+BPF_JEQ+BPF_K,
                                    static_cast<uint32_t>(cond.value_),
                                    RetExpression(gen, *cond.passed_),
                                    RetExpression(gen, *cond.failed_));
    gen->JoinInstructions(lsb_head, lsb_tail);

    // If we are looking at a 64bit argument, we need to also compare the
    // most significant bits.
    if (cond.width_ == ErrorCode::TP_64BIT) {
      msb_tail = gen->MakeInstruction(BPF_JMP+BPF_JEQ+BPF_K,
                                      static_cast<uint32_t>(cond.value_ >> 32),
                                      NULL,
                                      RetExpression(gen, *cond.failed_));
      gen->JoinInstructions(msb_head, msb_tail);
    }
    break;
  default:
    // TODO(markus): We can only check for equality so far.
    SANDBOX_DIE("Not implemented");
    break;
  }

  // Ensure that we never pass a 64bit value, when we only expect a 32bit
  // value. This is somewhat complicated by the fact that on 64bit systems,
  // callers could legitimately pass in a non-zero value in the MSB, iff the
  // LSB has been sign-extended into the MSB.
  if (cond.width_ == ErrorCode::TP_32BIT) {
    if (cond.value_ >> 32) {
      SANDBOX_DIE("Invalid comparison of a 32bit system call argument "
                  "against a 64bit constant; this test is always false.");
    }

    Instruction *invalid_64bit = RetExpression(gen, Unexpected64bitArgument());
    #if __SIZEOF_POINTER__ > 4
    invalid_64bit =
      gen->MakeInstruction(BPF_JMP+BPF_JEQ+BPF_K, 0xFFFFFFFF,
      gen->MakeInstruction(BPF_LD+BPF_W+BPF_ABS,
                           SECCOMP_ARG_LSB_IDX(cond.argno_),
      gen->MakeInstruction(BPF_JMP+BPF_JGE+BPF_K, 0x80000000,
                           lsb_head,
                           invalid_64bit)),
                           invalid_64bit);
    #endif
    gen->JoinInstructions(
      msb_tail,
      gen->MakeInstruction(BPF_JMP+BPF_JEQ+BPF_K, 0,
                           lsb_head,
                           invalid_64bit));
  } else {
    gen->JoinInstructions(msb_tail, lsb_head);
  }

  return msb_head;
}

ErrorCode Sandbox::Unexpected64bitArgument() {
  return Kill("Unexpected 64bit argument detected");
}

void Sandbox::SigSys(int nr, siginfo_t *info, void *void_context) {
  // Various sanity checks to make sure we actually received a signal
  // triggered by a BPF filter. If something else triggered SIGSYS
  // (e.g. kill()), there is really nothing we can do with this signal.
  if (nr != SIGSYS || info->si_code != SYS_SECCOMP || !void_context ||
      info->si_errno <= 0 ||
      static_cast<size_t>(info->si_errno) > trap_array_size_) {
    // SANDBOX_DIE() can call LOG(FATAL). This is not normally async-signal
    // safe and can lead to bugs. We should eventually implement a different
    // logging and reporting mechanism that is safe to be called from
    // the sigSys() handler.
    // TODO: If we feel confident that our code otherwise works correctly, we
    //       could actually make an argument that spurious SIGSYS should
    //       just get silently ignored. TBD
  sigsys_err:
    SANDBOX_DIE("Unexpected SIGSYS received");
  }

  // Signal handlers should always preserve "errno". Otherwise, we could
  // trigger really subtle bugs.
  int old_errno   = errno;

  // Obtain the signal context. This, most notably, gives us access to
  // all CPU registers at the time of the signal.
  ucontext_t *ctx = reinterpret_cast<ucontext_t *>(void_context);

  // Obtain the siginfo information that is specific to SIGSYS. Unfortunately,
  // most versions of glibc don't include this information in siginfo_t. So,
  // we need to explicitly copy it into a arch_sigsys structure.
  struct arch_sigsys sigsys;
  memcpy(&sigsys, &info->_sifields, sizeof(sigsys));

  // Some more sanity checks.
  if (sigsys.ip != reinterpret_cast<void *>(SECCOMP_IP(ctx)) ||
      sigsys.nr != static_cast<int>(SECCOMP_SYSCALL(ctx)) ||
      sigsys.arch != SECCOMP_ARCH) {
    goto sigsys_err;
  }

  intptr_t rc;
  if (has_unsafe_traps_ && GetIsInSigHandler(ctx)) {
    errno = old_errno;
    if (sigsys.nr == __NR_clone) {
      SANDBOX_DIE("Cannot call clone() from an UnsafeTrap() handler");
    }
    rc = SandboxSyscall(sigsys.nr,
                        SECCOMP_PARM1(ctx), SECCOMP_PARM2(ctx),
                        SECCOMP_PARM3(ctx), SECCOMP_PARM4(ctx),
                        SECCOMP_PARM5(ctx), SECCOMP_PARM6(ctx));
  } else {
    const ErrorCode& err = trap_array_[info->si_errno - 1];
    if (!err.safe_) {
      SetIsInSigHandler();
    }

    // Copy the seccomp-specific data into a arch_seccomp_data structure. This
    // is what we are showing to TrapFnc callbacks that the system call
    // evaluator registered with the sandbox.
    struct arch_seccomp_data data = {
      sigsys.nr,
      SECCOMP_ARCH,
      reinterpret_cast<uint64_t>(sigsys.ip),
      {
        static_cast<uint64_t>(SECCOMP_PARM1(ctx)),
        static_cast<uint64_t>(SECCOMP_PARM2(ctx)),
        static_cast<uint64_t>(SECCOMP_PARM3(ctx)),
        static_cast<uint64_t>(SECCOMP_PARM4(ctx)),
        static_cast<uint64_t>(SECCOMP_PARM5(ctx)),
        static_cast<uint64_t>(SECCOMP_PARM6(ctx))
      }
    };

    // Now call the TrapFnc callback associated with this particular instance
    // of SECCOMP_RET_TRAP.
    rc = err.fnc_(data, err.aux_);
  }

  // Update the CPU register that stores the return code of the system call
  // that we just handled, and restore "errno" to the value that it had
  // before entering the signal handler.
  SECCOMP_RESULT(ctx) = static_cast<greg_t>(rc);
  errno               = old_errno;

  return;
}

bool Sandbox::TrapKey::operator<(const Sandbox::TrapKey& o) const {
  if (fnc != o.fnc) {
    return fnc < o.fnc;
  } else if (aux != o.aux) {
    return aux < o.aux;
  } else {
    return safe < o.safe;
  }
}

ErrorCode Sandbox::MakeTrap(ErrorCode::TrapFnc fnc, const void *aux,
                            bool safe) {
  // Each unique pair of TrapFnc and auxiliary data make up a distinct instance
  // of a SECCOMP_RET_TRAP.
  TrapKey key(fnc, aux, safe);
  TrapIds::const_iterator iter = trap_ids_.find(key);
  uint16_t id;
  if (iter != trap_ids_.end()) {
    // We have seen this pair before. Return the same id that we assigned
    // earlier.
    id = iter->second;
  } else {
    // This is a new pair. Remember it and assign a new id.
    // Please note that we have to store traps in memory that doesn't get
    // deallocated when the program is shutting down. A memory leak is
    // intentional, because we might otherwise not be able to execute
    // system calls part way through the program shutting down
    if (!traps_) {
      traps_ = new Traps();
    }
    if (traps_->size() >= SECCOMP_RET_DATA) {
      // In practice, this is pretty much impossible to trigger, as there
      // are other kernel limitations that restrict overall BPF program sizes.
      SANDBOX_DIE("Too many SECCOMP_RET_TRAP callback instances");
    }
    id = traps_->size() + 1;

    traps_->push_back(ErrorCode(fnc, aux, safe, id));
    trap_ids_[key] = id;

    // We want to access the traps_ vector from our signal handler. But
    // we are not assured that doing so is async-signal safe. On the other
    // hand, C++ guarantees that the contents of a vector is stored in a
    // contiguous C-style array.
    // So, we look up the address and size of this array outside of the
    // signal handler, where we can safely do so.
    trap_array_      = &(*traps_)[0];
    trap_array_size_ = id;
    return traps_->back();
  }

  return ErrorCode(fnc, aux, safe, id);
}

ErrorCode Sandbox::Trap(ErrorCode::TrapFnc fnc, const void *aux) {
  return MakeTrap(fnc, aux, true /* Safe Trap */);
}

ErrorCode Sandbox::UnsafeTrap(ErrorCode::TrapFnc fnc, const void *aux) {
  return MakeTrap(fnc, aux, false /* Unsafe Trap */);
}

intptr_t Sandbox::ForwardSyscall(const struct arch_seccomp_data& args) {
  return SandboxSyscall(args.nr,
                        static_cast<intptr_t>(args.args[0]),
                        static_cast<intptr_t>(args.args[1]),
                        static_cast<intptr_t>(args.args[2]),
                        static_cast<intptr_t>(args.args[3]),
                        static_cast<intptr_t>(args.args[4]),
                        static_cast<intptr_t>(args.args[5]));
}

intptr_t Sandbox::ReturnErrno(const struct arch_seccomp_data&, void *aux) {
  // TrapFnc functions report error by following the native kernel convention
  // of returning an exit code in the range of -1..-4096. They do not try to
  // set errno themselves. The glibc wrapper that triggered the SIGSYS will
  // ultimately do so for us.
  int err = reinterpret_cast<intptr_t>(aux) & SECCOMP_RET_DATA;
  return -err;
}

ErrorCode Sandbox::Cond(int argno, ErrorCode::ArgType width,
                        ErrorCode::Operation op, uint64_t value,
                        const ErrorCode& passed, const ErrorCode& failed) {
  return ErrorCode(argno, width, op, value,
                   &*conds_.insert(passed).first,
                   &*conds_.insert(failed).first);
}

intptr_t Sandbox::BpfFailure(const struct arch_seccomp_data&, void *aux) {
  SANDBOX_DIE(static_cast<char *>(aux));
}

ErrorCode Sandbox::Kill(const char *msg) {
  return Trap(BpfFailure, const_cast<char *>(msg));
}

Sandbox::SandboxStatus Sandbox::status_ = STATUS_UNKNOWN;
int Sandbox::proc_fd_                   = -1;
Sandbox::Evaluators Sandbox::evaluators_;
Sandbox::Traps *Sandbox::traps_         = NULL;
Sandbox::TrapIds Sandbox::trap_ids_;
ErrorCode *Sandbox::trap_array_         = NULL;
size_t Sandbox::trap_array_size_        = 0;
  bool Sandbox::has_unsafe_traps_       = false;
Sandbox::Conds Sandbox::conds_;

}  // namespace
