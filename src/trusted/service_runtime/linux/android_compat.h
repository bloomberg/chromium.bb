/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_LINUX_NACL_ANDROID_COMPAT_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_LINUX_NACL_ANDROID_COMPAT_H_

#if NACL_ANDROID

#include <linux/auxvec.h>
#include <linux/compiler.h>
#include <stdint.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_auxv.h"

/* sys/exec_elf.h conflicts with too many internal NaCl defines. */
#ifndef ELFMAG0
# define ELFMAG0 0x7f
# define ELFMAG1 'E'
# define ELFMAG2 'L'
# define ELFMAG3 'F'
#endif

/* Bionic is currently 32-bit only. */
#define ElfW(type) Elf32_##type

struct dl_phdr_info {
  ElfW(Addr) dlpi_addr;
  const char* dlpi_name;
  const ElfW(Phdr)* dlpi_phdr;
  ElfW(Half) dlpi_phnum;
};

struct link_map {
  uintptr_t l_addr;
  char * l_name;
  uintptr_t l_ld;
  struct link_map * l_next;
  struct link_map * l_prev;
};

struct r_debug {
  int32_t r_version;
  struct link_map * r_map;
  void (*r_brk)(void);
  int32_t r_state;
  uintptr_t r_ldbase;
};

/* Few syscall-related defines. */
# define SYS_futex __NR_futex
# define FUTEX_WAIT_PRIVATE FUTEX_WAIT
# define FUTEX_WAKE_PRIVATE FUTEX_WAKE

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
#if !defined(_ASMARM_SIGCONTEXT_H)
struct sigcontext {
  uint32_t trap_no;
  uint32_t error_code;
  uint32_t oldmask;
  uint32_t arm_r0;
  uint32_t arm_r1;
  uint32_t arm_r2;
  uint32_t arm_r3;
  uint32_t arm_r4;
  uint32_t arm_r5;
  uint32_t arm_r6;
  uint32_t arm_r7;
  uint32_t arm_r8;
  uint32_t arm_r9;
  uint32_t arm_r10;
  uint32_t arm_fp;
  uint32_t arm_ip;
  uint32_t arm_sp;
  uint32_t arm_lr;
  uint32_t arm_pc;
  uint32_t arm_cpsr;
  uint32_t fault_address;
};
#endif
typedef struct sigcontext mcontext_t;
#endif

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
#if NACL_BUILD_SUBARCH == 32
/* Type for general register.  */
typedef int greg_t;

/* Number of general registers.  */
#define NGREG   19

/* Container for all general registers.  */
typedef greg_t gregset_t[NGREG];

/* Number of each register is the `gregset_t' array.  */
enum {
  REG_GS = 0,
# define REG_GS         REG_GS
  REG_FS,
# define REG_FS         REG_FS
  REG_ES,
# define REG_ES         REG_ES
  REG_DS,
# define REG_DS         REG_DS
  REG_EDI,
# define REG_EDI        REG_EDI
  REG_ESI,
# define REG_ESI        REG_ESI
  REG_EBP,
# define REG_EBP        REG_EBP
  REG_ESP,
# define REG_ESP        REG_ESP
  REG_EBX,
# define REG_EBX        REG_EBX
  REG_EDX,
# define REG_EDX        REG_EDX
  REG_ECX,
# define REG_ECX        REG_ECX
  REG_EAX,
# define REG_EAX        REG_EAX
  REG_TRAPNO,
# define REG_TRAPNO     REG_TRAPNO
  REG_ERR,
# define REG_ERR        REG_ERR
  REG_EIP,
# define REG_EIP        REG_EIP
  REG_CS,
# define REG_CS         REG_CS
  REG_EFL,
# define REG_EFL        REG_EFL
  REG_UESP,
# define REG_UESP       REG_UESP
  REG_SS
# define REG_SS REG_SS
};

/* Definitions taken from the kernel headers.  */
struct _libc_fpreg {
  uint16_t significand[4];
  uint16_t exponent;
};

struct _libc_fpstate {
  uint32_t cw;
  uint32_t sw;
  uint32_t tag;
  uint32_t ipoff;
  uint32_t cssel;
  uint32_t dataoff;
  uint32_t datasel;
  struct _libc_fpreg _st[8];
  uint32_t status;
};

/* Structure to describe FPU registers.  */
typedef struct _libc_fpstate *fpregset_t;

/* Context to describe whole processor state.  */
typedef struct {
  gregset_t gregs;
  fpregset_t fpregs;
  uint32_t oldmask;
  uint32_t cr2;
} mcontext_t;
#else
# error "Android 64-bit unsupported"
#endif

#endif

/* Userlevel context.  */
typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext *uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  sigset_t uc_sigmask;
  uint32_t uc_regspace[128] __attribute__((__aligned__(8)));
} ucontext_t;

#endif /* NACL_ANDROID */

#endif /* NATIVE_CLIENT_SERVICE_RUNTIME_LINUX_NACL_ANDROID_COMPAT_H_ */
