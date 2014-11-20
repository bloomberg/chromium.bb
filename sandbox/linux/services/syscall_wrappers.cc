// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/syscall_wrappers.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "sandbox/linux/services/linux_syscalls.h"

namespace sandbox {

pid_t sys_getpid(void) {
  return syscall(__NR_getpid);
}

pid_t sys_gettid(void) {
  return syscall(__NR_gettid);
}

long sys_clone(unsigned long flags,
               void* child_stack,
               void* ptid,
               void* ctid,
               struct pt_regs* regs) {
  return syscall(__NR_clone, flags, child_stack, ptid, ctid, regs);
}

void sys_exit_group(int status) {
  syscall(__NR_exit_group, status);
}

}  // namespace sandbox
