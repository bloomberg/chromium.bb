/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Secure Runtime
 */

#ifndef __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_X86_32_SEL_RT_32_H__
#define __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_X86_32_SEL_RT_32_H__ 1

/* This file can be #included from assembly to get the #defines. */
#if !defined(__ASSEMBLER__)

#include <stddef.h>

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"

uint16_t NaClGetGlobalCs(void);

uint16_t NaClGetGlobalDs(void);

uint16_t NaClGetCs(void);
/* setting CS is done using an lcall */

uint16_t NaClGetDs(void);

void    NaClSetDs(uint16_t v);

uint16_t NaClGetEs(void);

void    NaClSetEs(uint16_t v);

uint16_t NaClGetFs(void);

void    NaClSetFs(uint16_t v);

uint16_t NaClGetGs(void);

void    NaClSetGs(uint16_t v);

uint16_t NaClGetSs(void);

/*
 * On a context switch through the syscall interface, not all
 * registers are saved.  We assume that C calling convention is used,
 * so %ecx and %edx are caller-saved and the NaCl service runtime does
 * not have to bother saving them; %eax (or %edx:%eax pair) should
 * have the return value, so its old value is also not saved.  (We
 * should, however, ensure that there is not an accidental covert
 * channel leaking information via these registers on syscall return.)
 * The eflags register is also caller saved.
 *
 * We assume that the following is packed.  This is true for gcc and
 * msvc for x86, but we will include a check that sizeof(struct
 * NaClThreadContext) == 64 bytes. (32-bit and 64-bit mode)
 */

typedef uint32_t  nacl_reg_t;  /* general purpose register type */

#define NACL_PRIdNACL_REG NACL_PRId32
#define NACL_PRIiNACL_REG NACL_PRIi32
#define NACL_PRIoNACL_REG NACL_PRIo32
#define NACL_PRIuNACL_REG NACL_PRIu32
#define NACL_PRIxNACL_REG NACL_PRIx32
#define NACL_PRIXNACL_REG NACL_PRIX32

/*
 * The layout of NaClThreadContext must be kept in sync with the
 * #defines below.
 */
struct NaClThreadContext {
  /* ecx, edx, eax, eflags not saved */
  nacl_reg_t  ebx, esi, edi, prog_ctr; /* return addr */
  /*          0    4    8    c */
  nacl_reg_t  frame_ptr;
  /*          10 */
  nacl_reg_t  stack_ptr;
  /*          14 */
  uint16_t    ss; /* stack_ptr and ss must be adjacent */
  /*          18 */
  uint16_t    fcw;
  /*          1a */
  uint16_t    sys_fcw;
  /*          1c */
  char        dummy[2];
  /*
   * gs is our TLS base in the app; on the host side it's either fs or gs.
   */
  uint16_t    ds, es, fs, gs;
  /*          20  22  24  26 */
  /*
   * spring_addr, sys_ret and new_prog_ctr are not a part of the
   * thread's register set, but are needed by NaClSwitch.  By
   * including them here, the two use the same interface.
   */
  nacl_reg_t  new_prog_ctr;
  /*          28 */
  nacl_reg_t  sysret;
  /*          2c */
  nacl_reg_t  spring_addr;
  /*          30 */
  uint16_t    cs; /* spring_addr and cs must be adjacent */
  /*          34 */
  uint16_t    padding;
  /*          36 */

  /* These two are adjacent because they are restored using 'lss'. */
  uint32_t    trusted_stack_ptr;
  /*          38 */
  uint16_t    trusted_ss;
  /*          3c */

  uint16_t    trusted_es;
  /*          3e */
  uint16_t    trusted_fs;
  /*          40 */
  uint16_t    trusted_gs;
  /*          42 */
};

#endif /* !defined(__ASSEMBLER__) */

#define NACL_THREAD_CONTEXT_OFFSET_EBX           0x00
#define NACL_THREAD_CONTEXT_OFFSET_ESI           0x04
#define NACL_THREAD_CONTEXT_OFFSET_EDI           0x08
#define NACL_THREAD_CONTEXT_OFFSET_PROG_CTR      0x0c
#define NACL_THREAD_CONTEXT_OFFSET_FRAME_PTR     0x10
#define NACL_THREAD_CONTEXT_OFFSET_STACK_PTR     0x14
#define NACL_THREAD_CONTEXT_OFFSET_SS            0x18
#define NACL_THREAD_CONTEXT_OFFSET_FCW           0x1a
#define NACL_THREAD_CONTEXT_OFFSET_SYS_FCW       0x1c
#define NACL_THREAD_CONTEXT_OFFSET_DS            0x20
#define NACL_THREAD_CONTEXT_OFFSET_ES            0x22
#define NACL_THREAD_CONTEXT_OFFSET_FS            0x24
#define NACL_THREAD_CONTEXT_OFFSET_GS            0x26
#define NACL_THREAD_CONTEXT_OFFSET_NEW_PROG_CTR  0x28
#define NACL_THREAD_CONTEXT_OFFSET_SYSRET        0x2c
#define NACL_THREAD_CONTEXT_OFFSET_SPRING_ADDR   0x30
#define NACL_THREAD_CONTEXT_OFFSET_CS            0x34
#define NACL_THREAD_CONTEXT_OFFSET_TRUSTED_STACK_PTR 0x38
#define NACL_THREAD_CONTEXT_OFFSET_TRUSTED_SS    0x3c
#define NACL_THREAD_CONTEXT_OFFSET_TRUSTED_ES    0x3e
#define NACL_THREAD_CONTEXT_OFFSET_TRUSTED_FS    0x40
#define NACL_THREAD_CONTEXT_OFFSET_TRUSTED_GS    0x42

#if !defined(__ASSEMBLER__)

static INLINE void NaClThreadContextOffsetCheck(void) {
  /*
   * We use 'offset' to check that every field of NaClThreadContext is
   * verified below.  The fields must be listed below in order.
   */
  int offset = 0;

#define NACL_CHECK_FIELD(offset_name, field) \
    NACL_COMPILE_TIME_ASSERT(offset_name == \
                             offsetof(struct NaClThreadContext, field)); \
    CHECK(offset == offset_name); \
    offset += sizeof(((struct NaClThreadContext *) NULL)->field);

  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_EBX, ebx);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_ESI, esi);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_EDI, edi);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_PROG_CTR, prog_ctr);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_FRAME_PTR, frame_ptr);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_STACK_PTR, stack_ptr);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_SS, ss);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_FCW, fcw);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_SYS_FCW, sys_fcw);
  offset += 2;  /* Padding */
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_DS, ds);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_ES, es);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_FS, fs);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_GS, gs);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_NEW_PROG_CTR, new_prog_ctr);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_SYSRET, sysret);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_SPRING_ADDR, spring_addr);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_CS, cs);
  offset += 2;  /* Padding */
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_TRUSTED_STACK_PTR,
                   trusted_stack_ptr);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_TRUSTED_SS, trusted_ss);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_TRUSTED_ES, trusted_es);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_TRUSTED_FS, trusted_fs);
  NACL_CHECK_FIELD(NACL_THREAD_CONTEXT_OFFSET_TRUSTED_GS, trusted_gs);
  CHECK(offset == sizeof(struct NaClThreadContext));

#undef NACL_CHECK_FIELD
}

#endif

#endif /* __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_X86_32_SEL_RT_32_H__ */
