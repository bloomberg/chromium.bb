// Copyright (c) 2012, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_BREAKPAD_COMMON_ANDROID_INCLUDE_SYS_USER_H
#define GOOGLE_BREAKPAD_COMMON_ANDROID_INCLUDE_SYS_USER_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// These types are used with ptrace(), more specifically with
// PTRACE_GETREGS, PTRACE_GETFPREGS and PTRACE_GETVFPREGS respectively.
//
// They are also defined, sometimes with different names, in <asm/user.h>
//

#if defined(__arm__)

#define _ARM_USER_H  1  // Prevent <asm/user.h> conflicts

// Note: on ARM, GLibc uses user_regs instead of user_regs_struct.
struct user_regs {
  // Note: Entries 0-15 match r0..r15
  //       Entry 16 is used to store the CPSR register.
  //       Entry 17 is used to store the "orig_r0" value.
  unsigned long int uregs[18];
};

// Same here: user_fpregs instead of user_fpregs_struct.
struct user_fpregs {
  struct fp_reg {
    unsigned int sign1:1;
    unsigned int unused:15;
    unsigned int sign2:1;
    unsigned int exponent:14;
    unsigned int j:1;
    unsigned int mantissa1:31;
    unsigned int mantissa0:32;
  } fpregs[8];
  unsigned int  fpsr:32;
  unsigned int  fpcr:32;
  unsigned char ftype[8];
  unsigned int  init_flag;
};

// GLibc doesn't define this one in <sys/user.h> though.
struct user_vfpregs {
  unsigned long long  fpregs[32];
  unsigned long       fpscr;
};

#elif defined(__aarch64__)

// aarch64 does not have user_regs definitions in <asm/user.h>, instead
// use the definitions in <asm/ptrace.h>, which we don't need to redefine here.

#elif defined(__i386__)

#define _I386_USER_H 1  // Prevent <asm/user.h> conflicts

// GLibc-compatible definitions
struct user_regs_struct {
  long ebx, ecx, edx, esi, edi, ebp, eax;
  long xds, xes, xfs, xgs, orig_eax;
  long eip, xcs, eflags, esp, xss;
};

struct user_fpregs_struct {
  long cwd, swd, twd, fip, fcs, foo, fos;
  long st_space[20];
};

struct user_fpxregs_struct {
  unsigned short cwd, swd, twd, fop;
  long fip, fcs, foo, fos, mxcsr, reserved;
  long st_space[32];
  long xmm_space[32];
  long padding[56];
};

struct user {
  struct user_regs_struct    regs;
  int                        u_fpvalid;
  struct user_fpregs_struct  i387;
  unsigned long              u_tsize;
  unsigned long              u_dsize;
  unsigned long              u_ssize;
  unsigned long              start_code;
  unsigned long              start_stack;
  long                       signal;
  int                        reserved;
  struct user_regs_struct*   u_ar0;
  struct user_fpregs_struct* u_fpstate;
  unsigned long              magic;
  char                       u_comm [32];
  int                        u_debugreg [8];
};


#elif defined(__mips__)

#define _ASM_USER_H 1  // Prevent <asm/user.h> conflicts

struct user_regs_struct {
  unsigned long long regs[32];
  unsigned long long lo;
  unsigned long long hi;
  unsigned long long epc;
  unsigned long long badvaddr;
  unsigned long long status;
  unsigned long long cause;
};

struct user_fpregs_struct {
  unsigned long long regs[32];
  unsigned int fpcsr;
  unsigned int fir;
};

#elif defined(__x86_64__)
#include <sys/types.h>
#include_next <sys/user.h>

// This struct is essentially the same as user_i387_struct in sys/user.h
// except that the struct name and individual field names are chosen here
// to match the ones used in breakpad for other x86_64 platforms.

struct user_fpregs_struct {
  __u16 cwd;
  __u16 swd;
  __u16 ftw;
  __u16 fop;
  __u64 rip;
  __u64 rdp;
  __u32 mxcsr;
  __u32 mxcr_mask;
  __u32 st_space[32];   /* 8*16 bytes for each FP-reg = 128 bytes */
  __u32 xmm_space[64];  /* 16*16 bytes for each XMM-reg = 256 bytes */
  __u32 padding[24];
};

#else
#  error "Unsupported Android CPU ABI"
#endif

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // GOOGLE_BREAKPAD_COMMON_ANDROID_INCLUDE_SYS_USER_H
