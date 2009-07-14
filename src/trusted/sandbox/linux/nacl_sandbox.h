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

// Sandbox for tracing the sel_ldr and untrusted NaCl module.

#ifndef NATIVE_CLIENT_SANDBOX_LINUX_NACL_SANDBOX_H_
#define NATIVE_CLIENT_SANDBOX_LINUX_NACL_SANDBOX_H_

#include <map>

class NaclSyscallFilter;
struct user_regs_struct;

class ThreadState {
 public:
  ThreadState();
  ~ThreadState();

  // Toggle between in_syscall = true and in_syscall = false.
  bool ChangeThreadSyscallStatus(int tid,
                                 struct user_regs_struct* regs);
  // Returns true if the thread appears to be entering a syscall.
  const bool ThreadEnteringSyscall(int tid,
                                   struct user_regs_struct* regs,
                                   bool* error);
  // Attempts to add the thread, prints error if it already exists.
  void AddThread(int tid);
  // Remove thread from thread_state_.
  void RemoveThread(int tid);
  // Returns needs_setup == true.
  const bool ThreadNeedsSetup(int tid);
  // Set ptrace options on thread.
  bool SetupThread(int tid);
  // Returns true if tid is not in thread_state_.
  const bool UnknownThread(int tid);
  // Return true if there are no threads in thread_state_.
  const bool AllDone();
  // Print all threads
  const void DumpThreadState();

 private:
  ThreadState&  operator = (const ThreadState& ts);
  ThreadState(const ThreadState& ts) {  }

  struct traced_child {
    bool in_syscall;
    bool needs_setup;
    long int current_syscall; // only valid if in_syscall is true
  };
  std::map<int, struct traced_child>* thread_state_;
};

class NaclSandbox {
 public:
  NaclSandbox();
  ~NaclSandbox();

  void Run(char* application_name, char** argv);
  void ExecTracedSelLdr(char* sel_ldr_path, char** argv);

  static bool SelLdrPath(int argc, char** argv, char* path, int path_size);
  static bool ParseApplicationName(int argc, char** argv, char* app_name,
                                   int buffer_size);

  static const char* kSelLdrPathSuffix;
  static const char* kTraceSandboxEnvVariable;

 private:
  NaclSandbox&  operator = (const NaclSandbox& sb);
  NaclSandbox(const NaclSandbox& sb) {  }

  int KillChildProcess(pid_t pid,
                       struct user_regs_struct* regs_old,
                       struct user_regs_struct* regs_new);

  NaclSyscallFilter* filter_;
  ThreadState* threads_;
};

#endif  // NATIVE_CLIENT_SANDBOX_LINUX_NACL_SANDBOX_H_
