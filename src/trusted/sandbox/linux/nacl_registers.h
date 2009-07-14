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

// Control access to registers.  Provides an abstraction for i386
// vs. x86_64.

#ifndef NATIVE_CLIENT_SANDBOX_LINUX_NACL_REGISTERS_H_
#define NATIVE_CLIENT_SANDBOX_LINUX_NACL_REGISTERS_H_

#include <cstddef>
#include <sys/user.h>
#include <stdint.h>

class Registers {
 public:
  // Gets the CS register.
  static uintptr_t GetCS(const user_regs_struct& regs);
  // Gets the syscall userspace is trying to execute.
  static uintptr_t GetSyscall(const user_regs_struct& regs);
  // Gets the first syscall argument register.
  static uintptr_t GetReg1(const user_regs_struct& regs);
  // Gets the second syscall argument register.
  static uintptr_t GetReg2(const user_regs_struct& regs);
  // Gets the third syscall argument register.
  static uintptr_t GetReg3(const user_regs_struct& regs);
  // Gets the fourth syscall argument register.
  static uintptr_t GetReg4(const user_regs_struct& regs);
  // Gets the fifth syscall argument register.
  static uintptr_t GetReg5(const user_regs_struct& regs);
  // Gets the sixth syscall argument register.
  static uintptr_t GetReg6(const user_regs_struct& regs);
  // Gets the value pointed to as a string
  static bool GetStringVal(uintptr_t reg_val, size_t max_length,
                           pid_t pid, char* result);

  // Sets the first syscall argument register.
  static void SetReg1(struct user_regs_struct* regs, uintptr_t val);
  // Sets the second syscall argument register.
  static void SetReg2(struct user_regs_struct* regs, uintptr_t val);
  // Sets the third syscall argument register.
  static void SetReg3(struct user_regs_struct* regs, uintptr_t val);
  // Sets the fourth syscall argument register.
  static void SetReg4(struct user_regs_struct* regs, uintptr_t val);
  // Sets the fifth syscall argument register.
  static void SetReg5(struct user_regs_struct* regs, uintptr_t val);
  // Sets the sixth syscall argument register.
  static void SetReg6(struct user_regs_struct* regs, uintptr_t val);
};

#endif  // NATIVE_CLIENT_SANDBOX_LINUX_NACL_REGISTERS_H_
