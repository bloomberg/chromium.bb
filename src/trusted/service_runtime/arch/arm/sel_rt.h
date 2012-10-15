/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Secure Runtime
 */

#ifndef __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_ARM_SEL_RT_H__
#define __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_ARM_SEL_RT_H__ 1

/* This file can be #included from assembly to get the #defines. */
#if !defined(__ASSEMBLER__)

#include <stddef.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"

uint32_t NaClGetStackPtr(void);

typedef uint32_t nacl_reg_t;

#define NACL_PRIdNACL_REG NACL_PRId32
#define NACL_PRIiNACL_REG NACL_PRIi32
#define NACL_PRIoNACL_REG NACL_PRIo32
#define NACL_PRIuNACL_REG NACL_PRIu32
#define NACL_PRIxNACL_REG NACL_PRIx32
#define NACL_PRIXNACL_REG NACL_PRIX32

struct NaClThreadContext {
  /*
   * r4 through to stack_ptr correspond to NACL_CALLEE_SAVE_LIST, and
   * the assembly code expects them to appear at the start of the
   * struct.
   */
  nacl_reg_t  r4, r5, r6, r7, r8, r9, r10, fp, stack_ptr, prog_ctr;
  /*           0   4   8   c  10  14   18  1c         20        24 */
  /*
   * sys_ret and new_prog_ctr are not a part of the thread's register
   * set, but are needed by NaClSwitch.  By including them here, the
   * two use the same interface.
   */
  uint32_t  sysret;
  /*            28 */
  uint32_t  new_prog_ctr;
  /*            2c */
  uint32_t  trusted_stack_ptr;
  /*            30 */
  uint32_t  tls_idx;
  /*            34 */
  uint32_t  fpscr;
  /*            38 */
  uint32_t  sys_fpscr;
  /*            3c */
  uint32_t  tls_value1;
  /*            40 */
  uint32_t  tls_value2;
  /*            44 */
};

#endif /* !defined(__ASSEMBLER__) */

#define NACL_THREAD_CONTEXT_OFFSET_TRUSTED_STACK_PTR 0x30
#define NACL_THREAD_CONTEXT_OFFSET_FPSCR 0x38
#define NACL_THREAD_CONTEXT_OFFSET_SYS_FPSCR 0x3c

#if !defined(__ASSEMBLER__)

/*
 * This function exists as a function only because compile-time
 * assertions need to be inside a function.  This function does not
 * need to be called for the assertions to be checked.
 */
static INLINE void NaClThreadContextOffsetCheck(void) {
  NACL_COMPILE_TIME_ASSERT(NACL_THREAD_CONTEXT_OFFSET_TRUSTED_STACK_PTR
                           == offsetof(struct NaClThreadContext,
                                       trusted_stack_ptr));
  NACL_COMPILE_TIME_ASSERT(NACL_THREAD_CONTEXT_OFFSET_FPSCR
                           == offsetof(struct NaClThreadContext,
                                       fpscr));
  NACL_COMPILE_TIME_ASSERT(NACL_THREAD_CONTEXT_OFFSET_SYS_FPSCR
                           == offsetof(struct NaClThreadContext,
                                       sys_fpscr));
}

#endif /* !defined(__ASSEMBLER__) */

#endif /* __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_ARM_SEL_RT_H___ */
