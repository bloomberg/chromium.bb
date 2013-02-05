/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file contains common parts of x86-32 and x86-64 internals (inline
 * functions and defines).
 *
 * We only include simple schematic diagrams here.  For full description see
 * AMD/Intel manuals.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODING_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODING_H_

#include "native_client/src/trusted/validator_ragel/decoder.h"

#if NACL_WINDOWS
# define FORCEINLINE __forceinline
#else
# define FORCEINLINE __inline __attribute__ ((always_inline))
#endif


/*
 * Opcode with register number embedded:
 *
 *     7       6       5       4       3       2       1       0
 * ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┒
 * │                Opcode                 │    register number    ┃
 * ┕━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┛
 */
static FORCEINLINE uint8_t RegFromOpcode(uint8_t modrm) {
  return modrm & 0x07;
}

/*
 * ModRM byte format:
 *
 *     7       6       5       4       3       2       1       0
 * ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┒
 * │      mod      │          reg          │          r/m          ┃
 * ┕━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┛
 */
static FORCEINLINE uint8_t ModFromModRM(uint8_t modrm) {
  return modrm >> 6;
}

static FORCEINLINE uint8_t RegFromModRM(uint8_t modrm) {
  return (modrm & 0x38) >> 3;
}

static FORCEINLINE uint8_t RMFromModRM(uint8_t modrm) {
  return modrm & 0x07;
}

/*
 * SIB byte format:
 *
 *     7       6       5       4       3       2       1       0
 * ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┒
 * │     scale     │         index         │          base         ┃
 * ┕━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┛
 */
static FORCEINLINE uint8_t ScaleFromSIB(uint8_t sib) {
  return sib >> 6;
}

static FORCEINLINE uint8_t IndexFromSIB(uint8_t sib) {
  return (sib & 0x38) >> 3;
}

static FORCEINLINE uint8_t BaseFromSIB(uint8_t sib) {
  return sib & 0x07;
}

/*
 * REX byte format:
 *
 *     7       6       5       4       3       2       1       0
 * ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┒
 * │   0   │   1   │   0   │   0   │   W   │   R   │   X   │   B   ┃
 * ┕━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┛
 */

enum {
  REX_B = 1,
  REX_X = 2,
  REX_R = 4,
  REX_W = 8
};

static FORCEINLINE uint8_t BaseExtentionFromREX(uint8_t rex) {
  return (rex & REX_B) << 3;
}

static FORCEINLINE uint8_t IndexExtentionFromREX(uint8_t rex) {
  return (rex & REX_X) << 2;
}

static FORCEINLINE uint8_t RegisterExtentionFromREX(uint8_t rex) {
  return (rex & REX_R) << 1;
}

/*
 * VEX 2nd byte format:
 *
 *     7       6       5       4       3       2       1       0
 * ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┒
 * │  ¬R   │  ¬X   │  ¬B   │          opcode map selector          ┃
 * ┕━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┛
 */

enum {
  VEX_MAP1 = 0x01,
  VEX_MAP2 = 0x02,
  VEX_MAP3 = 0x03,
  VEX_MAP8 = 0x08,
  VEX_MAP9 = 0x09,
  VEX_MAPA = 0x0a,
  VEX_B    = 0x20,
  VEX_X    = 0x40,
  VEX_R    = 0x80,
  VEX_W    = 0x80
};

static FORCEINLINE uint8_t BaseExtentionFromVEX(uint8_t vex2) {
  return ((~vex2) & VEX_B) >> 2;
}

static FORCEINLINE uint8_t IndexExtentionFromVEX(uint8_t vex2) {
  return ((~vex2) & VEX_X) >> 3;
}

static FORCEINLINE uint8_t RegisterExtentionFromVEX(uint8_t vex2) {
  return ((~vex2) & VEX_R) >> 4;
}

