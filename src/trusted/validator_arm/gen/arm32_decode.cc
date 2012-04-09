/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * DO NOT EDIT: GENERATED CODE
 */

#include "native_client/src/trusted/validator_arm/gen/arm32_decode.h"

#include <stdio.h>

namespace nacl_arm_dec {

/*
 * Prototypes for static table-matching functions.
 */
static inline const ClassDecoder &decode_ARMv7(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_dp_misc(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_dp_reg(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_dp_reg_shifted(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_dp_immed(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_mult(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_sat_add_sub(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_half_mult(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_extra_load_store(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_sync(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_msr_and_hints(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_misc(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_load_store_word_byte(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_media(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_parallel_add_sub(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_pack_sat_rev(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_signed_mult(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_branch_block_xfer(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_super_cop(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_unconditional(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_misc_hints_simd(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_dp(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_dp_3same(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_dp_3diff(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_dp_2scalar(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_dp_2shift(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_dp_2misc(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_dp_1imm(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_load_store(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_load_store_l0(
    const Instruction insn, const Arm32DecoderState *state);

static inline const ClassDecoder &decode_simd_load_store_l1(
    const Instruction insn, const Arm32DecoderState *state);

/*
 * Table-matching function implementations.
 */

/*
 * Implementation of table ARMv7.
 * Specified by: See Section A5.1.
 */
static inline const ClassDecoder &decode_ARMv7(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0E000000) == 0x04000000) && (true)) {
    return decode_load_store_word_byte(insn, state);
  }
  
  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0E000000) == 0x06000000) && ((insn & 0x00000010) == 0x00000000)) {
    return decode_load_store_word_byte(insn, state);
  }
  
  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0E000000) == 0x06000000) && ((insn & 0x00000010) == 0x00000010)) {
    return decode_media(insn, state);
  }
  
  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0C000000) == 0x00000000) && (true)) {
    return decode_dp_misc(insn, state);
  }
  
  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0C000000) == 0x08000000) && (true)) {
    return decode_branch_block_xfer(insn, state);
  }
  
  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0C000000) == 0x0C000000) && (true)) {
    return decode_super_cop(insn, state);
  }
  
  if (((insn & 0xF0000000) == 0xF0000000) && (true) && (true)) {
    return decode_unconditional(insn, state);
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: ARMv7 could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table dp_misc.
 * Specified by: See Section A5.2.
 */
static inline const ClassDecoder &decode_dp_misc(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01900000) != 0x01000000) && ((insn & 0x00000090) == 0x00000010)) {
    return decode_dp_reg_shifted(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01900000) != 0x01000000) && ((insn & 0x00000010) == 0x00000000)) {
    return decode_dp_reg(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01900000) == 0x01000000) && ((insn & 0x00000090) == 0x00000080)) {
    return decode_half_mult(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01900000) == 0x01000000) && ((insn & 0x00000080) == 0x00000000)) {
    return decode_misc(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) != 0x00200000) && ((insn & 0x000000F0) == 0x000000B0)) {
    return decode_extra_load_store(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) != 0x00200000) && ((insn & 0x000000D0) == 0x000000D0)) {
    return decode_extra_load_store(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) == 0x00200000) && ((insn & 0x000000F0) == 0x000000B0)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) == 0x00200000) && ((insn & 0x000000D0) == 0x000000D0)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01000000) == 0x00000000) && ((insn & 0x000000F0) == 0x00000090)) {
    return decode_mult(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01000000) == 0x01000000) && ((insn & 0x000000F0) == 0x00000090)) {
    return decode_sync(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01B00000) == 0x01000000) && (true)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01B00000) == 0x01200000) && (true)) {
    return decode_msr_and_hints(insn, state);
  }
  
  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01900000) != 0x01000000) && (true)) {
    return decode_dp_immed(insn, state);
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_misc could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table dp_reg.
 * Specified by: See Section A5.2.1.
 */
static inline const ClassDecoder &decode_dp_reg(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x01900000) == 0x01100000)) {
    return state->Test_instance_;
  }
  
  if (((insn & 0x01800000) == 0x01800000)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x01000000) == 0x00000000)) {
    return state->DataProc_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_reg could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table dp_reg_shifted.
 * Specified by: See Section A5.2.2.
 */
static inline const ClassDecoder &decode_dp_reg_shifted(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x01E00000) == 0x01A00000)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x01E00000) == 0x01E00000)) {
    return state->Unary3RegisterShiftedOp_instance_;
  }
  
  if (((insn & 0x01900000) == 0x01100000)) {
    return state->Binary3RegisterShiftedTest_instance_;
  }
  
  if (((insn & 0x01A00000) == 0x01800000)) {
    return state->Binary4RegisterShiftedOp_instance_;
  }
  
  if (((insn & 0x01000000) == 0x00000000)) {
    return state->Binary4RegisterShiftedOp_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_reg_shifted could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table dp_immed.
 * Specified by: See Section A5.2.3.
 */
static inline const ClassDecoder &decode_dp_immed(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x01F00000) == 0x01100000)) {
    return state->TestImmediate_instance_;
  }
  
  if (((insn & 0x01F00000) == 0x01500000)) {
    return state->Test_instance_;
  }
  
  if (((insn & 0x01B00000) == 0x01300000)) {
    return state->Test_instance_;
  }
  
  if (((insn & 0x01E00000) == 0x01C00000)) {
    return state->ImmediateBic_instance_;
  }
  
  if (((insn & 0x01E00000) == 0x01E00000)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x01C00000) == 0x00000000)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x00C00000) == 0x00800000)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x01400000) == 0x00400000)) {
    return state->DataProc_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_immed could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table mult.
 * Specified by: See Section A5.2.5.
 */
static inline const ClassDecoder &decode_mult(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00F00000) == 0x00400000)) {
    return state->LongMultiply_instance_;
  }
  
  if (((insn & 0x00F00000) == 0x00600000)) {
    return state->Multiply_instance_;
  }
  
  if (((insn & 0x00D00000) == 0x00500000)) {
    return state->Undefined_instance_;
  }
  
  if (((insn & 0x00C00000) == 0x00000000)) {
    return state->Multiply_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00800000)) {
    return state->LongMultiply_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: mult could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table sat_add_sub.
 * Specified by: See Section A5.2.6.
 */
static inline const ClassDecoder &decode_sat_add_sub(
    const Instruction insn, const Arm32DecoderState *state) {
  if ((true)) {
    return state->SatAddSub_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: sat_add_sub could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table half_mult.
 * Specified by: See Section A5.2.7.
 */
static inline const ClassDecoder &decode_half_mult(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00600000) == 0x00400000)) {
    return state->LongMultiply_instance_;
  }
  
  if (((insn & 0x00600000) == 0x00600000)) {
    return state->Multiply_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000)) {
    return state->Multiply_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: half_mult could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table extra_load_store.
 * Specified by: See Section A5.2.8.
 */
static inline const ClassDecoder &decode_extra_load_store(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00000060) == 0x00000040) && ((insn & 0x00500000) == 0x00000000)) {
    return state->LoadDoubleR_instance_;
  }
  
  if (((insn & 0x00000060) == 0x00000040) && ((insn & 0x00500000) == 0x00100000)) {
    return state->LoadRegister_instance_;
  }
  
  if (((insn & 0x00000060) == 0x00000040) && ((insn & 0x00500000) == 0x00400000)) {
    return state->LoadDoubleI_instance_;
  }
  
  if (((insn & 0x00000060) == 0x00000040) && ((insn & 0x00500000) == 0x00500000)) {
    return state->LoadImmediate_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00500000) == 0x00000000)) {
    return state->StoreRegister_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00500000) == 0x00100000)) {
    return state->LoadRegister_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00500000) == 0x00400000)) {
    return state->StoreImmediate_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00500000) == 0x00500000)) {
    return state->LoadImmediate_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: extra_load_store could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table sync.
 * Specified by: See Section A5.2.10.
 */
static inline const ClassDecoder &decode_sync(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00F00000) == 0x00800000)) {
    return state->StoreExclusive_instance_;
  }
  
  if (((insn & 0x00F00000) == 0x00900000)) {
    return state->LoadExclusive_instance_;
  }
  
  if (((insn & 0x00F00000) == 0x00B00000)) {
    return state->LoadDoubleExclusive_instance_;
  }
  
  if (((insn & 0x00F00000) == 0x00C00000)) {
    return state->StoreExclusive_instance_;
  }
  
  if (((insn & 0x00B00000) == 0x00000000)) {
    return state->Deprecated_instance_;
  }
  
  if (((insn & 0x00B00000) == 0x00A00000)) {
    return state->StoreExclusive_instance_;
  }
  
  if (((insn & 0x00D00000) == 0x00D00000)) {
    return state->LoadExclusive_instance_;
  }
  
  if ((true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: sync could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table msr_and_hints.
 * Specified by: See Section A5.2.11.
 */
static inline const ClassDecoder &decode_msr_and_hints(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000FF) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000FF) == 0x00000002)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000FF) == 0x00000004)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000FD) == 0x00000001)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000F0) == 0x000000F0)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00040000) && (true)) {
    return state->MoveToStatusRegister_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000B0000) == 0x00080000) && (true)) {
    return state->MoveToStatusRegister_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x00030000) == 0x00010000) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x00020000) == 0x00020000) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x00400000) == 0x00400000) && (true) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if ((true) && (true) && (true)) {
    return state->Forbidden_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: msr_and_hints could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table misc.
 * Specified by: See Section A5.2.12.
 */
static inline const ClassDecoder &decode_misc(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00600000) == 0x00200000) && ((insn & 0x00030000) == 0x00000000)) {
    return state->MoveToStatusRegister_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00600000) == 0x00200000) && ((insn & 0x00030000) == 0x00010000)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00600000) == 0x00200000) && ((insn & 0x00020000) == 0x00020000)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00600000) == 0x00600000) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00200000) == 0x00000000) && (true)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000010) && ((insn & 0x00600000) == 0x00200000) && (true)) {
    return state->BxBlx_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000010) && ((insn & 0x00600000) == 0x00600000) && (true)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000020) && ((insn & 0x00600000) == 0x00200000) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000030) && ((insn & 0x00600000) == 0x00200000) && (true)) {
    return state->BxBlx_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000050) && (true) && (true)) {
    return decode_sat_add_sub(insn, state);
  }
  
  if (((insn & 0x00000070) == 0x00000070) && ((insn & 0x00600000) == 0x00200000) && (true)) {
    return state->Breakpoint_instance_;
  }
  
  if (((insn & 0x00000070) == 0x00000070) && ((insn & 0x00600000) == 0x00600000) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if ((true) && (true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: misc could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table load_store_word_byte.
 * Specified by: See Section A5.3.
 */
static inline const ClassDecoder &decode_load_store_word_byte(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) == 0x00200000) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x00100000) == 0x00000000) && (true)) {
    return state->StoreImmediate_instance_;
  }
  
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x00100000) == 0x00100000) && (true)) {
    return state->LoadImmediate_instance_;
  }
  
  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01200000) == 0x00200000) && ((insn & 0x00000010) == 0x00000000)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x00100000) == 0x00000000) && ((insn & 0x00000010) == 0x00000000)) {
    return state->StoreRegister_instance_;
  }
  
  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x00100000) == 0x00100000) && ((insn & 0x00000010) == 0x00000000)) {
    return state->LoadRegister_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: load_store_word_byte could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table media.
 * Specified by: See Section A5.4.
 */
static inline const ClassDecoder &decode_media(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x01F00000) == 0x01800000) && ((insn & 0x000000E0) == 0x00000000)) {
    return state->Multiply_instance_;
  }
  
  if (((insn & 0x01F00000) == 0x01F00000) && ((insn & 0x000000E0) == 0x000000E0)) {
    return state->Roadblock_instance_;
  }
  
  if (((insn & 0x01E00000) == 0x01C00000) && ((insn & 0x00000060) == 0x00000000)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x01A00000) == 0x01A00000) && ((insn & 0x00000060) == 0x00000040)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x01800000) == 0x00000000) && (true)) {
    return decode_parallel_add_sub(insn, state);
  }
  
  if (((insn & 0x01800000) == 0x00800000) && (true)) {
    return decode_pack_sat_rev(insn, state);
  }
  
  if (((insn & 0x01800000) == 0x01000000) && (true)) {
    return decode_signed_mult(insn, state);
  }
  
  if ((true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: media could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table parallel_add_sub.
 * Specified by: See Sections A5.4.1, A5.4.2.
 */
static inline const ClassDecoder &decode_parallel_add_sub(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00300000) == 0x00200000) && ((insn & 0x000000E0) == 0x00000080)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x00300000) == 0x00200000) && ((insn & 0x000000E0) == 0x000000E0)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x00300000) == 0x00200000) && ((insn & 0x00000080) == 0x00000000)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x00100000) == 0x00100000) && ((insn & 0x000000E0) == 0x00000080)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x00100000) == 0x00100000) && ((insn & 0x000000E0) == 0x000000E0)) {
    return state->DataProc_instance_;
  }
  
  if (((insn & 0x00100000) == 0x00100000) && ((insn & 0x00000080) == 0x00000000)) {
    return state->DataProc_instance_;
  }
  
  if ((true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: parallel_add_sub could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table pack_sat_rev.
 * Specified by: See Section A5.4.3.
 */
static inline const ClassDecoder &decode_pack_sat_rev(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00700000) == 0x00000000) && ((insn & 0x000000E0) == 0x000000A0)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00700000) == 0x00000000) && ((insn & 0x00000020) == 0x00000000)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00700000) == 0x00400000) && ((insn & 0x000000E0) == 0x00000060)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00700000) == 0x00600000) && ((insn & 0x000000A0) == 0x00000020)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00700000) == 0x00700000) && ((insn & 0x000000E0) == 0x00000020)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00300000) == 0x00300000) && ((insn & 0x000000E0) == 0x00000060)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00300000) == 0x00300000) && ((insn & 0x000000E0) == 0x000000A0)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00500000) == 0x00000000) && ((insn & 0x000000E0) == 0x00000060)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00600000) == 0x00200000) && ((insn & 0x000000E0) == 0x00000020)) {
    return state->PackSatRev_instance_;
  }
  
  if (((insn & 0x00200000) == 0x00200000) && ((insn & 0x00000020) == 0x00000000)) {
    return state->PackSatRev_instance_;
  }
  
  if ((true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: pack_sat_rev could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table signed_mult.
 * Specified by: See Section A5.4.4.
 */
static inline const ClassDecoder &decode_signed_mult(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00700000) == 0x00000000) && ((insn & 0x000000C0) == 0x00000040) && ((insn & 0x0000F000) != 0x0000F000)) {
    return state->Multiply_instance_;
  }
  
  if (((insn & 0x00700000) == 0x00000000) && ((insn & 0x00000080) == 0x00000000) && (true)) {
    return state->Multiply_instance_;
  }
  
  if (((insn & 0x00700000) == 0x00400000) && ((insn & 0x00000080) == 0x00000000) && (true)) {
    return state->LongMultiply_instance_;
  }
  
  if (((insn & 0x00700000) == 0x00500000) && ((insn & 0x000000C0) == 0x00000000) && (true)) {
    return state->Multiply_instance_;
  }
  
  if (((insn & 0x00700000) == 0x00500000) && ((insn & 0x000000C0) == 0x000000C0) && (true)) {
    return state->Multiply_instance_;
  }
  
  if ((true) && (true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: signed_mult could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table branch_block_xfer.
 * Specified by: See Section A5.5.
 */
static inline const ClassDecoder &decode_branch_block_xfer(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x02500000) == 0x00000000)) {
    return state->StoreImmediate_instance_;
  }
  
  if (((insn & 0x02500000) == 0x00100000)) {
    return state->LoadMultiple_instance_;
  }
  
  if (((insn & 0x02400000) == 0x00400000)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x02000000) == 0x02000000)) {
    return state->Branch_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: branch_block_xfer could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table super_cop.
 * Specified by: See Section A5.6.
 */
static inline const ClassDecoder &decode_super_cop(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x03F00000) == 0x00400000) && (true)) {
    return state->CoprocessorOp_instance_;
  }
  
  if (((insn & 0x03F00000) == 0x00500000) && (true)) {
    return state->MoveDoubleFromCoprocessor_instance_;
  }
  
  if (((insn & 0x03E00000) == 0x00000000) && (true)) {
    return state->Undefined_instance_;
  }
  
  if (((insn & 0x03100000) == 0x02000000) && ((insn & 0x00000010) == 0x00000010)) {
    return state->CoprocessorOp_instance_;
  }
  
  if (((insn & 0x03100000) == 0x02100000) && ((insn & 0x00000010) == 0x00000010)) {
    return state->MoveFromCoprocessor_instance_;
  }
  
  if (((insn & 0x02100000) == 0x00000000) && (true)) {
    return state->StoreCoprocessor_instance_;
  }
  
  if (((insn & 0x02100000) == 0x00100000) && (true)) {
    return state->LoadCoprocessor_instance_;
  }
  
  if (((insn & 0x03000000) == 0x02000000) && ((insn & 0x00000010) == 0x00000000)) {
    return state->CoprocessorOp_instance_;
  }
  
  if (((insn & 0x03000000) == 0x03000000) && (true)) {
    return state->Forbidden_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: super_cop could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table unconditional.
 * Specified by: See Section A5.7.
 */
static inline const ClassDecoder &decode_unconditional(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x0FF00000) == 0x0C400000) && (true) && (true)) {
    return state->CoprocessorOp_instance_;
  }
  
  if (((insn & 0x0FF00000) == 0x0C500000) && (true) && (true)) {
    return state->MoveDoubleFromCoprocessor_instance_;
  }
  
  if (((insn & 0x0FB00000) == 0x0C200000) && (true) && (true)) {
    return state->StoreCoprocessor_instance_;
  }
  
  if (((insn & 0x0FB00000) == 0x0C300000) && (true) && ((insn & 0x000F0000) != 0x000F0000)) {
    return state->LoadCoprocessor_instance_;
  }
  
  if (((insn & 0x0F900000) == 0x0C800000) && (true) && (true)) {
    return state->StoreCoprocessor_instance_;
  }
  
  if (((insn & 0x0F900000) == 0x0C900000) && (true) && ((insn & 0x000F0000) == 0x000F0000)) {
    return state->LoadCoprocessor_instance_;
  }
  
  if (((insn & 0x0E500000) == 0x08100000) && (true) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x0E500000) == 0x08400000) && (true) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x0F100000) == 0x0D000000) && (true) && (true)) {
    return state->StoreCoprocessor_instance_;
  }
  
  if (((insn & 0x0F100000) == 0x0D100000) && (true) && ((insn & 0x000F0000) == 0x000F0000)) {
    return state->LoadCoprocessor_instance_;
  }
  
  if (((insn & 0x0F100000) == 0x0E000000) && ((insn & 0x00000010) == 0x00000010) && (true)) {
    return state->CoprocessorOp_instance_;
  }
  
  if (((insn & 0x0F100000) == 0x0E100000) && ((insn & 0x00000010) == 0x00000010) && (true)) {
    return state->MoveFromCoprocessor_instance_;
  }
  
  if (((insn & 0x0F000000) == 0x0E000000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
    return state->CoprocessorOp_instance_;
  }
  
  if (((insn & 0x0E000000) == 0x0A000000) && (true) && (true)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x08000000) == 0x00000000) && (true) && (true)) {
    return decode_misc_hints_simd(insn, state);
  }
  
  if ((true) && (true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: unconditional could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table misc_hints_simd.
 * Specified by: See Section A5.7.1.
 */
static inline const ClassDecoder &decode_misc_hints_simd(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x07F00000) == 0x01000000) && ((insn & 0x000000F0) == 0x00000000) && ((insn & 0x00010000) == 0x00010000)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x07F00000) == 0x01000000) && ((insn & 0x00000020) == 0x00000000) && ((insn & 0x00010000) == 0x00000000)) {
    return state->Forbidden_instance_;
  }
  
  if (((insn & 0x07F00000) == 0x05700000) && ((insn & 0x000000F0) == 0x00000010) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x07F00000) == 0x05700000) && ((insn & 0x000000F0) == 0x00000050) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x07F00000) == 0x05700000) && ((insn & 0x000000D0) == 0x00000040) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x07700000) == 0x04100000) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x07700000) == 0x04500000) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x07700000) == 0x05100000) && (true) && ((insn & 0x000F0000) != 0x000F0000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x07700000) == 0x05100000) && (true) && ((insn & 0x000F0000) == 0x000F0000)) {
    return state->Unpredictable_instance_;
  }
  
  if (((insn & 0x07700000) == 0x05500000) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x07700000) == 0x06500000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x07700000) == 0x07500000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x06700000) == 0x06100000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x06300000) == 0x04300000) && (true) && (true)) {
    return state->Unpredictable_instance_;
  }
  
  if (((insn & 0x06300000) == 0x06300000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
    return state->Unpredictable_instance_;
  }
  
  if (((insn & 0x07100000) == 0x04000000) && (true) && (true)) {
    return decode_simd_load_store(insn, state);
  }
  
  if (((insn & 0x06000000) == 0x02000000) && (true) && (true)) {
    return decode_simd_dp(insn, state);
  }
  
  if ((true) && (true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: misc_hints_simd could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_dp.
 * Specified by: See Section A7.4.
 */
static inline const ClassDecoder &decode_simd_dp(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x01000000) == 0x00000000) && ((insn & 0x00B00000) == 0x00B00000) && (true) && ((insn & 0x00000010) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x01000000) == 0x01000000) && ((insn & 0x00B00000) == 0x00B00000) && ((insn & 0x00000F00) == 0x00000C00) && ((insn & 0x00000090) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x01000000) == 0x01000000) && ((insn & 0x00B00000) == 0x00B00000) && ((insn & 0x00000C00) == 0x00000800) && ((insn & 0x00000010) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x01000000) == 0x01000000) && ((insn & 0x00B00000) == 0x00B00000) && ((insn & 0x00000800) == 0x00000000) && ((insn & 0x00000010) == 0x00000000)) {
    return decode_simd_dp_2misc(insn, state);
  }
  
  if ((true) && ((insn & 0x00B80000) == 0x00800000) && (true) && ((insn & 0x00000090) == 0x00000010)) {
    return decode_simd_dp_1imm(insn, state);
  }
  
  if ((true) && ((insn & 0x00B80000) == 0x00880000) && (true) && ((insn & 0x00000090) == 0x00000010)) {
    return decode_simd_dp_2shift(insn, state);
  }
  
  if ((true) && ((insn & 0x00B00000) == 0x00900000) && (true) && ((insn & 0x00000090) == 0x00000010)) {
    return decode_simd_dp_2shift(insn, state);
  }
  
  if ((true) && ((insn & 0x00B00000) == 0x00A00000) && (true) && ((insn & 0x00000050) == 0x00000000)) {
    return decode_simd_dp_3diff(insn, state);
  }
  
  if ((true) && ((insn & 0x00B00000) == 0x00A00000) && (true) && ((insn & 0x00000050) == 0x00000040)) {
    return decode_simd_dp_2scalar(insn, state);
  }
  
  if ((true) && ((insn & 0x00A00000) == 0x00800000) && (true) && ((insn & 0x00000050) == 0x00000000)) {
    return decode_simd_dp_3diff(insn, state);
  }
  
  if ((true) && ((insn & 0x00A00000) == 0x00800000) && (true) && ((insn & 0x00000050) == 0x00000040)) {
    return decode_simd_dp_2scalar(insn, state);
  }
  
  if ((true) && ((insn & 0x00A00000) == 0x00A00000) && (true) && ((insn & 0x00000090) == 0x00000010)) {
    return decode_simd_dp_2shift(insn, state);
  }
  
  if ((true) && ((insn & 0x00800000) == 0x00000000) && (true) && (true)) {
    return decode_simd_dp_3same(insn, state);
  }
  
  if ((true) && ((insn & 0x00800000) == 0x00800000) && (true) && ((insn & 0x00000090) == 0x00000090)) {
    return decode_simd_dp_2shift(insn, state);
  }
  
  if ((true) && (true) && (true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_dp_3same.
 * Specified by: See Section A7.4.1.
 */
static inline const ClassDecoder &decode_simd_dp_3same(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00000F00) == 0x00000100) && ((insn & 0x00000010) == 0x00000010) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000500) && (true) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000900) && (true) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000B00) && ((insn & 0x00000010) == 0x00000000) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000D00) && ((insn & 0x00000010) == 0x00000000) && ((insn & 0x01000000) == 0x01000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000D00) && ((insn & 0x00000010) == 0x00000010) && ((insn & 0x01000000) == 0x01000000) && ((insn & 0x00200000) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000D00) && (true) && ((insn & 0x01000000) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000E00) && ((insn & 0x00000010) == 0x00000000) && ((insn & 0x01000000) == 0x01000000) && ((insn & 0x00200000) == 0x00200000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000E00) && ((insn & 0x00000010) == 0x00000000) && (true) && ((insn & 0x00200000) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000E00) && ((insn & 0x00000010) == 0x00000010) && ((insn & 0x01000000) == 0x01000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000700) == 0x00000700) && ((insn & 0x00000010) == 0x00000000) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000B00) == 0x00000300) && ((insn & 0x00000010) == 0x00000010) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000B00) == 0x00000B00) && ((insn & 0x00000010) == 0x00000010) && ((insn & 0x01000000) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000D00) == 0x00000100) && ((insn & 0x00000010) == 0x00000000) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000D00) == 0x00000800) && (true) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000900) == 0x00000000) && (true) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if ((true) && (true) && (true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_3same could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_dp_3diff.
 * Specified by: See Section A7.4.2.
 */
static inline const ClassDecoder &decode_simd_dp_3diff(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00000F00) == 0x00000D00) && ((insn & 0x01000000) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000D00) == 0x00000900) && ((insn & 0x01000000) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000900) == 0x00000800) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000800) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if ((true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_3diff could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_dp_2scalar.
 * Specified by: See Section A7.4.3.
 */
static inline const ClassDecoder &decode_simd_dp_2scalar(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00000F00) == 0x00000A00) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000B00) && ((insn & 0x01000000) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000B00) == 0x00000200) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000B00) == 0x00000300) && ((insn & 0x01000000) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000200) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if ((true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2scalar could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_dp_2shift.
 * Specified by: See Section A7.4.4.
 */
static inline const ClassDecoder &decode_simd_dp_2shift(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00000F00) == 0x00000500) && ((insn & 0x01000000) == 0x00000000) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000800) && ((insn & 0x01000000) == 0x00000000) && ((insn & 0x00000040) == 0x00000000) && ((insn & 0x00000080) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000800) && ((insn & 0x01000000) == 0x01000000) && ((insn & 0x00000040) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000800) && (true) && ((insn & 0x00000040) == 0x00000040) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000900) && (true) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000F00) == 0x00000A00) && (true) && ((insn & 0x00000040) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000E00) == 0x00000400) && ((insn & 0x01000000) == 0x01000000) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000600) == 0x00000600) && (true) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000C00) == 0x00000000) && (true) && (true) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if ((true) && (true) && (true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2shift could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_dp_2misc.
 * Specified by: See Section A7.4.5.
 */
static inline const ClassDecoder &decode_simd_dp_2misc(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00030000) == 0x00000000) && ((insn & 0x00000780) == 0x00000700)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00000000) && ((insn & 0x00000380) == 0x00000100)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00000000) && ((insn & 0x00000580) == 0x00000580)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00000000) && ((insn & 0x00000100) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00010000) && ((insn & 0x00000380) == 0x00000380)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00010000) && ((insn & 0x00000280) == 0x00000200)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00010000) && ((insn & 0x00000200) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00020000) && ((insn & 0x000007C0) == 0x00000300)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00020000) && ((insn & 0x000006C0) == 0x00000600)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00020000) && ((insn & 0x00000700) == 0x00000200)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00020000) && ((insn & 0x00000600) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00030000) == 0x00030000) && ((insn & 0x00000400) == 0x00000400)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if ((true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2misc could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_dp_1imm.
 * Specified by: See Section A7.4.6.
 */
static inline const ClassDecoder &decode_simd_dp_1imm(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00000020) == 0x00000000) && (true)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000F00) == 0x00000E00)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000F00) == 0x00000F00)) {
    return state->Undefined_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000E00) == 0x00000C00)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000C00) == 0x00000800)) {
    return state->EffectiveNoOp_instance_;
  }
  
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000800) == 0x00000000)) {
    return state->EffectiveNoOp_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_1imm could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_load_store.
 * Specified by: See Section A7.7.
 */
static inline const ClassDecoder &decode_simd_load_store(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00200000) == 0x00000000)) {
    return decode_simd_load_store_l0(insn, state);
  }
  
  if (((insn & 0x00200000) == 0x00200000)) {
    return decode_simd_load_store_l1(insn, state);
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_load_store_l0.
 * Specified by: See Section A7.7, Table A7-20.
 */
static inline const ClassDecoder &decode_simd_load_store_l0(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000F00) == 0x00000300)) {
    return state->VectorStore_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000700) == 0x00000200)) {
    return state->VectorStore_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000600) == 0x00000000)) {
    return state->VectorStore_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000C00) == 0x00000400)) {
    return state->VectorStore_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00800000) && ((insn & 0x00000C00) == 0x00000800)) {
    return state->VectorStore_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00800000) && ((insn & 0x00000800) == 0x00000000)) {
    return state->VectorStore_instance_;
  }
  
  if ((true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store_l0 could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

/*
 * Implementation of table simd_load_store_l1.
 * Specified by: See Section A7.7, Table A7-21.
 */
static inline const ClassDecoder &decode_simd_load_store_l1(
    const Instruction insn, const Arm32DecoderState *state) {
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000F00) == 0x00000300)) {
    return state->VectorLoad_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000700) == 0x00000200)) {
    return state->VectorLoad_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000600) == 0x00000000)) {
    return state->VectorLoad_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000C00) == 0x00000400)) {
    return state->VectorLoad_instance_;
  }
  
  if (((insn & 0x00800000) == 0x00800000) && (true)) {
    return state->VectorLoad_instance_;
  }
  
  if ((true) && (true)) {
    return state->Undefined_instance_;
  }
  
  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store_l1 could not parse %08X",insn.bits(31,0));
  return state->Forbidden_instance_;
}

