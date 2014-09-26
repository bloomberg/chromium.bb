// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"

#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <linux/net.h>
#include <sched.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"

#if defined(OS_ANDROID)

#include "sandbox/linux/services/android_futex.h"

#if !defined(F_DUPFD_CLOEXEC)
#define F_DUPFD_CLOEXEC (F_LINUX_SPECIFIC_BASE + 6)
#endif

#endif  // defined(OS_ANDROID)

#if defined(__arm__) && !defined(MAP_STACK)
#define MAP_STACK 0x20000  // Daisy build environment has old headers.
#endif

#if defined(__mips__) && !defined(MAP_STACK)
#define MAP_STACK 0x40000
#endif
namespace {

inline bool IsArchitectureX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureI386() {
#if defined(__i386__)
  return true;
#else
  return false;
#endif
}

inline bool IsAndroid() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureMips() {
#if defined(__mips__)
  return true;
#else
  return false;
#endif
}

}  // namespace.

#define CASES SANDBOX_BPF_DSL_CASES

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::BoolExpr;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {

// Allow Glibc's and Android pthread creation flags, crash on any other
// thread creation attempts and EPERM attempts to use neither
// CLONE_VM, nor CLONE_THREAD, which includes all fork() implementations.
ResultExpr RestrictCloneToThreadsAndEPERMFork() {
  const Arg<unsigned long> flags(0);

  // TODO(mdempsky): Extend DSL to support (flags & ~mask1) == mask2.
  const uint64_t kAndroidCloneMask = CLONE_VM | CLONE_FS | CLONE_FILES |
                                     CLONE_SIGHAND | CLONE_THREAD |
                                     CLONE_SYSVSEM;
  const uint64_t kObsoleteAndroidCloneMask = kAndroidCloneMask | CLONE_DETACHED;

  const uint64_t kGlibcPthreadFlags =
      CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
      CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
  const BoolExpr glibc_test = flags == kGlibcPthreadFlags;

  const BoolExpr android_test = flags == kAndroidCloneMask ||
                                flags == kObsoleteAndroidCloneMask ||
                                flags == kGlibcPthreadFlags;

  return If(IsAndroid() ? android_test : glibc_test, Allow())
      .ElseIf((flags & (CLONE_VM | CLONE_THREAD)) == 0, Error(EPERM))
      .Else(CrashSIGSYSClone());
}

ResultExpr RestrictPrctl() {
  // Will need to add seccomp compositing in the future. PR_SET_PTRACER is
  // used by breakpad but not needed anymore.
  const Arg<int> option(0);
  return Switch(option)
      .CASES((PR_GET_NAME, PR_SET_NAME, PR_GET_DUMPABLE, PR_SET_DUMPABLE),
             Allow())
      .Default(CrashSIGSYSPrctl());
}

ResultExpr RestrictIoctl() {
  const Arg<int> request(1);
  return Switch(request).CASES((TCGETS, FIONREAD), Allow()).Default(
      CrashSIGSYSIoctl());
}

ResultExpr RestrictMmapFlags() {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit MAP_HUGETLB, or the newer flags such as
  // MAP_POPULATE.
  // TODO(davidung), remove MAP_DENYWRITE with updated Tegra libraries.
  const uint64_t kAllowedMask = MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS |
                                MAP_STACK | MAP_NORESERVE | MAP_FIXED |
                                MAP_DENYWRITE;
  const Arg<int> flags(3);
  return If((flags & ~kAllowedMask) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictMprotectFlags() {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit weird undocumented flags such as
  // PROT_GROWSDOWN.
  const uint64_t kAllowedMask = PROT_READ | PROT_WRITE | PROT_EXEC;
  const Arg<int> prot(2);
  return If((prot & ~kAllowedMask) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictFcntlCommands() {
  // We also restrict the flags in F_SETFL. We don't want to permit flags with
  // a history of trouble such as O_DIRECT. The flags you see are actually the
  // allowed ones, and the variable is a "denied" mask because of the negation
  // operator.
  // Glibc overrides the kernel's O_LARGEFILE value. Account for this.
  uint64_t kOLargeFileFlag = O_LARGEFILE;
  if (IsArchitectureX86_64() || IsArchitectureI386() || IsArchitectureMips())
    kOLargeFileFlag = 0100000;

  const Arg<int> cmd(1);
  const Arg<long> long_arg(2);

  const uint64_t kAllowedMask = O_ACCMODE | O_APPEND | O_NONBLOCK | O_SYNC |
                                kOLargeFileFlag | O_CLOEXEC | O_NOATIME;
  return Switch(cmd)
      .CASES((F_GETFL,
              F_GETFD,
              F_SETFD,
              F_SETLK,
              F_SETLKW,
              F_GETLK,
              F_DUPFD,
              F_DUPFD_CLOEXEC),
             Allow())
      .Case(F_SETFL,
            If((long_arg & ~kAllowedMask) == 0, Allow()).Else(CrashSIGSYS()))
      .Default(CrashSIGSYS());
}

#if defined(__i386__) || defined(__mips__)
ResultExpr RestrictSocketcallCommand() {
  // Unfortunately, we are unable to restrict the first parameter to
  // socketpair(2). Whilst initially sounding bad, it's noteworthy that very
  // few protocols actually support socketpair(2). The scary call that we're
  // worried about, socket(2), remains blocked.
  const Arg<int> call(0);
  return Switch(call)
      .CASES((SYS_SOCKETPAIR,
              SYS_SHUTDOWN,
              SYS_RECV,
              SYS_SEND,
              SYS_RECVFROM,
              SYS_SENDTO,
              SYS_RECVMSG,
              SYS_SENDMSG),
             Allow())
      .Default(Error(EPERM));
}
#endif

ResultExpr RestrictKillTarget(pid_t target_pid, int sysno) {
  switch (sysno) {
    case __NR_kill:
    case __NR_tgkill: {
      const Arg<pid_t> pid(0);
      return If(pid == target_pid, Allow()).Else(CrashSIGSYSKill());
    }
    case __NR_tkill:
      return CrashSIGSYSKill();
    default:
      NOTREACHED();
      return CrashSIGSYS();
  }
}

ResultExpr RestrictFutex() {
  const uint64_t kAllowedFutexFlags = FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME;
  const Arg<int> op(1);
  return Switch(op & ~kAllowedFutexFlags)
      .CASES((FUTEX_WAIT,
              FUTEX_WAKE,
              FUTEX_REQUEUE,
              FUTEX_CMP_REQUEUE,
              FUTEX_WAKE_OP,
              FUTEX_WAIT_BITSET,
              FUTEX_WAKE_BITSET),
             Allow())
      .Default(CrashSIGSYSFutex());
}

ResultExpr RestrictGetSetpriority(pid_t target_pid) {
  const Arg<int> which(0);
  const Arg<int> who(1);
  return If(which == PRIO_PROCESS,
            If(who == 0 || who == target_pid, Allow()).Else(Error(EPERM)))
      .Else(CrashSIGSYS());
}

ResultExpr RestrictClockID() {
  COMPILE_ASSERT(4 == sizeof(clockid_t), clockid_is_not_32bit);
  const Arg<clockid_t> clockid(0);
  return If(
#if defined(OS_CHROMEOS)
             // Allow the special clock for Chrome OS used by Chrome tracing.
             clockid == base::TimeTicks::kClockSystemTrace ||
#endif
                 clockid == CLOCK_MONOTONIC ||
                 clockid == CLOCK_PROCESS_CPUTIME_ID ||
                 clockid == CLOCK_REALTIME ||
                 clockid == CLOCK_THREAD_CPUTIME_ID,
             Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictSchedTarget(pid_t target_pid, int sysno) {
  switch (sysno) {
    case __NR_sched_getaffinity:
    case __NR_sched_getattr:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_rr_get_interval:
    case __NR_sched_setaffinity:
    case __NR_sched_setattr:
    case __NR_sched_setparam:
    case __NR_sched_setscheduler: {
      const Arg<pid_t> pid(0);
      return If(pid == 0 || pid == target_pid, Allow())
          .Else(RewriteSchedSIGSYS());
    }
    default:
      NOTREACHED();
      return CrashSIGSYS();
  }
}


}  // namespace sandbox.
