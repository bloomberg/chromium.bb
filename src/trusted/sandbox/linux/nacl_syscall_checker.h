/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Base class used to define policy objects for specific syscalls.

#ifndef NATIVE_CLIENT_SANDBOX_LINUX_NACL_SYSCALL_CHECKER_H_
#define NATIVE_CLIENT_SANDBOX_LINUX_NACL_SYSCALL_CHECKER_H_

#include "native_client/src/trusted/sandbox/linux/nacl_syscall_filter.h"

struct user_regs_struct; // sys/user.h
class SandboxState;


class SyscallChecker {
 public:
  SyscallChecker() {
    allowed_ = false;
  }
  virtual ~SyscallChecker();

  virtual bool RunRule(struct user_regs_struct* regs,
                       SandboxState* state) {
    return true;
  }

  const bool allowed() {
    return allowed_;
  }

  const bool once() {
    return once_;
  }

  void set_allowed(bool allowed) {
    allowed_ = allowed;
  }

  void set_once(bool once) {
    once_ = once;
  }

 private:
  bool allowed_;
  bool once_;
};

#endif  // NATIVE_CLIENT_SANDBOX_LINUX_NACL_SYSCALL_CHECKER_H_
