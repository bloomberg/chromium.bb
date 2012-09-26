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
typedef struct sigcontext mcontext_t;

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
