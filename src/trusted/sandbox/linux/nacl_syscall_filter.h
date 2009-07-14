/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
