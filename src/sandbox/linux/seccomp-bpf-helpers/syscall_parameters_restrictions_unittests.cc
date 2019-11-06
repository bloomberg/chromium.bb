// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/elf.h>
#include <sched.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <time.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/system_headers/linux_time.h"
#include "sandbox/linux/tests/unit_tests.h"

#if !defined(OS_ANDROID)
#include "third_party/lss/linux_syscall_support.h"  // for MAKE_PROCESS_CPUCLOCK
#endif

namespace sandbox {

namespace {

// NOTE: most of the parameter restrictions are tested in
// baseline_policy_unittest.cc as a more end-to-end test.

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

class RestrictClockIdPolicy : public bpf_dsl::Policy {
 public:
  RestrictClockIdPolicy() {}
  ~RestrictClockIdPolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_clock_gettime:
      case __NR_clock_getres:
        return RestrictClockID();
      default:
        return Allow();
    }
  }
};

void CheckClock(clockid_t clockid) {
  struct timespec ts;
  ts.tv_sec = -1;
  ts.tv_nsec = -1;
  BPF_ASSERT_EQ(0, clock_getres(clockid, &ts));
  BPF_ASSERT_EQ(0, ts.tv_sec);
  BPF_ASSERT_LE(0, ts.tv_nsec);
  ts.tv_sec = -1;
  ts.tv_nsec = -1;
  BPF_ASSERT_EQ(0, clock_gettime(clockid, &ts));
  BPF_ASSERT_LE(0, ts.tv_sec);
  BPF_ASSERT_LE(0, ts.tv_nsec);
}

BPF_TEST_C(ParameterRestrictions,
           clock_gettime_allowed,
           RestrictClockIdPolicy) {
  CheckClock(CLOCK_MONOTONIC);
  CheckClock(CLOCK_MONOTONIC_COARSE);
  CheckClock(CLOCK_PROCESS_CPUTIME_ID);
#if defined(OS_ANDROID)
  CheckClock(CLOCK_BOOTTIME);
#endif
  CheckClock(CLOCK_REALTIME);
  CheckClock(CLOCK_REALTIME_COARSE);
  CheckClock(CLOCK_THREAD_CPUTIME_ID);
#if defined(OS_ANDROID)
  clockid_t clock_id;
  pthread_getcpuclockid(pthread_self(), &clock_id);
  CheckClock(clock_id);
#endif
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 clock_gettime_crash_monotonic_raw,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictClockIdPolicy) {
  struct timespec ts;
  syscall(SYS_clock_gettime, CLOCK_MONOTONIC_RAW, &ts);
}

#if !defined(OS_ANDROID)
BPF_DEATH_TEST_C(ParameterRestrictions,
                 clock_gettime_crash_cpu_clock,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictClockIdPolicy) {
  // We can't use clock_getcpuclockid() because it's not implemented in newlib,
  // and it might not work inside the sandbox anyway.
  const pid_t kInitPID = 1;
  const clockid_t kInitCPUClockID =
      MAKE_PROCESS_CPUCLOCK(kInitPID, CPUCLOCK_SCHED);

  struct timespec ts;
  clock_gettime(kInitCPUClockID, &ts);
}
#endif  // !defined(OS_ANDROID)

class RestrictSchedPolicy : public bpf_dsl::Policy {
 public:
  RestrictSchedPolicy() {}
  ~RestrictSchedPolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_sched_getparam:
        return RestrictSchedTarget(getpid(), sysno);
      default:
        return Allow();
    }
  }
};

void CheckSchedGetParam(pid_t pid, struct sched_param* param) {
  BPF_ASSERT_EQ(0, sched_getparam(pid, param));
}

void SchedGetParamThread(base::WaitableEvent* thread_run) {
  const pid_t pid = getpid();
  const pid_t tid = sys_gettid();
  BPF_ASSERT_NE(pid, tid);

  struct sched_param current_pid_param;
  CheckSchedGetParam(pid, &current_pid_param);

  struct sched_param zero_param;
  CheckSchedGetParam(0, &zero_param);

  struct sched_param tid_param;
  CheckSchedGetParam(tid, &tid_param);

  BPF_ASSERT_EQ(zero_param.sched_priority, tid_param.sched_priority);

  // Verify that the SIGSYS handler sets errno properly.
  errno = 0;
  BPF_ASSERT_EQ(-1, sched_getparam(tid, NULL));
  BPF_ASSERT_EQ(EINVAL, errno);

  thread_run->Signal();
}

