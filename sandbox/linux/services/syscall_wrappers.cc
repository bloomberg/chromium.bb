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
#include "sandbox/linux/services/linux_syscalls.h"

namespace sandbox {

pid_t sys_getpid(void) {
  return syscall(__NR_getpid);
}

pid_t sys_gettid(void) {
  return syscall(__NR_gettid);
}

namespace {

bool CloneArgumentsValid(unsigned long flags, pid_t* ptid, pid_t* ctid) {
  const bool clone_tls_used = flags & CLONE_SETTLS;
  const bool invalid_ctid =
      (flags & (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) && !ctid;
  const bool invalid_ptid = (flags & CLONE_PARENT_SETTID) && !ptid;

  // We do not support CLONE_VM.
  const bool clone_vm_used = flags & CLONE_VM;

  return !(clone_tls_used || invalid_ctid || invalid_ptid || clone_vm_used);
}

bool IsRunningOnValgrind() {
  return RUNNING_ON_VALGRIND;
}

// This function runs on the stack specified on the clone call. It uses longjmp
// to switch back to the original stack so the child can return from sys_clone.
int CloneHelper(void* arg) {
  jmp_buf* env_ptr = reinterpret_cast<jmp_buf*>(arg);
  longjmp(*env_ptr, 1);

  // Should not be reached.
  RAW_CHECK(false);
  return 1;
}

// This function is noinline to ensure that stack_buf is below the stack pointer
// that is saved when setjmp is called below. This is needed because when
// compiled with FORTIFY_SOURCE, glibc's longjmp checks that the stack is moved
// upwards. See crbug.com/442912 for more details.
NOINLINE pid_t CloneAndLongjmpInChild(unsigned long flags,
                                     pid_t* ptid,
                                     pid_t* ctid,
                                     jmp_buf* env) {
  // We use the libc clone wrapper instead of making the syscall
  // directly because making the syscall may fail to update the libc's
  // internal pid cache. The libc interface unfortunately requires
  // specifying a new stack, so we use setjmp/longjmp to emulate
  // fork-like behavior.
  char stack_buf[PTHREAD_STACK_MIN];
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY) || \
    defined(ARCH_CPU_MIPS64_FAMILY) || defined(ARCH_CPU_MIPS_FAMILY)
  // The stack grows downward.
  void* stack = stack_buf + sizeof(stack_buf);
#else
#error "Unsupported architecture"
#endif
  return clone(&CloneHelper, stack, flags, env, ptid, nullptr, ctid);
}

}  // namespace

long sys_clone(unsigned long flags,
               decltype(nullptr) child_stack,
               pid_t* ptid,
               pid_t* ctid,
               decltype(nullptr) tls) {

  if (!CloneArgumentsValid(flags, ptid, ctid)) {
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

long sys_clone(unsigned long flags) {
  return sys_clone(flags, nullptr, nullptr, nullptr, nullptr);
}

pid_t ForkWithFlags(unsigned long flags, pid_t* ptid, pid_t* ctid) {
  if (!CloneArgumentsValid(flags, ptid, ctid)) {
    RAW_LOG(FATAL, "Invalid usage of ForkWithFlags");
  }

  // Valgrind's clone implementation does not support specifiying a child_stack
  // without CLONE_VM, so we cannot use libc's clone wrapper when running under
  // Valgrind. As a result, the libc pid cache may be incorrect under Valgrind.
  // See crbug.com/442817 for more details.
  if (IsRunningOnValgrind()) {
    return sys_clone(flags, nullptr, ptid, ctid, nullptr);
  }

  jmp_buf env;
  if (setjmp(env) == 0) {
    return CloneAndLongjmpInChild(flags, ptid, ctid, &env);
  }

  return 0;
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
  return syscall(__NR_prlimit64, pid, resource, new_limit, old_limit);
}

}  // namespace sandbox
