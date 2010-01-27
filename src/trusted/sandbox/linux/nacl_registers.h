/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
