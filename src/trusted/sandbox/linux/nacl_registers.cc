/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */



#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <string.h>


#include "native_client/src/trusted/sandbox/linux/nacl_registers.h"

uintptr_t Registers::GetCS(const user_regs_struct& regs) {
  return regs.xcs;
}

uintptr_t Registers::GetSyscall(const user_regs_struct& regs) {
  return regs.orig_eax;
}

uintptr_t Registers::GetReg1(const user_regs_struct& regs) {
#ifdef __i386__
  return regs.ebx;
#else
  return regs.rdi;
#endif
}

uintptr_t Registers::GetReg2(const user_regs_struct& regs) {
#ifdef __i386__
  return regs.ecx;
#else
  return regs.rsi;
#endif
}

uintptr_t Registers::GetReg3(const user_regs_struct& regs) {
#ifdef __i386__
  return regs.edx;
#else
  return regs.rdx;
#endif
}

uintptr_t Registers::GetReg4(const user_regs_struct& regs) {
#ifdef __i386__
  return regs.esi;
#else
  return regs.r10;
#endif
}

uintptr_t Registers::GetReg5(const user_regs_struct& regs) {
#ifdef __i386__
  return regs.edi;
#else
  return regs.r8;
#endif
}

uintptr_t Registers::GetReg6(const user_regs_struct& regs) {
#ifdef __i386__
  return regs.ebp;
#else
  return regs.r9;
#endif
}


bool LoadValue(pid_t pid, uintptr_t addr, long* word) {
  errno = 0;
  *word = ptrace(PTRACE_PEEKDATA, pid, (void*)(addr), 0);
  if ((int)(*word) == -1 && errno != 0) {
    return false;
  }
  return true;
}

// Not threadsafe.  Used only before module is multi-threaded with
// untrusted code.
bool Registers::GetStringVal(uintptr_t reg_val,
                             size_t max_length,
                             pid_t pid,
                             char* result) {
  long word;
  int long_len = sizeof(long);
  char word_buf[sizeof(long)+1];
  word_buf[long_len] = '\0';

  if (max_length < 1) return false;

  // 32-bit (64-bit)  word alignment.
  uintptr_t aligned_addr = reg_val & ~(sizeof(long) - 1);
  uintptr_t start_idx = reg_val - aligned_addr;
  size_t idx = 0;

  while (1) {
    if (!LoadValue(pid, aligned_addr, &word)) {
      return false;
    }
    memcpy(word_buf, &word, long_len);

    for (int i = start_idx; i < long_len; i++) {
      result[idx] = word_buf[i];
      if (word_buf[i] == '\0') {
        return true;
      }
      if (idx >= max_length-1) {
        result[idx] = '\0';
        return false;
      }
      idx++;
    }
    start_idx = 0;  // now reading from start of wordsize
    // Check for overflow.  The process being traced must be 32bit,
    // and this only works on x86 architectures.
    if (aligned_addr > UINT32_MAX - long_len) {
      return false;
    }
    aligned_addr += long_len;
  }
  return false;
}



void Registers::SetReg1(struct user_regs_struct* regs, uintptr_t val) {
#ifdef __i386__
  regs->ebx = val;
#else
  regs->rdi = val;
#endif
}

void Registers::SetReg2(struct user_regs_struct* regs, uintptr_t val) {
#ifdef __i386__
  regs->ecx = val;
#else
  regs->rsi = val;
#endif
}

void Registers::SetReg3(struct user_regs_struct* regs, uintptr_t val) {
#ifdef __i386__
  regs->edx = val;
#else
  regs->rdx = val;
#endif
}

void Registers::SetReg4(struct user_regs_struct* regs, uintptr_t val) {
#ifdef __i386__
  regs->esi = val;
#else
  regs->rcx = val;
#endif
}

void Registers::SetReg5(struct user_regs_struct* regs, uintptr_t val) {
#ifdef __i386__
  regs->edi = val;
#else
  regs->r8 = val;
#endif
}

void Registers::SetReg6(struct user_regs_struct* regs, uintptr_t val) {
#ifdef __i386__
  regs->ebp = val;
#else
  regs->r9 = val;
#endif
}