BPF_TEST_C(ParameterRestrictions,
           sched_getparam_allowed,
           RestrictSchedPolicy) {
  base::WaitableEvent thread_run(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  // Run the actual test in a new thread so that the current pid and tid are
  // different.
  base::Thread getparam_thread("sched_getparam_thread");
  BPF_ASSERT(getparam_thread.Start());
  getparam_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&SchedGetParamThread, &thread_run));
  BPF_ASSERT(thread_run.TimedWait(base::TimeDelta::FromMilliseconds(5000)));
  getparam_thread.Stop();
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 sched_getparam_crash_non_zero,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictSchedPolicy) {
  const pid_t kInitPID = 1;
  struct sched_param param;
  sched_getparam(kInitPID, &param);
}

class RestrictPrlimit64Policy : public bpf_dsl::Policy {
 public:
  RestrictPrlimit64Policy() {}
  ~RestrictPrlimit64Policy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_prlimit64:
        return RestrictPrlimit64(getpid());
      default:
        return Allow();
    }
  }
};

BPF_TEST_C(ParameterRestrictions, prlimit64_allowed, RestrictPrlimit64Policy) {
  BPF_ASSERT_EQ(0, sys_prlimit64(0, RLIMIT_AS, NULL, NULL));
  BPF_ASSERT_EQ(0, sys_prlimit64(getpid(), RLIMIT_AS, NULL, NULL));
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 prlimit64_crash_not_self,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictPrlimit64Policy) {
  const pid_t kInitPID = 1;
  BPF_ASSERT_NE(kInitPID, getpid());
  sys_prlimit64(kInitPID, RLIMIT_AS, NULL, NULL);
}

class RestrictGetrusagePolicy : public bpf_dsl::Policy {
 public:
  RestrictGetrusagePolicy() {}
  ~RestrictGetrusagePolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_getrusage:
        return RestrictGetrusage();
      default:
        return Allow();
    }
  }
};

BPF_TEST_C(ParameterRestrictions, getrusage_allowed, RestrictGetrusagePolicy) {
  struct rusage usage;
  BPF_ASSERT_EQ(0, getrusage(RUSAGE_SELF, &usage));
}

BPF_DEATH_TEST_C(ParameterRestrictions,
                 getrusage_crash_not_self,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 RestrictGetrusagePolicy) {
  struct rusage usage;
  getrusage(RUSAGE_CHILDREN, &usage);
}

// ptace() Tests ///////////////////////////////////////////////////////////////

// Tests for ptrace involve a slightly complex setup in order to properly test
// ptrace and the variety of ways it is access-checked. The BPF_TEST macro,
// the body of which already runs in its own process, spawns another process
// called the "tracee". The "tracee" then spawns another process called the
// "tracer". The child then traces the parent and performs the test operations.
// The tracee must be careful to un-stop the tracer if the tracee expects to
// die.

class RestrictPtracePolicy : public bpf_dsl::Policy {
 public:
  RestrictPtracePolicy() = default;
  ~RestrictPtracePolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      case __NR_ptrace:
        return RestrictPtrace();
      default:
        return Allow();
    }
  }
};

constexpr char kExitPtraceChildClean = '!';

class PtraceTestHarness {
 public:
  using PtraceChildTracerFunc = void (*)(pid_t tracee);

  PtraceTestHarness(PtraceChildTracerFunc tracer_func, bool expect_death)
      : tracer_func_(tracer_func), expect_death_(expect_death) {}
  ~PtraceTestHarness() = default;

  void Run() {
    // Fork the tracee process that will be traced by its child.
    pid_t pid = fork();
    BPF_ASSERT_GE(pid, 0);

    if (pid == 0) {
      RunTracee();
    } else {
      // The tracee should always exit cleanly.
      int status = 0;
      int rv = waitpid(pid, &status, 0);
      BPF_ASSERT_EQ(pid, rv);
      BPF_ASSERT_EQ(0, WEXITSTATUS(status));
    }
  }

 private:
  void RunTracee() {
    // Create a communications pipe between tracer and tracee.
    int rv = pipe2(pipes_, O_NONBLOCK);
    BPF_ASSERT_EQ(0, rv);

    // Pipes for redirecting output.
    int output_pipes[2];
    BPF_ASSERT_EQ(0, pipe(output_pipes));

    // Create the tracer process.
    pid_t pid = fork();
    BPF_ASSERT_GE(pid, 0);

    if (pid == 0) {
      // Close the pipe read ends and redirect output.
      close(pipes_[0]);
      close(output_pipes[0]);

      close(STDOUT_FILENO);
      dup2(output_pipes[1], STDOUT_FILENO);

      close(STDERR_FILENO);
      dup2(output_pipes[1], STDERR_FILENO);

      RunTracer();

      close(output_pipes[1]);
    } else {
      close(pipes_[1]);
      close(output_pipes[1]);

      // Ensure the tracer can trace the tracee. This may fail on systems
      // without YAMA, so the result is not checked.
      prctl(PR_SET_PTRACER, pid);

      char c = 0;
      while (c != kExitPtraceChildClean) {
        // Read from the control channel in a non-blocking fashion.
        // If no data are present, loop.
        ignore_result(read(pipes_[0], &c, 1));

        // Poll the exit status of the child.
        int status = 0;
        rv = waitpid(pid, &status, WNOHANG);
        if (rv != 0) {
          BPF_ASSERT_EQ(pid, rv);
          CheckTracerStatus(status, output_pipes[0]);
          _exit(0);
        }
      }

      _exit(0);
    }
  }

