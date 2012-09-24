/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stddef.h>
#include <stdint.h>
#include "native_client/src/trusted/platform_qualify/nacl_dep_qualify.h"
#include "native_client/src/include/nacl_macros.h"

/* Assembled equivalent of "jr ra" */
#define INST_JR_RA 0x3E00008
#define INST_NOP   0x0000000

int NaClCheckDEP() {
  /*
   * We require DEP, so forward this call to the OS-specific check routine.
   */
  return NaClAttemptToExecuteData();
}

nacl_void_thunk NaClGenerateThunk(char *buf, size_t size_in_bytes) {
  /*
   * Place a "jr ra" at the next aligned address after buf.  Instructions
   * are always little-endian, regardless of data setting. We also place opcode
   * for "nop" (which is zero) because of delay slot in Mips.
   */
  uint32_t *aligned_buf = (uint32_t *) (((uintptr_t) buf + 3) & ~3);

  if ((char*) aligned_buf + 8 > buf + size_in_bytes) return 0;

  aligned_buf[0] = (uint32_t) INST_JR_RA;
  aligned_buf[1] = (uint32_t) INST_NOP;

  /*
   * ISO C prevents a direct data->function cast, because the pointers aren't
   * guaranteed to be the same size.  For our platforms this is fine, but we
   * verify at compile time anyway before tricking the compiler:
   */
  NACL_ASSERT_SAME_SIZE(char *, nacl_void_thunk);
  return (nacl_void_thunk) (uintptr_t) aligned_buf;
}
