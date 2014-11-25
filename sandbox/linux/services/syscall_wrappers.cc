// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/syscall_wrappers.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "sandbox/linux/services/linux_syscalls.h"

namespace sandbox {

pid_t sys_getpid(void) {
  return syscall(__NR_getpid);
}

pid_t sys_gettid(void) {
  return syscall(__NR_gettid);
}

long sys_clone(unsigned long flags) {
  return sys_clone(flags, nullptr, nullptr, nullptr, nullptr);
}

long sys_clone(unsigned long flags,
               void* child_stack,
               pid_t* ptid,
               pid_t* ctid,
               decltype(nullptr) tls) {
  const bool clone_tls_used = flags & CLONE_SETTLS;
  const bool invalid_ctid =
      (flags & (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) && !ctid;
  const bool invalid_ptid = (flags & CLONE_PARENT_SETTID) && !ptid;
  const bool invalid_stack = (flags & CLONE_VM) && !child_stack;

  if (clone_tls_used || invalid_ctid || invalid_ptid || invalid_stack) {
    RAW_LOG(FATAL, "Invalid usage of sys_clone");
  }

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

void sys_exit_group(int status) {
  syscall(__NR_exit_group, status);
}

int sys_seccomp(unsigned int operation,
                unsigned int flags,
                const struct sock_fprog* args) {
  return syscall(__NR_seccomp, operation, flags, args);
}

}  // namespace sandbox
