// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/syscall.h>
#include <sys/utsname.h>

#include <ostream>

#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "sandbox/linux/services/broker_process.h"
#include "sandbox/linux/services/linux_syscalls.h"
#include "testing/gtest/include/gtest/gtest.h"

// Workaround for Android's prctl.h file.
#if !defined(PR_CAPBSET_READ)
#define PR_CAPBSET_READ 23
#define PR_CAPBSET_DROP 24
#endif

using namespace playground2;
using sandbox::BrokerProcess;

namespace {

const int kExpectedReturnValue = 42;

// This test should execute no matter whether we have kernel support. So,
// we make it a TEST() instead of a BPF_TEST().
TEST(SandboxBpf, CallSupports) {
  // We check that we don't crash, but it's ok if the kernel doesn't
  // support it.
  bool seccomp_bpf_supported =
      Sandbox::SupportsSeccompSandbox(-1) == Sandbox::STATUS_AVAILABLE;
  // We want to log whether or not seccomp BPF is actually supported
  // since actual test coverage depends on it.
  RecordProperty("SeccompBPFSupported",
                 seccomp_bpf_supported ? "true." : "false.");
  std::cout << "Seccomp BPF supported: "
            << (seccomp_bpf_supported ? "true." : "false.")
            << "\n";
  RecordProperty("PointerSize", sizeof(void*));
  std::cout << "Pointer size: " << sizeof(void*) << "\n";
}

SANDBOX_TEST(SandboxBpf, CallSupportsTwice) {
  Sandbox::SupportsSeccompSandbox(-1);
  Sandbox::SupportsSeccompSandbox(-1);
}

// BPF_TEST does a lot of the boiler-plate code around setting up a
// policy and optional passing data between the caller, the policy and
// any Trap() handlers. This is great for writing short and concise tests,
// and it helps us accidentally forgetting any of the crucial steps in
// setting up the sandbox. But it wouldn't hurt to have at least one test
// that explicitly walks through all these steps.

intptr_t FakeGetPid(const struct arch_seccomp_data& args, void *aux) {
  BPF_ASSERT(aux);
  pid_t *pid_ptr = static_cast<pid_t *>(aux);
  return (*pid_ptr)++;
}

ErrorCode VerboseAPITestingPolicy(int sysno, void *aux) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    return ErrorCode(ENOSYS);
  } else if (sysno == __NR_getpid) {
    return Sandbox::Trap(FakeGetPid, aux);
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

SANDBOX_TEST(SandboxBpf, VerboseAPITesting) {
  if (Sandbox::SupportsSeccompSandbox(-1) ==
      playground2::Sandbox::STATUS_AVAILABLE) {
    pid_t test_var = 0;
    playground2::Sandbox::SetSandboxPolicy(VerboseAPITestingPolicy, &test_var);
    playground2::Sandbox::StartSandbox();

    BPF_ASSERT(test_var == 0);
    BPF_ASSERT(syscall(__NR_getpid) == 0);
    BPF_ASSERT(test_var == 1);
    BPF_ASSERT(syscall(__NR_getpid) == 1);
    BPF_ASSERT(test_var == 2);

    // N.B.: Any future call to getpid() would corrupt the stack.
    //       This is OK. The SANDBOX_TEST() macro is guaranteed to
    //       only ever call _exit() after the test completes.
  }
}

// A simple blacklist test

ErrorCode BlacklistNanosleepPolicy(int sysno, void *) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
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

ErrorCode WhitelistGetpidPolicy(int sysno, void *) {
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

intptr_t EnomemHandler(const struct arch_seccomp_data& args, void *aux) {
  // We also check that the auxiliary data is correct
  SANDBOX_ASSERT(aux);
  *(static_cast<int*>(aux)) = kExpectedReturnValue;
  return -ENOMEM;
}

ErrorCode BlacklistNanosleepPolicySigsys(int sysno, void *aux) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    // FIXME: we should really not have to do that in a trivial policy
    return ErrorCode(ENOSYS);
  }

  switch (sysno) {
    case __NR_nanosleep:
      return Sandbox::Trap(EnomemHandler, aux);
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(SandboxBpf, BasicBlacklistWithSigsys,
         BlacklistNanosleepPolicySigsys, int /* BPF_AUX */) {
  // getpid() should work properly
  errno = 0;
  BPF_ASSERT(syscall(__NR_getpid) > 0);
  BPF_ASSERT(errno == 0);

  // Our Auxiliary Data, should be reset by the signal handler
  BPF_AUX = -1;
  const struct timespec ts = {0, 0};
  BPF_ASSERT(syscall(__NR_nanosleep, &ts, NULL) == -1);
  BPF_ASSERT(errno == ENOMEM);

  // We expect the signal handler to modify AuxData
  BPF_ASSERT(BPF_AUX == kExpectedReturnValue);
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

ErrorCode SyntheticPolicy(int sysno, void *) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
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

ErrorCode ArmPrivatePolicy(int sysno, void *) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
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

intptr_t CountSyscalls(const struct arch_seccomp_data& args, void *aux) {
  // Count all invocations of our callback function.
  ++*reinterpret_cast<int *>(aux);

  // Verify that within the callback function all filtering is temporarily
  // disabled.
  BPF_ASSERT(syscall(__NR_getpid) > 1);

  // Verify that we can now call the underlying system call without causing
  // infinite recursion.
  return Sandbox::ForwardSyscall(args);
}

ErrorCode GreyListedPolicy(int sysno, void *aux) {
  // The use of UnsafeTrap() causes us to print a warning message. This is
  // generally desirable, but it results in the unittest failing, as it doesn't
  // expect any messages on "stderr". So, temporarily disable messages. The
  // BPF_TEST() is guaranteed to turn messages back on, after the policy
  // function has completed.
  Die::SuppressInfoMessages(true);

  // Some system calls must always be allowed, if our policy wants to make
  // use of UnsafeTrap()
  if (sysno == __NR_rt_sigprocmask ||
      sysno == __NR_rt_sigreturn
#if defined(__NR_sigprocmask)
   || sysno == __NR_sigprocmask
#endif
#if defined(__NR_sigreturn)
   || sysno == __NR_sigreturn
#endif
      ) {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  } else if (sysno == __NR_getpid) {
    // Disallow getpid()
    return ErrorCode(EPERM);
  } else if (Sandbox::IsValidSyscallNumber(sysno)) {
    // Allow (and count) all other system calls.
      return Sandbox::UnsafeTrap(CountSyscalls, aux);
  } else {
    return ErrorCode(ENOSYS);
  }
}

BPF_TEST(SandboxBpf, GreyListedPolicy,
         GreyListedPolicy, int /* BPF_AUX */) {
  BPF_ASSERT(syscall(__NR_getpid) == -1);
  BPF_ASSERT(errno == EPERM);
  BPF_ASSERT(BPF_AUX == 0);
  BPF_ASSERT(syscall(__NR_geteuid) == syscall(__NR_getuid));
  BPF_ASSERT(BPF_AUX == 2);
  char name[17] = { };
  BPF_ASSERT(!syscall(__NR_prctl, PR_GET_NAME, name, (void *)NULL,
                      (void *)NULL, (void *)NULL));
  BPF_ASSERT(BPF_AUX == 3);
  BPF_ASSERT(*name);
}

intptr_t PrctlHandler(const struct arch_seccomp_data& args, void *) {
  if (args.args[0] == PR_CAPBSET_DROP &&
      static_cast<int>(args.args[1]) == -1) {
    // prctl(PR_CAPBSET_DROP, -1) is never valid. The kernel will always
    // return an error. But our handler allows this call.
    return 0;
  } else {
    return Sandbox::ForwardSyscall(args);
  }
}

ErrorCode PrctlPolicy(int sysno, void *aux) {
  Die::SuppressInfoMessages(true);

  if (sysno == __NR_prctl) {
    // Handle prctl() inside an UnsafeTrap()
    return Sandbox::UnsafeTrap(PrctlHandler, NULL);
  } else if (Sandbox::IsValidSyscallNumber(sysno)) {
    // Allow all other system calls.
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  } else {
    return ErrorCode(ENOSYS);
  }
}

BPF_TEST(SandboxBpf, ForwardSyscall, PrctlPolicy) {
  // This call should never be allowed. But our policy will intercept it and
  // let it pass successfully.
  BPF_ASSERT(!prctl(PR_CAPBSET_DROP, -1, (void *)NULL, (void *)NULL,
                    (void *)NULL));

  // Verify that the call will fail, if it makes it all the way to the kernel.
  BPF_ASSERT(prctl(PR_CAPBSET_DROP, -2, (void *)NULL, (void *)NULL,
                   (void *)NULL) == -1);

  // And verify that other uses of prctl() work just fine.
  char name[17] = { };
  BPF_ASSERT(!syscall(__NR_prctl, PR_GET_NAME, name, (void *)NULL,
                      (void *)NULL, (void *)NULL));
  BPF_ASSERT(*name);

  // Finally, verify that system calls other than prctl() are completely
  // unaffected by our policy.
  struct utsname uts = { };
  BPF_ASSERT(!uname(&uts));
  BPF_ASSERT(!strcmp(uts.sysname, "Linux"));
}

intptr_t AllowRedirectedSyscall(const struct arch_seccomp_data& args, void *) {
  return Sandbox::ForwardSyscall(args);
}

ErrorCode RedirectAllSyscallsPolicy(int sysno, void *aux) {
  Die::SuppressInfoMessages(true);

  // Some system calls must always be allowed, if our policy wants to make
  // use of UnsafeTrap()
  if (sysno == __NR_rt_sigprocmask ||
      sysno == __NR_rt_sigreturn
#if defined(__NR_sigprocmask)
   || sysno == __NR_sigprocmask
#endif
#if defined(__NR_sigreturn)
   || sysno == __NR_sigreturn
#endif
      ) {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  } else if (Sandbox::IsValidSyscallNumber(sysno)) {
    return Sandbox::UnsafeTrap(AllowRedirectedSyscall, aux);
  } else {
    return ErrorCode(ENOSYS);
  }
}

int bus_handler_fd_ = -1;

void SigBusHandler(int, siginfo_t *info, void *void_context) {
  BPF_ASSERT(write(bus_handler_fd_, "\x55", 1) == 1);
}

BPF_TEST(SandboxBpf, SigBus, RedirectAllSyscallsPolicy) {
  // We use the SIGBUS bit in the signal mask as a thread-local boolean
  // value in the implementation of UnsafeTrap(). This is obviously a bit
  // of a hack that could conceivably interfere with code that uses SIGBUS
  // in more traditional ways. This test verifies that basic functionality
  // of SIGBUS is not impacted, but it is certainly possibly to construe
  // more complex uses of signals where our use of the SIGBUS mask is not
  // 100% transparent. This is expected behavior.
  int fds[2];
  BPF_ASSERT(pipe(fds) == 0);
  bus_handler_fd_ = fds[1];
  struct sigaction sa = { };
  sa.sa_sigaction = SigBusHandler;
  sa.sa_flags = SA_SIGINFO;
  BPF_ASSERT(sigaction(SIGBUS, &sa, NULL) == 0);
  raise(SIGBUS);
  char c = '\000';
  BPF_ASSERT(read(fds[0], &c, 1) == 1);
  BPF_ASSERT(close(fds[0]) == 0);
  BPF_ASSERT(close(fds[1]) == 0);
  BPF_ASSERT(c == 0x55);
}

BPF_TEST(SandboxBpf, SigMask, RedirectAllSyscallsPolicy) {
  // Signal masks are potentially tricky to handle. For instance, if we
  // ever tried to update them from inside a Trap() or UnsafeTrap() handler,
  // the call to sigreturn() at the end of the signal handler would undo
  // all of our efforts. So, it makes sense to test that sigprocmask()
  // works, even if we have a policy in place that makes use of UnsafeTrap().
  // In practice, this works because we force sigprocmask() to be handled
  // entirely in the kernel.
  sigset_t mask0, mask1, mask2;

  // Call sigprocmask() to verify that SIGUSR2 wasn't blocked, if we didn't
  // change the mask (it shouldn't have been, as it isn't blocked by default
  // in POSIX).
  //
  // Use SIGUSR2 because Android seems to use SIGUSR1 for some purpose.
  sigemptyset(&mask0);
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, &mask0, &mask1));
  BPF_ASSERT(!sigismember(&mask1, SIGUSR2));

  // Try again, and this time we verify that we can block it. This
  // requires a second call to sigprocmask().
  sigaddset(&mask0, SIGUSR2);
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, &mask0, NULL));
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, NULL, &mask2));
  BPF_ASSERT( sigismember(&mask2, SIGUSR2));
}