  void RunTracer() {
    pid_t ppid = getppid();
    BPF_ASSERT_NE(0, ppid);

    // Attach to the tracee and then call out to the test function.
    BPF_ASSERT_EQ(0, ptrace(PTRACE_ATTACH, ppid, nullptr, nullptr));

    tracer_func_(ppid);

    BPF_ASSERT_EQ(1, HANDLE_EINTR(write(pipes_[1], &kExitPtraceChildClean, 1)));
    close(pipes_[1]);

    _exit(0);
  }

  void CheckTracerStatus(int status, int output_pipe) {
    // The child has exited. Test that it did so in the way we were
    // expecting.
    if (expect_death_) {
      // This duplicates a bit of what //sandbox/linux/tests/unit_tests.cc does
      // but that code is not shareable here.
      std::string output;
      const size_t kBufferSize = 1024;
      size_t total_bytes_read = 0;
      ssize_t read_this_pass = 0;
      do {
        output.resize(output.size() + kBufferSize);
        read_this_pass = HANDLE_EINTR(
            read(output_pipe, &output[total_bytes_read], kBufferSize));
        if (read_this_pass >= 0) {
          total_bytes_read += read_this_pass;
          output.resize(total_bytes_read);
        }
      } while (read_this_pass > 0);

#if !defined(SANDBOX_USES_BASE_TEST_SUITE)
      const bool subprocess_got_sigsegv =
          WIFSIGNALED(status) && (SIGSEGV == WTERMSIG(status));
#else
      // This hack is required when a signal handler is installed
      // for SEGV that will _exit(1).
      const bool subprocess_got_sigsegv =
          WIFEXITED(status) && (1 == WEXITSTATUS(status));
#endif
      BPF_ASSERT(subprocess_got_sigsegv);
      BPF_ASSERT_NE(output.find(GetPtraceErrorMessageContentForTests()),
                    std::string::npos);
    } else {
      BPF_ASSERT(WIFEXITED(status));
      BPF_ASSERT_EQ(0, WEXITSTATUS(status));
    }
  }

  PtraceChildTracerFunc tracer_func_;
  bool expect_death_;
  int pipes_[2];

  DISALLOW_COPY_AND_ASSIGN(PtraceTestHarness);
};

// Fails on Android L and M.
// See https://crbug.com/934930
BPF_TEST_C(ParameterRestrictions,
           DISABLED_ptrace_getregs_allowed,
           RestrictPtracePolicy) {
  auto tracer = [](pid_t pid) {
#if defined(__arm__)
    user_regs regs;
#else
    user_regs_struct regs;
#endif
    iovec iov;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);
    BPF_ASSERT_EQ(0, ptrace(PTRACE_GETREGSET, pid,
                            reinterpret_cast<void*>(NT_PRSTATUS), &iov));

    BPF_ASSERT_EQ(0, ptrace(PTRACE_DETACH, pid, nullptr, nullptr));
  };
  PtraceTestHarness(tracer, false).Run();
}

// Fails on Android L and M.
// See https://crbug.com/934930
BPF_TEST_C(ParameterRestrictions,
           DISABLED_ptrace_syscall_blocked,
           RestrictPtracePolicy) {
  auto tracer = [](pid_t pid) {
    // The tracer is about to die. Make sure the tracee is not stopped so it
    // can reap it and inspect its death signal.
    kill(pid, SIGCONT);

    BPF_ASSERT_NE(0, ptrace(PTRACE_SYSCALL, 0, nullptr, nullptr));
  };
  PtraceTestHarness(tracer, true).Run();
}

BPF_TEST_C(ParameterRestrictions,
           DISABLED_ptrace_setregs_blocked,
           RestrictPtracePolicy) {
  auto tracer = [](pid_t pid) {
#if defined(__arm__)
    user_regs regs;
#else
    user_regs_struct regs;
#endif
    iovec iov;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);
    BPF_ASSERT_EQ(0, ptrace(PTRACE_GETREGSET, pid,
                            reinterpret_cast<void*>(NT_PRSTATUS), &iov));

    // The tracer is about to die. Make sure the tracee is not stopped so it
    // can reap it and inspect its death signal.
    kill(pid, SIGCONT);

    BPF_ASSERT_NE(0, ptrace(PTRACE_SETREGSET, pid,
                            reinterpret_cast<void*>(NT_PRSTATUS), &iov));
  };
  PtraceTestHarness(tracer, true).Run();
}

}  // namespace

}  // namespace sandbox