Arm32DecoderState::Arm32DecoderState() :
  DecoderState()
  , CoprocessorOp_instance_()
  , ImmediateBic_instance_()
  , LoadMultiple_instance_()
  , LoadCoprocessor_instance_()
  , LoadDoubleExclusive_instance_()
  , Branch_instance_()
  , Test_instance_()
  , StoreRegister_instance_()
  , MoveDoubleFromCoprocessor_instance_()
  , TestImmediate_instance_()
  , BxBlx_instance_()
  , EffectiveNoOp_instance_()
  , LongMultiply_instance_()
  , Binary4RegisterShiftedOp_instance_()
  , Breakpoint_instance_()
  , Multiply_instance_()
  , PackSatRev_instance_()
  , LoadExclusive_instance_()
  , VectorStore_instance_()
  , Unary3RegisterShiftedOp_instance_()
  , Undefined_instance_()
  , DataProc_instance_()
  , Deprecated_instance_()
  , LoadImmediate_instance_()
  , StoreCoprocessor_instance_()
  , Roadblock_instance_()
  , LoadDoubleR_instance_()
  , StoreExclusive_instance_()
  , StoreImmediate_instance_()
  , MoveFromCoprocessor_instance_()
  , LoadRegister_instance_()
  , LoadDoubleI_instance_()
  , Binary3RegisterShiftedTest_instance_()
  , Unpredictable_instance_()
  , Forbidden_instance_()
  , VectorLoad_instance_()
  , MoveToStatusRegister_instance_()
  , SatAddSub_instance_()
 {
}

Arm32DecoderState::~Arm32DecoderState() {
}

const ClassDecoder &Arm32DecoderState::decode(const Instruction insn) const {
  return decode_ARMv7(insn, this);
}

}  // namespace
