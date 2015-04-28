// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/syscall_wrappers.h"

#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/third_party/valgrind/valgrind.h"
#include "build/build_config.h"
#include "sandbox/linux/system_headers/capability.h"
#include "sandbox/linux/system_headers/linux_signal.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {

pid_t sys_getpid(void) {
  return syscall(__NR_getpid);
}

pid_t sys_gettid(void) {
  return syscall(__NR_gettid);
}

long sys_clone(unsigned long flags,
               decltype(nullptr) child_stack,
               pid_t* ptid,
               pid_t* ctid,
               decltype(nullptr) tls) {
  const bool clone_tls_used = flags & CLONE_SETTLS;
  const bool invalid_ctid =
      (flags & (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) && !ctid;
  const bool invalid_ptid = (flags & CLONE_PARENT_SETTID) && !ptid;

  // We do not support CLONE_VM.
  const bool clone_vm_used = flags & CLONE_VM;
  if (clone_tls_used || invalid_ctid || invalid_ptid || clone_vm_used) {
    RAW_LOG(FATAL, "Invalid usage of sys_clone");
  }

  if (ptid) MSAN_UNPOISON(ptid, sizeof(*ptid));
  if (ctid) MSAN_UNPOISON(ctid, sizeof(*ctid));
  // See kernel/fork.c in Linux. There is different ordering of sys_clone
  // parameters depending on CONFIG_CLONE_BACKWARDS* configuration options.
#if defined(ARCH_CPU_X86_64)
  return syscall(__NR_clone, flags, child_stack, ptid, ctid, tls);
#elif defined(ARCH_CPU_X86) || defined(ARCH_CPU_ARM_FAMILY) || \
    defined(ARCH_CPU_MIPS_FAMILY) || defined(ARCH_CPU_MIPS64_FAMILY)
  // CONFIG_CLONE_BACKWARDS defined.
  return syscall(__NR_clone, flags, child_stack, ptid, tls, ctid);
#endif
}

long sys_clone(unsigned long flags) {
  return sys_clone(flags, nullptr, nullptr, nullptr, nullptr);
}

void sys_exit_group(int status) {
  syscall(__NR_exit_group, status);
}

int sys_seccomp(unsigned int operation,
                unsigned int flags,
                const struct sock_fprog* args) {
  return syscall(__NR_seccomp, operation, flags, args);
}

int sys_prlimit64(pid_t pid,
                  int resource,
                  const struct rlimit64* new_limit,
                  struct rlimit64* old_limit) {
  int res = syscall(__NR_prlimit64, pid, resource, new_limit, old_limit);
  if (res == 0 && old_limit) MSAN_UNPOISON(old_limit, sizeof(*old_limit));
  return res;
}

int sys_capget(cap_hdr* hdrp, cap_data* datap) {
  int res = syscall(__NR_capget, hdrp, datap);
  if (res == 0) {
    if (hdrp) MSAN_UNPOISON(hdrp, sizeof(*hdrp));
    if (datap) MSAN_UNPOISON(datap, sizeof(*datap));
  }
  return res;
}

int sys_capset(cap_hdr* hdrp, const cap_data* datap) {
  return syscall(__NR_capset, hdrp, datap);
}

int sys_getresuid(uid_t* ruid, uid_t* euid, uid_t* suid) {
  int res;
#if defined(ARCH_CPU_X86) || defined(ARCH_CPU_ARMEL)
  // On 32-bit x86 or 32-bit arm, getresuid supports 16bit values only.
  // Use getresuid32 instead.
  res = syscall(__NR_getresuid32, ruid, euid, suid);
#else
  res = syscall(__NR_getresuid, ruid, euid, suid);
#endif
  if (res == 0) {
    if (ruid) MSAN_UNPOISON(ruid, sizeof(*ruid));
    if (euid) MSAN_UNPOISON(euid, sizeof(*euid));
    if (suid) MSAN_UNPOISON(suid, sizeof(*suid));
  }
  return res;
}

int sys_getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid) {
  int res;
#if defined(ARCH_CPU_X86) || defined(ARCH_CPU_ARMEL)
  // On 32-bit x86 or 32-bit arm, getresgid supports 16bit values only.
  // Use getresgid32 instead.
  res = syscall(__NR_getresgid32, rgid, egid, sgid);
#else
  res = syscall(__NR_getresgid, rgid, egid, sgid);
#endif
  if (res == 0) {
    if (rgid) MSAN_UNPOISON(rgid, sizeof(*rgid));
    if (egid) MSAN_UNPOISON(egid, sizeof(*egid));
    if (sgid) MSAN_UNPOISON(sgid, sizeof(*sgid));
  }
  return res;
}

