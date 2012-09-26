/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file contains common parts of x86-32 and x86-64 internals (inline
 * functions and defines).
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_INTERNAL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_INTERNAL_H_

#include "native_client/src/trusted/validator_ragel/unreviewed/decoder.h"

#if NACL_WINDOWS
# define FORCEINLINE __forceinline
#else
# define FORCEINLINE __inline __attribute__ ((always_inline))
#endif

static FORCEINLINE uint8_t RegFromOpcode(uint8_t modrm) {
  return modrm & 0x07;
}

static FORCEINLINE uint8_t ModFromModRM(uint8_t modrm) {
  return modrm >> 6;
}

static FORCEINLINE uint8_t RegFromModRM(uint8_t modrm) {
  return (modrm & 0x38) >> 3;
}

static FORCEINLINE uint8_t RMFromModRM(uint8_t modrm) {
  return modrm & 0x07;
}

static FORCEINLINE uint8_t ScaleFromSIB(uint8_t sib) {
  return sib >> 6;
}

static FORCEINLINE uint8_t IndexFromSIB(uint8_t sib) {
  return (sib & 0x38) >> 3;
}

static FORCEINLINE uint8_t BaseFromSIB(uint8_t sib) {
  return sib & 0x07;
}

static FORCEINLINE uint8_t BaseExtentionFromREX(uint8_t rex) {
  return (rex & 0x01) << 3;
}

static FORCEINLINE uint8_t BaseExtentionFromVEX(uint8_t vex2) {
  return ((~vex2) & 0x20) >> 2;
}

static FORCEINLINE uint8_t IndexExtentionFromREX(uint8_t rex) {
  return (rex & 0x02) << 2;
}

static FORCEINLINE uint8_t IndexExtentionFromVEX(uint8_t vex2) {
  return ((~vex2) & 0x40) >> 3;
}

static FORCEINLINE uint8_t RegisterExtentionFromREX(uint8_t rex) {
  return (rex & 0x04) << 1;
}

static FORCEINLINE uint8_t RegisterExtentionFromVEX(uint8_t vex2) {
  return ((~vex2) & 0x80) >> 4;
}

static FORCEINLINE uint8_t GetOperandFromVexIA32(uint8_t vex3) {
  return ((~vex3) & 0x38) >> 3;
}

static FORCEINLINE uint8_t GetOperandFromVexAMD64(uint8_t vex3) {
  return ((~vex3) & 0x78) >> 3;
}

static FORCEINLINE uint8_t RegisterFromIS4(uint8_t is4) {
  return is4 >> 4;
}

static const uint8_t index_registers[] = {
  /* Note how REG_RIZ falls out of the pattern. */
  REG_RAX, REG_RCX, REG_RDX, REG_RBX,
  REG_RIZ, REG_RBP, REG_RSI, REG_RDI,
  REG_R8,  REG_R9,  REG_R10, REG_R11,
  REG_R12, REG_R13, REG_R14, REG_R15
};

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_INTERNAL_H_ */
