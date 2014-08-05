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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/android_futex.h"

#if defined(OS_ANDROID)
#if !defined(F_DUPFD_CLOEXEC)
#define F_DUPFD_CLOEXEC (F_LINUX_SPECIFIC_BASE + 6)
#endif
#endif

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

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::BoolExpr;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

// TODO(mdempsky): Make BoolExpr a standalone class so these operators can
// be resolved via argument-dependent lookup.
using sandbox::bpf_dsl::operator||;
using sandbox::bpf_dsl::operator&&;

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
  const BoolExpr android_test =
      flags == kAndroidCloneMask || flags == kObsoleteAndroidCloneMask;

  const uint64_t kGlibcPthreadFlags =
      CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
      CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
  const BoolExpr glibc_test = flags == kGlibcPthreadFlags;

  return If(IsAndroid() ? android_test : glibc_test, Allow())
      .ElseIf((flags & (CLONE_VM | CLONE_THREAD)) == 0, Error(EPERM))
      .Else(CrashSIGSYSClone());
}

ResultExpr RestrictPrctl() {
  // Will need to add seccomp compositing in the future. PR_SET_PTRACER is
  // used by breakpad but not needed anymore.
  const Arg<int> option(0);
  return If(option == PR_GET_NAME || option == PR_SET_NAME ||
                option == PR_GET_DUMPABLE || option == PR_SET_DUMPABLE,
            Allow()).Else(CrashSIGSYSPrctl());
}

ResultExpr RestrictIoctl() {
  const Arg<int> request(1);
  return If(request == TCGETS || request == FIONREAD, Allow())
      .Else(CrashSIGSYSIoctl());
}

ResultExpr RestrictMmapFlags() {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit MAP_HUGETLB, or the newer flags such as
  // MAP_POPULATE.
  // TODO(davidung), remove MAP_DENYWRITE with updated Tegra libraries.
  const uint32_t denied_mask =
      ~(MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_NORESERVE |
        MAP_FIXED | MAP_DENYWRITE);
  const Arg<int> flags(3);
  return If((flags & denied_mask) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictMprotectFlags() {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit weird undocumented flags such as
  // PROT_GROWSDOWN.
  const uint32_t denied_mask = ~(PROT_READ | PROT_WRITE | PROT_EXEC);
  const Arg<int> prot(2);
  return If((prot & denied_mask) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictFcntlCommands() {
  // We also restrict the flags in F_SETFL. We don't want to permit flags with
  // a history of trouble such as O_DIRECT. The flags you see are actually the
  // allowed ones, and the variable is a "denied" mask because of the negation
  // operator.
  // Glibc overrides the kernel's O_LARGEFILE value. Account for this.
  int kOLargeFileFlag = O_LARGEFILE;
  if (IsArchitectureX86_64() || IsArchitectureI386() || IsArchitectureMips())
    kOLargeFileFlag = 0100000;

  const Arg<int> cmd(1);
  const Arg<long> long_arg(2);

  unsigned long denied_mask = ~(O_ACCMODE | O_APPEND | O_NONBLOCK | O_SYNC |
                                kOLargeFileFlag | O_CLOEXEC | O_NOATIME);
  return If(cmd == F_GETFL || cmd == F_GETFD || cmd == F_SETFD ||
                cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK ||
                cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC ||
                (cmd == F_SETFL && (long_arg & denied_mask) == 0),
            Allow()).Else(CrashSIGSYS());
}

#if defined(__i386__) || defined(__mips__)
ResultExpr RestrictSocketcallCommand() {
  // Unfortunately, we are unable to restrict the first parameter to
  // socketpair(2). Whilst initially sounding bad, it's noteworthy that very
  // few protocols actually support socketpair(2). The scary call that we're
  // worried about, socket(2), remains blocked.
  const Arg<int> call(0);
  return If(call == SYS_SOCKETPAIR || call == SYS_SHUTDOWN ||
                call == SYS_RECV || call == SYS_SEND ||
                call == SYS_RECVFROM || call == SYS_SENDTO ||
                call == SYS_RECVMSG || call == SYS_SENDMSG,
            Allow()).Else(Error(EPERM));
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
  // In futex.c, the kernel does "int cmd = op & FUTEX_CMD_MASK;". We need to
  // make sure that the combination below will cover every way to get
  // FUTEX_CMP_REQUEUE_PI.
  const int kBannedFutexBits =
      ~(FUTEX_CMD_MASK | FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME);
  COMPILE_ASSERT(0 == kBannedFutexBits,
                 need_to_explicitly_blacklist_more_bits);

  const Arg<int> op(1);
  return If(op == FUTEX_CMP_REQUEUE_PI || op == FUTEX_CMP_REQUEUE_PI_PRIVATE ||
                op == (FUTEX_CMP_REQUEUE_PI | FUTEX_CLOCK_REALTIME) ||
                op == (FUTEX_CMP_REQUEUE_PI_PRIVATE | FUTEX_CLOCK_REALTIME),
            CrashSIGSYSFutex()).Else(Allow());
}

}  // namespace sandbox.