BPF_TEST(SandboxBpf, UnsafeTrapWithErrno, RedirectAllSyscallsPolicy) {
  // An UnsafeTrap() (or for that matter, a Trap()) has to report error
  // conditions by returning an exit code in the range -1..-4096. This
  // should happen automatically if using ForwardSyscall(). If the TrapFnc()
  // uses some other method to make system calls, then it is responsible
  // for computing the correct return code.
  // This test verifies that ForwardSyscall() does the correct thing.

  // The glibc system wrapper will ultimately set errno for us. So, from normal
  // userspace, all of this should be completely transparent.
  errno = 0;
  BPF_ASSERT(close(-1) == -1);
  BPF_ASSERT(errno == EBADF);

  // Explicitly avoid the glibc wrapper. This is not normally the way anybody
  // would make system calls, but it allows us to verify that we don't
  // accidentally mess with errno, when we shouldn't.
  errno = 0;
  struct arch_seccomp_data args = { };
  args.nr      = __NR_close;
  args.args[0] = -1;
  BPF_ASSERT(Sandbox::ForwardSyscall(args) == -EBADF);
  BPF_ASSERT(errno == 0);
}

// Test a trap handler that makes use of a broker process to open().

class InitializedOpenBroker {
 public:
  InitializedOpenBroker() : initialized_(false) {
    std::vector<std::string> allowed_files;
    allowed_files.push_back("/proc/allowed");
    allowed_files.push_back("/proc/cpuinfo");

    broker_process_.reset(new BrokerProcess(allowed_files,
                                            std::vector<std::string>()));
    BPF_ASSERT(broker_process() != NULL);
    BPF_ASSERT(broker_process_->Init(NULL));

    initialized_ = true;
  }
  bool initialized() { return initialized_; }
  class BrokerProcess* broker_process() { return broker_process_.get(); }
 private:
  bool initialized_;
  scoped_ptr<class BrokerProcess> broker_process_;
  DISALLOW_COPY_AND_ASSIGN(InitializedOpenBroker);
};

