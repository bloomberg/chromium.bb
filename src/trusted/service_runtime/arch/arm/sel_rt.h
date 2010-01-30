/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Secure Runtime
 */

#ifndef __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_ARM_SEL_RT_H__
#define __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_ARM_SEL_RT_H__ 1

#include "native_client/src/include/portability.h"

uint32_t NaClGetSp(void);

typedef uint32_t nacl_reg_t;

#define PRIdNACL_REG PRId32
#define PRIiNACL_REG PRIi32
#define PRIoNACL_REG PRIo32
#define PRIuNACL_REG PRIu32
#define PRIxNACL_REG PRIx32
#define PRIXNACL_REG PRIX32

struct NaClThreadContext {
  nacl_reg_t  r4, r5, r6, r7, r8, r9, r10, fp, stack_ptr, prog_ctr;
  /*           0   4   8   c  10  14   18  1c         20        24 */
  /*
   * spring_addr, sys_ret and new_eip are not a part of the thread's
   * register set, but are needed by NaClSwitch. By including them
   * here, the two use the same interface.
   */
  uint32_t  sysret;
  /*            28 */
  uint32_t  new_eip;
  /*            2c */
  uint32_t  spring_addr;
  /*            30 */
};

#endif /* __NATIVE_CLIENT_SERVICE_RUNTIME_ARCH_ARM_SEL_RT_H___ */