int sys_chroot(const char* path) {
  return syscall(__NR_chroot, path);
}

int sys_unshare(int flags) {
  return syscall(__NR_unshare, flags);
}

int sys_sigprocmask(int how, const sigset_t* set, decltype(nullptr) oldset) {
  // In some toolchain (in particular Android and PNaCl toolchain),
  // sigset_t is 32 bits, but Linux ABI requires 64 bits.
  uint64_t linux_value = 0;
  std::memcpy(&linux_value, set, std::min(sizeof(sigset_t), sizeof(uint64_t)));
  return syscall(__NR_rt_sigprocmask, how, &linux_value, nullptr,
                 sizeof(linux_value));
}

#if defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    (defined(ARCH_CPU_X86_64) && !defined(__clang__))
// If MEMORY_SANITIZER or THREAD_SANITIZER is enabled, it is necessary to call
// sigaction() here, rather than the direct syscall (sys_sigaction() defined
// by ourselves).
// It is because, if MEMORY_SANITIZER or THREAD_SANITIZER is enabled, sigaction
// is wrapped, and |act->sa_handler| is injected in order to unpoisonize the
// memory passed via callback's arguments for MEMORY_SANITIZER, or handle
// signals to check thread consistency for THREAD_SANITIZER. Please see
// msan_interceptors.cc and tsan_interceptors.cc for more details.
// So, specifically, if MEMORY_SANITIZER is enabled while the direct syscall is
// used, as MEMORY_SANITIZER does not know about it, sigaction() invocation in
// other places would be broken (in more precise, returned |oldact| would have
// a broken |sa_handler| callback).
// Practically, it would break NaCl's signal handler installation.
// cf) native_client/src/trusted/service_runtime/linux/nacl_signal.c.
// As for THREAD_SANITIZER, the intercepted signal handlers are processed more
// in other libc functions' interceptors (such as for raise()), so that it
// would not work properly.
//
// Also on x86_64 architecture, we need naked function for rt_sigreturn.
// However, there is no simple way to define it with GCC. Note that the body
// of function is actually very small (only two instructions), but we need to
// define much debug information in addition, otherwise backtrace() used by
// base::StackTrace would not work so that some tests would fail.
int sys_sigaction(int signum,
                  const struct sigaction* act,
                  struct sigaction* oldact) {
  return sigaction(signum, act, oldact);
}
#else
// struct sigaction is different ABI from the Linux's.
struct KernelSigAction {
  void (*kernel_handler)(int);
  uint32_t sa_flags;
  void (*sa_restorer)(void);
  uint64_t sa_mask;
};

// On X86_64 arch, it is necessary to set sa_restorer always.
#if defined(ARCH_CPU_X86_64)
#if !defined(SA_RESTORER)
#define SA_RESTORER 0x04000000
#endif

// rt_sigreturn is a special system call that interacts with the user land
// stack. Thus, here prologue must not be created, which implies syscall()
// does not work properly, too. Note that rt_sigreturn will never return.
static __attribute__((naked)) void sys_rt_sigreturn() {
  // Just invoke rt_sigreturn system call.
  asm volatile ("syscall\n"
                :: "a"(__NR_rt_sigreturn));
}
#endif

int sys_sigaction(int signum,
                  const struct sigaction* act,
                  struct sigaction* oldact) {
  KernelSigAction kernel_act = {};
  if (act) {
    kernel_act.kernel_handler = act->sa_handler;
    std::memcpy(&kernel_act.sa_mask, &act->sa_mask,
                std::min(sizeof(kernel_act.sa_mask), sizeof(act->sa_mask)));
    kernel_act.sa_flags = act->sa_flags;

#if defined(ARCH_CPU_X86_64)
    if (!(kernel_act.sa_flags & SA_RESTORER)) {
      kernel_act.sa_flags |= SA_RESTORER;
      kernel_act.sa_restorer = sys_rt_sigreturn;
    }
#endif
  }

  KernelSigAction kernel_oldact = {};
  int result = syscall(__NR_rt_sigaction, signum, act ? &kernel_act : nullptr,
                       oldact ? &kernel_oldact : nullptr, sizeof(uint64_t));
  if (result == 0 && oldact) {
    oldact->sa_handler = kernel_oldact.kernel_handler;
    sigemptyset(&oldact->sa_mask);
    std::memcpy(&oldact->sa_mask, &kernel_oldact.sa_mask,
                std::min(sizeof(kernel_act.sa_mask), sizeof(act->sa_mask)));
    oldact->sa_flags = kernel_oldact.sa_flags;
  }
  return result;
}

#endif  // defined(MEMORY_SANITIZER)

}  // namespace sandbox