intptr_t BrokerOpenTrapHandler(const struct arch_seccomp_data& args,
                               void *aux) {
  BPF_ASSERT(aux);
  BrokerProcess* broker_process = static_cast<BrokerProcess*>(aux);
  switch(args.nr) {
    case __NR_open:
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
          static_cast<int>(args.args[1]));
    case __NR_openat:
      // We only call open() so if we arrive here, it's because glibc uses
      // the openat() system call.
      BPF_ASSERT(static_cast<int>(args.args[0]) == AT_FDCWD);
      return broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
          static_cast<int>(args.args[2]));
    default:
      BPF_ASSERT(false);
      return -ENOSYS;
  }
}

ErrorCode DenyOpenPolicy(int sysno, void *aux) {
  InitializedOpenBroker* iob = static_cast<InitializedOpenBroker*>(aux);
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    return ErrorCode(ENOSYS);
  }

  switch (sysno) {
    case __NR_open:
    case __NR_openat:
      // We get a InitializedOpenBroker class, but our trap handler wants
      // the BrokerProcess object.
      return ErrorCode(Sandbox::Trap(BrokerOpenTrapHandler,
                                     iob->broker_process()));
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

// We use a InitializedOpenBroker class, so that we can run unsandboxed
// code in its constructor, which is the only way to do so in a BPF_TEST.
BPF_TEST(SandboxBpf, UseOpenBroker, DenyOpenPolicy,
         InitializedOpenBroker /* BPF_AUX */) {
  BPF_ASSERT(BPF_AUX.initialized());
  BrokerProcess* broker_process =  BPF_AUX.broker_process();
  BPF_ASSERT(broker_process != NULL);

  // First, use the broker "manually"
  BPF_ASSERT(broker_process->Open("/proc/denied", O_RDONLY) == -EPERM);
  BPF_ASSERT(broker_process->Open("/proc/allowed", O_RDONLY) == -ENOENT);

  // Now use glibc's open() as an external library would.
  BPF_ASSERT(open("/proc/denied", O_RDONLY) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(open("/proc/allowed", O_RDONLY) == -1);
  BPF_ASSERT(errno == ENOENT);

  // Also test glibc's openat(), some versions of libc use it transparently
  // instead of open().
  BPF_ASSERT(openat(AT_FDCWD, "/proc/denied", O_RDONLY) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(openat(AT_FDCWD, "/proc/allowed", O_RDONLY) == -1);
  BPF_ASSERT(errno == ENOENT);


  // This is also white listed and does exist.
  int cpu_info_fd = open("/proc/cpuinfo", O_RDONLY);
  BPF_ASSERT(cpu_info_fd >= 0);
  char buf[1024];
  BPF_ASSERT(read(cpu_info_fd, buf, sizeof(buf)) > 0);
}

// This test exercises the Sandbox::Cond() method by building a complex
// tree of conditional equality operations. It then makes system calls and
// verifies that they return the values that we expected from our BPF
// program.
class EqualityStressTest {
 public:
  EqualityStressTest() {
    // We want a deterministic test
    srand(0);

    // Iterates over system call numbers and builds a random tree of
    // equality tests.
    // We are actually constructing a graph of ArgValue objects. This
    // graph will later be used to a) compute our sandbox policy, and
    // b) drive the code that verifies the output from the BPF program.
    COMPILE_ASSERT(kNumTestCases < (int)(MAX_PUBLIC_SYSCALL-MIN_SYSCALL-10),
           num_test_cases_must_be_significantly_smaller_than_num_system_calls);
    for (int sysno = MIN_SYSCALL, end = kNumTestCases; sysno < end; ++sysno) {
      if (IsReservedSyscall(sysno)) {
        // Skip reserved system calls. This ensures that our test frame
        // work isn't impacted by the fact that we are overriding
        // a lot of different system calls.
        ++end;
        arg_values_.push_back(NULL);
      } else {
        arg_values_.push_back(RandomArgValue(rand() % kMaxArgs, 0,
                                             rand() % kMaxArgs));
      }
    }
  }

  ~EqualityStressTest() {
    for (std::vector<ArgValue *>::iterator iter = arg_values_.begin();
         iter != arg_values_.end();
         ++iter) {
      DeleteArgValue(*iter);
    }
  }

  ErrorCode Policy(int sysno) {
    if (!Sandbox::IsValidSyscallNumber(sysno)) {
      // FIXME: we should really not have to do that in a trivial policy
      return ErrorCode(ENOSYS);
    } else if (sysno < 0 || sysno >= (int)arg_values_.size() ||
               IsReservedSyscall(sysno)) {
      // We only return ErrorCode values for the system calls that
      // are part of our test data. Every other system call remains
      // allowed.
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    } else {
      // ToErrorCode() turns an ArgValue object into an ErrorCode that is
      // suitable for use by a sandbox policy.
      return ToErrorCode(arg_values_[sysno]);
    }
  }

  void VerifyFilter() {
    // Iterate over all system calls. Skip the system calls that have
    // previously been determined as being reserved.
    for (int sysno = 0; sysno < (int)arg_values_.size(); ++sysno) {
      if (!arg_values_[sysno]) {
        // Skip reserved system calls.
        continue;
      }
      // Verify that system calls return the values that we expect them to
      // return. This involves passing different combinations of system call
      // parameters in order to exercise all possible code paths through the
      // BPF filter program.
      // We arbitrarily start by setting all six system call arguments to
      // zero. And we then recursive traverse our tree of ArgValues to
      // determine the necessary combinations of parameters.
      intptr_t args[6] = { };
      Verify(sysno, args, *arg_values_[sysno]);
    }
  }

 private:
  struct ArgValue {
    int argno;                     // Argument number to inspect.
    int size;                      // Number of test cases (must be > 0).
    struct Tests {
      uint32_t k_value;            // Value to compare syscall arg against.
      int      err;                // If non-zero, errno value to return.
      struct ArgValue *arg_value;  // Otherwise, more args needs inspecting.
    } *tests;
    int err;                       // If none of the tests passed, this is what
    struct ArgValue *arg_value;    // we'll return (this is the "else" branch).
  };

  bool IsReservedSyscall(int sysno) {
    // There are a handful of system calls that we should never use in our
    // test cases. These system calls are needed to allow the test framework
    // to run properly.
    // If we wanted to write fully generic code, there are more system calls
    // that could be listed here, and it is quite difficult to come up with a
    // truly comprehensive list. After all, we are deliberately making system
    // calls unavailable. In practice, we have a pretty good idea of the system
    // calls that will be made by this particular test. So, this small list is
    // sufficient. But if anybody copy'n'pasted this code for other uses, they
    // would have to review that the list.
    return sysno == __NR_read       ||
           sysno == __NR_write      ||
           sysno == __NR_exit       ||
           sysno == __NR_exit_group ||
           sysno == __NR_restart_syscall;
  }

  ArgValue *RandomArgValue(int argno, int args_mask, int remaining_args) {
    // Create a new ArgValue and fill it with random data. We use as bit mask
    // to keep track of the system call parameters that have previously been
    // set; this ensures that we won't accidentally define a contradictory
    // set of equality tests.
    struct ArgValue *arg_value = new ArgValue();
    args_mask        |= 1 << argno;
    arg_value->argno  = argno;

    // Apply some restrictions on just how complex our tests can be.
    // Otherwise, we end up with a BPF program that is too complicated for
    // the kernel to load.
    int fan_out       = kMaxFanOut;
    if (remaining_args > 3) {
      fan_out         = 1;
    } else if (remaining_args > 2) {
      fan_out         = 2;
    }

    // Create a couple of different test cases with randomized values that
    // we want to use when comparing system call parameter number "argno".
    arg_value->size   = rand() % fan_out + 1;
    arg_value->tests  = new ArgValue::Tests[arg_value->size];

    uint32_t k_value  = rand();
    for (int n = 0; n < arg_value->size; ++n) {
      // Ensure that we have unique values
      k_value += rand() % (RAND_MAX/(kMaxFanOut+1)) + 1;

      // There are two possible types of nodes. Either this is a leaf node;
      // in that case, we have completed all the equality tests that we
      // wanted to perform, and we can now compute a random "errno" value that
      // we should return. Or this is part of a more complex boolean
      // expression; in that case, we have to recursively add tests for some
      // of system call parameters that we have not yet included in our
      // tests.
      arg_value->tests[n].k_value = k_value;
      if (!remaining_args || (rand() & 1)) {
        arg_value->tests[n].err = (rand() % 1000) + 1;
        arg_value->tests[n].arg_value = NULL;
      } else {
        arg_value->tests[n].err = 0;
        arg_value->tests[n].arg_value =
          RandomArgValue(RandomArg(args_mask), args_mask, remaining_args - 1);
      }
    }
    // Finally, we have to define what we should return if none of the
    // previous equality tests pass. Again, we can either deal with a leaf
    // node, or we can randomly add another couple of tests.
    if (!remaining_args || (rand() & 1)) {
      arg_value->err = (rand() % 1000) + 1;
      arg_value->arg_value = NULL;
    } else {
      arg_value->err = 0;
      arg_value->arg_value =
        RandomArgValue(RandomArg(args_mask), args_mask, remaining_args - 1);
    }
    // We have now built a new (sub-)tree of ArgValues defining a set of
    // boolean expressions for testing random system call arguments against
    // random values. Return this tree to our caller.
    return arg_value;
  }

  int RandomArg(int args_mask) {
    // Compute a random system call parameter number.
    int argno = rand() % kMaxArgs;

    // Make sure that this same parameter number has not previously been
    // used. Otherwise, we could end up with a test that is impossible to
    // satisfy (e.g. args[0] == 1 && args[0] == 2).
    while (args_mask & (1 << argno)) {
      argno = (argno + 1) % kMaxArgs;
    }
    return argno;
  }

  void DeleteArgValue(ArgValue *arg_value) {
    // Delete an ArgValue and all of its child nodes. This requires
    // recursively descending into the tree.
    if (arg_value) {
      if (arg_value->size) {
        for (int n = 0; n < arg_value->size; ++n) {
          if (!arg_value->tests[n].err) {
            DeleteArgValue(arg_value->tests[n].arg_value);
          }
        }
        delete[] arg_value->tests;
      }
      if (!arg_value->err) {
        DeleteArgValue(arg_value->arg_value);
      }
      delete arg_value;
    }
  }

  ErrorCode ToErrorCode(ArgValue *arg_value) {
    // Compute the ErrorCode that should be returned, if none of our
    // tests succeed (i.e. the system call parameter doesn't match any
    // of the values in arg_value->tests[].k_value).
    ErrorCode err;
    if (arg_value->err) {
      // If this was a leaf node, return the errno value that we expect to
      // return from the BPF filter program.
      err = ErrorCode(arg_value->err);
    } else {
      // If this wasn't a leaf node yet, recursively descend into the rest
      // of the tree. This will end up adding a few more Sandbox::Cond()
      // tests to our ErrorCode.
      err = ToErrorCode(arg_value->arg_value);
    }

    // Now, iterate over all the test cases that we want to compare against.
    // This builds a chain of Sandbox::Cond() tests
    // (aka "if ... elif ... elif ... elif ... fi")
    for (int n = arg_value->size; n-- > 0; ) {
      ErrorCode matched;
      // Again, we distinguish between leaf nodes and subtrees.
      if (arg_value->tests[n].err) {
        matched = ErrorCode(arg_value->tests[n].err);
      } else {
        matched = ToErrorCode(arg_value->tests[n].arg_value);
      }
      // For now, all of our tests are limited to 32bit.
      // We have separate tests that check the behavior of 32bit vs. 64bit
      // conditional expressions.
      err = Sandbox::Cond(arg_value->argno, ErrorCode::TP_32BIT,
                          ErrorCode::OP_EQUAL, arg_value->tests[n].k_value,
                          matched, err);
    }
    return err;
  }

  void Verify(int sysno, intptr_t *args, const ArgValue& arg_value) {
    uint32_t mismatched = 0;
    // Iterate over all the k_values in arg_value.tests[] and verify that
    // we see the expected return values from system calls, when we pass
    // the k_value as a parameter in a system call.
    for (int n = arg_value.size; n-- > 0; ) {
      mismatched += arg_value.tests[n].k_value;
      args[arg_value.argno] = arg_value.tests[n].k_value;
      if (arg_value.tests[n].err) {
        VerifyErrno(sysno, args, arg_value.tests[n].err);
      } else {
        Verify(sysno, args, *arg_value.tests[n].arg_value);
      }
    }
    // Find a k_value that doesn't match any of the k_values in
    // arg_value.tests[]. In most cases, the current value of "mismatched"
    // would fit this requirement. But on the off-chance that it happens
    // to collide, we double-check.
  try_again:
    for (int n = arg_value.size; n-- > 0; ) {
      if (mismatched == arg_value.tests[n].k_value) {
        ++mismatched;
        goto try_again;
      }
    }
    // Now verify that we see the expected return value from system calls,
    // if we pass a value that doesn't match any of the conditions (i.e. this
    // is testing the "else" clause of the conditions).
    args[arg_value.argno] = mismatched;
    if (arg_value.err) {
      VerifyErrno(sysno, args, arg_value.err);
    } else {
      Verify(sysno, args, *arg_value.arg_value);
    }
    // Reset args[arg_value.argno]. This is not technically needed, but it
    // makes it easier to reason about the correctness of our tests.
    args[arg_value.argno] = 0;
  }

  void VerifyErrno(int sysno, intptr_t *args, int err) {
    // We installed BPF filters that return different errno values
    // based on the system call number and the parameters that we decided
    // to pass in. Verify that this condition holds true.
    BPF_ASSERT(SandboxSyscall(sysno,
                              args[0], args[1], args[2],
                              args[3], args[4], args[5]) == -err);
  }

  // Vector of ArgValue trees. These trees define all the possible boolean
  // expressions that we want to turn into a BPF filter program.
  std::vector<ArgValue *> arg_values_;

  // Don't increase these values. We are pushing the limits of the maximum
  // BPF program that the kernel will allow us to load. If the values are
  // increased too much, the test will start failing.
  static const int kNumIterations = 3;
  static const int kNumTestCases = 40;
  static const int kMaxFanOut = 3;
  static const int kMaxArgs = 6;
};

ErrorCode EqualityStressTestPolicy(int sysno, void *aux) {
  return reinterpret_cast<EqualityStressTest *>(aux)->Policy(sysno);
}

BPF_TEST(SandboxBpf, EqualityTests, EqualityStressTestPolicy,
         EqualityStressTest /* BPF_AUX */) {
  BPF_AUX.VerifyFilter();
}

ErrorCode EqualityArgumentWidthPolicy(int sysno, void *) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    // FIXME: we should really not have to do that in a trivial policy
    return ErrorCode(ENOSYS);
  } else if (sysno == __NR_uname) {
    return Sandbox::Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 0,
           Sandbox::Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         0x55555555, ErrorCode(1), ErrorCode(2)),
           Sandbox::Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_EQUAL,
                         0x55555555AAAAAAAAull, ErrorCode(1), ErrorCode(2)));
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(SandboxBpf, EqualityArgumentWidth, EqualityArgumentWidthPolicy) {
  BPF_ASSERT(SandboxSyscall(__NR_uname, 0, 0x55555555) == -1);
  BPF_ASSERT(SandboxSyscall(__NR_uname, 0, 0xAAAAAAAA) == -2);
#if __SIZEOF_POINTER__ > 4
  // On 32bit machines, there is no way to pass a 64bit argument through the
  // syscall interface. So, we have to skip the part of the test that requires
  // 64bit arguments.
  BPF_ASSERT(SandboxSyscall(__NR_uname, 1, 0x55555555AAAAAAAAull) == -1);
  BPF_ASSERT(SandboxSyscall(__NR_uname, 1, 0x5555555500000000ull) == -2);
  BPF_ASSERT(SandboxSyscall(__NR_uname, 1, 0x5555555511111111ull) == -2);
  BPF_ASSERT(SandboxSyscall(__NR_uname, 1, 0x11111111AAAAAAAAull) == -2);
#endif
}

