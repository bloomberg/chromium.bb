/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Defines the set of rules and checks on syscalls of the sandboxed
// process.  Should be extensible with virtual methods for defining
// different types of sandboxing.

#include <sys/types.h>

#ifndef NATIVE_CLIENT_SANDBOX_LINUX_NACL_SYSCALL_FILTER_H_
#define NATIVE_CLIENT_SANDBOX_LINUX_NACL_SYSCALL_FILTER_H_

#define NACL_SYSCALL_TABLE_SIZE 500

class SyscallChecker;
struct user_regs_struct;

struct SandboxState {
  bool opened_untrusted;
  bool done_exec;
  pid_t pid;
};

class NaclSyscallFilter {
 public:
  NaclSyscallFilter(const char* nacl_module_name);
  ~NaclSyscallFilter();

  static const int kMaxPathLength = 1024;

  // Return true if the regs/pid/entering satisfy the syscall (found
  // through regs).
  bool RunRule(pid_t pid, struct user_regs_struct* regs, bool entering);

  // If the code is changed to allow bad syscalls, print the list and
  // number of calls.
  void DebugPrintBadSyscalls();

 private:
  void InitSyscallRules();
  void PrintAllowedRules();
  SyscallChecker* syscall_rules_[NACL_SYSCALL_TABLE_SIZE];
  int syscall_calls_[NACL_SYSCALL_TABLE_SIZE];
  // Reset on every call to RunRule().
  struct SandboxState state_;
  const char* nacl_module_name_;
};


#endif  // NATIVE_CLIENT_SANDBOX_LINUX_NACL_SYSCALL_FILTER_H_
