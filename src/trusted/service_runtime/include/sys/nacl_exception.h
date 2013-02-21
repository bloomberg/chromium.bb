/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Exception Context
 */

#ifndef __NATIVE_CLIENT_SRC_SERVICE_RUNTIME_INCLUDE_SYS_NACL_EXCEPTION_H__
#define __NATIVE_CLIENT_SRC_SERVICE_RUNTIME_INCLUDE_SYS_NACL_EXCEPTION_H__ 1

#if defined(__native_client__)
# include <stdint.h>
#else
# include "native_client/src/include/portability.h"
#endif

/*
 * NaClUserRegisterState is similar to NaClSignalContext except that:
 *
 *  - It does not contain x86 segment register state that we don't
 *    want to report to untrusted code.
 *
 *  - We provide all architectures' definitions side by side so that,
 *    for example, a multiarch crash dump processor can use these to
 *    interpret register state for all architectures.
 *
 *  - NaClUserRegisterStateX8664 uses x86's normal register ordering
 *    rather than GDB's quirky register ordering.
 *
 * TODO(mseaborn): We could change NaClSignalContext to reuse
 * NaClUserRegisterState<ARCH> so that the register lists are not
 * duplicated so much.
 */

struct NaClUserRegisterStateX8632 {
  /* The first 8 are ordered by x86's register number encoding. */
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t stack_ptr;  /* esp */
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t prog_ctr;  /* eip */
  uint32_t flags;
};

struct NaClUserRegisterStateX8664 {
  /* The first 16 are ordered by x86's register number encoding. */
  uint64_t rax;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rbx;
  uint64_t stack_ptr;  /* rsp */
  uint64_t rbp;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
  uint64_t prog_ctr;  /* rip */
  uint32_t flags;
};

struct NaClUserRegisterStateARM {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t r12;
  uint32_t stack_ptr;  /* r13 */
  uint32_t lr;         /* r14 */
  uint32_t prog_ctr;   /* r15 */
  uint32_t cpsr;
};

struct NaClUserRegisterStateMIPS {
  uint32_t zero;
  uint32_t at;
  uint32_t v0;
  uint32_t v1;
  uint32_t a0;
  uint32_t a1;
  uint32_t a2;
  uint32_t a3;
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  uint32_t t7;
  uint32_t s0;
  uint32_t s1;
  uint32_t s2;
  uint32_t s3;
  uint32_t s4;
  uint32_t s5;
  uint32_t s6;
  uint32_t s7;
  uint32_t t8;
  uint32_t t9;
  uint32_t k0;
  uint32_t k1;
  uint32_t global_ptr;
  uint32_t stack_ptr;
  uint32_t frame_ptr;
  uint32_t return_addr;
  uint32_t prog_ctr;
};

/* This allows this header to be used under PNaCl. */
struct NaClUserRegisterStateUnknown {
  int dummy;  /* Empty structs are not allowed by -pedantic. */
};

#if defined(__native_client__)
# if defined(__i386__)
typedef struct NaClUserRegisterStateX8632 NaClUserRegisterState;
# elif defined(__x86_64__)
typedef struct NaClUserRegisterStateX8664 NaClUserRegisterState;
# elif defined(__arm__)
typedef struct NaClUserRegisterStateARM NaClUserRegisterState;
# elif defined(__mips__)
typedef struct NaClUserRegisterStateMIPS NaClUserRegisterState;
# else
typedef struct NaClUserRegisterStateUnknown NaClUserRegisterState;
# endif
#else
# if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
typedef struct NaClUserRegisterStateX8632 NaClUserRegisterState;
# elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
typedef struct NaClUserRegisterStateX8664 NaClUserRegisterState;
# elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
typedef struct NaClUserRegisterStateARM NaClUserRegisterState;
# elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_mips
typedef struct NaClUserRegisterStateMIPS NaClUserRegisterState;
# else
typedef struct NaClUserRegisterStateUnknown NaClUserRegisterState;
# endif
#endif

struct NaClExceptionContext {
  uint32_t prog_ctr;
  uint32_t stack_ptr;
  uint32_t frame_ptr;
  NaClUserRegisterState regs;
};

#endif /* __NATIVE_CLIENT_SRC_SERVICE_RUNTIME_INCLUDE_SYS_NACL_EXCEPTION_H__ */
