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

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/third_party/valgrind/valgrind.h"
#include "build/build_config.h"
#include "sandbox/linux/system_headers/capability.h"
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

}  // namespace sandbox