/*
 * VEX 3rd byte format:
 *
 *     7       6       5       4       3       2       1       0
 * ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┒
 * │   W   │    ¬vvvv (register number)    │   L   │      pp       ┃
 * ┕━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┛
 */

static FORCEINLINE uint8_t GetOperandFromVexIA32(uint8_t vex3) {
  return ((~vex3) & 0x38) >> 3;
}

static FORCEINLINE uint8_t GetOperandFromVexAMD64(uint8_t vex3) {
  return ((~vex3) & 0x78) >> 3;
}

/*
 * is4 byte format:
 *
 *     7       6       5       4       3       2       1       0
 * ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┒
 * │     vvvv (register number)    │   0   │   0   │  imm2 or zero ┃
 * ┕━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┷━━━━━━━┛
 */
static FORCEINLINE uint8_t RegisterFromIS4(uint8_t is4) {
  return is4 >> 4;
}

/*
 * SignExtendXXBit is used to sign-extend XX-bit value to unsigned 64-bit value.
 *
 * To do that you need to pass unsigned value of smaller then 64-bit size
 * to this function: it will be converted to signed value and then
 * sign-extended to become 64-bit value.
 *
 * Smaller values can be obtained by restricting this value further (which is
 * safe according to the C language specification: see 6.2.1.2 in C90 and
 * 6.3.1.3.2 in C99 specification).
 *
 * Note that these operations are safe but slightly unusual: they come very
 * close to the edge of what “well-behaved C program is not supposed to do”,
 * but they stay on the “safe” side of this boundary.  Specifically: this
 * behavior triggers “implementation-defined behavior” (see 6.2.1.2 in C90
 * specification and 6.3.1.3.3 in C99 specification) which sounds suspiciously
 * similar to the dreaded “undefined behavior”, but in reality these two are
 * quite different: any program which triggers “undefined behavior” is not a
 * valid C program at all, but program which triggers “implementation-defined
 * behavior” is quite valid C program.  What this program actually *does*
 * depends on the specification of a given C compiler: each particular
 * implementation must decide for itself what it'll do in this particular case
 * and *stick* *to* *it*.  If the implementation uses two's-complement negative
 * numbers (and all the implementation which can compile this code *must*
 * support two's-complement arythmetic—see 7.18.1.1 in C99 specification) then
 * the easiest thing to do is to do what we need here—this is what all known
 * compilers for all known platforms are actually doing.
 */
static FORCEINLINE uint64_t SignExtend8Bit(uint64_t value) {
  return (int8_t)value;
}

static FORCEINLINE uint64_t SignExtend16Bit(uint64_t value) {
  return (int16_t)value;
}

static FORCEINLINE uint64_t SignExtend32Bit(uint64_t value) {
  return (int32_t)value;
}

static FORCEINLINE uint64_t AnyFieldValue8bit(const uint8_t *start) {
  return *start;
}

static FORCEINLINE uint64_t AnyFieldValue16bit(const uint8_t *start) {
  return (start[0] + 256U * start[1]);
}

static FORCEINLINE uint64_t AnyFieldValue32bit(const uint8_t *start) {
  return (start[0] + 256U * (start[1] + 256U * (start[2] + 256U * (start[3]))));
}
static FORCEINLINE uint64_t AnyFieldValue64bit(const uint8_t *start) {
  return (*start + 256ULL * (start[1] + 256ULL * (start[2] + 256ULL *
          (start[3] + 256ULL * (start[4] + 256ULL * (start[5] + 256ULL *
           (start[6] + 256ULL * start[7])))))));
}

static const uint8_t index_registers[] = {
  /* Note how REG_RIZ falls out of the pattern. */
  REG_RAX, REG_RCX, REG_RDX, REG_RBX,
  REG_RIZ, REG_RBP, REG_RSI, REG_RDI,
  REG_R8,  REG_R9,  REG_R10, REG_R11,
  REG_R12, REG_R13, REG_R14, REG_R15
};

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODING_H_ */