#if __SIZEOF_POINTER__ > 4
// On 32bit machines, there is no way to pass a 64bit argument through the
// syscall interface. So, we have to skip the part of the test that requires
// 64bit arguments.
BPF_DEATH_TEST(SandboxBpf, EqualityArgumentUnallowed64bit,
               DEATH_MESSAGE("Unexpected 64bit argument detected"),
               EqualityArgumentWidthPolicy) {
  SandboxSyscall(__NR_uname, 0, 0x5555555555555555ull);
}
#endif

ErrorCode EqualityWithNegativeArgumentsPolicy(int sysno, void *) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    // FIXME: we should really not have to do that in a trivial policy
    return ErrorCode(ENOSYS);
  } else if (sysno == __NR_uname) {
    return Sandbox::Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         0xFFFFFFFF, ErrorCode(1), ErrorCode(2));
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(SandboxBpf, EqualityWithNegativeArguments,
         EqualityWithNegativeArgumentsPolicy) {
  BPF_ASSERT(SandboxSyscall(__NR_uname, 0xFFFFFFFF) == -1);
  BPF_ASSERT(SandboxSyscall(__NR_uname, -1) == -1);
  BPF_ASSERT(SandboxSyscall(__NR_uname, -1ll) == -1);
}

#if __SIZEOF_POINTER__ > 4
BPF_DEATH_TEST(SandboxBpf, EqualityWithNegative64bitArguments,
               DEATH_MESSAGE("Unexpected 64bit argument detected"),
               EqualityWithNegativeArgumentsPolicy) {
  // When expecting a 32bit system call argument, we look at the MSB of the
  // 64bit value and allow both "0" and "-1". But the latter is allowed only
  // iff the LSB was negative. So, this death test should error out.
  BPF_ASSERT(SandboxSyscall(__NR_uname, 0xFFFFFFFF00000000ll) == -1);
}
#endif

} // namespace
