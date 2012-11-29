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

#include "native_client/src/trusted/validator_ragel/unreviewed/decoding.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/decoder.h"

/*
 * Set of macroses used in actions defined in parse_instruction.rl to pull
 * parts of the instruction from a byte stream and store them for future use.
 */
#define GET_REX_PREFIX() instruction.prefix.rex
#define GET_VEX_PREFIX2() vex_prefix2
#define GET_VEX_PREFIX3() vex_prefix3
#define SET_VEX_PREFIX3(P) vex_prefix3 = (P)
#define SET_DATA16_PREFIX(S) instruction.prefix.data16 = (S)
#define SET_LOCK_PREFIX(S) instruction.prefix.lock = (S)
#define SET_REPZ_PREFIX(S) instruction.prefix.repz = (S)
#define SET_REPNZ_PREFIX(S) instruction.prefix.repnz = (S)
#define SET_BRANCH_TAKEN(S) instruction.prefix.branch_taken = (S)
#define SET_BRANCH_NOT_TAKEN(S) instruction.prefix.branch_not_taken = (S)
#define SET_INSTRUCTION_NAME(N) instruction.name = (N)
#define GET_OPERAND_NAME(N) instruction.operands[(N)].name
#define SET_OPERAND_NAME(N, S) instruction.operands[(N)].name = (S)
#define SET_OPERAND_TYPE(N, S) instruction.operands[(N)].type = (S)
#define SET_OPERANDS_COUNT(N) instruction.operands_count = (N)
#define SET_MODRM_BASE(N) instruction.rm.base = (N)
#define SET_MODRM_INDEX(N) instruction.rm.index = (N)
#define SET_MODRM_SCALE(S) instruction.rm.scale = (S)
#define SET_DISP_TYPE(T) instruction.rm.disp_type = (T)
#define SET_DISP_PTR(P) \
  instruction.rm.offset = DecodeDisplacementValue(instruction.rm.disp_type, (P))
#define SET_IMM_TYPE(T) imm_operand = (T)
#define SET_IMM_PTR(P) \
  instruction.imm[0] = DecodeImmediateValue(imm_operand, (P))
#define SET_IMM2_TYPE(T) imm2_operand = (T)
#define SET_IMM2_PTR(P) \
  instruction.imm[1] = DecodeImmediateValue(imm2_operand, (P))
#define SET_CPU_FEATURE(F)
#define SET_ATT_INSTRUCTION_SUFFIX(S) instruction.att_instruction_suffix = (S)
#define CLEAR_SPURIOUS_DATA16() instruction.prefix.data16_spurious = FALSE
#define SET_SPURIOUS_DATA16() instruction.prefix.data16_spurious = TRUE

/*
 * Immediate mode: size of the instruction's immediate operand.  Note that there
 * IMMNONE (which means there are no immediate operand) and IMM2 (which is
 * two-bit immediate which shares it's byte with other operands).
 */
enum ImmediateMode {
  IMMNONE,
  IMM2,
  IMM8,
  IMM16,
  IMM32,
  IMM64
};

static FORCEINLINE uint64_t DecodeDisplacementValue(
    enum DisplacementMode disp_mode, const uint8_t *disp_ptr) {
  switch(disp_mode) {
    case DISPNONE: return 0;
    case DISP8:    return SignExtend8Bit(AnyFieldValue8bit(disp_ptr));
    case DISP16:   return SignExtend16Bit(AnyFieldValue16bit(disp_ptr));
    case DISP32:   return SignExtend32Bit(AnyFieldValue32bit(disp_ptr));
    case DISP64:   return AnyFieldValue64bit(disp_ptr);
  }
  assert(FALSE);
  return 0;
}


static FORCEINLINE uint64_t DecodeImmediateValue(enum ImmediateMode imm_mode,
                                                 const uint8_t *imm_ptr) {
  switch(imm_mode) {
    case IMMNONE: return 0;
    case IMM2:    return imm_ptr[0] & 0x03;
    case IMM8:    return AnyFieldValue8bit(imm_ptr);
    case IMM16:   return AnyFieldValue16bit(imm_ptr);
    case IMM32:   return AnyFieldValue32bit(imm_ptr);
    case IMM64:   return AnyFieldValue64bit(imm_ptr);
  }
  assert(FALSE);
  return 0;
}

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_INTERNAL_H_ */
