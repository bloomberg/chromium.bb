/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_decoder.h"

#include <stdio.h>

using nacl_arm_dec::ClassDecoder;
using nacl_arm_dec::Instruction;

namespace nacl_arm_test {

NamedArm32DecoderState::NamedArm32DecoderState()
{}

NamedArm32DecoderState::~NamedArm32DecoderState() {}

/*
 * Implementation of table ARMv7.
 * Specified by: ('See Section A5.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_ARMv7(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x04000000 /* op1(27:25) == 010 */)
    return decode_load_store_word_byte(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25) == 011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */)
    return decode_load_store_word_byte(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25) == 011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return decode_media(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x00000000 /* op1(27:25) == 00x */)
    return decode_dp_misc(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x08000000 /* op1(27:25) == 10x */)
    return decode_branch_block_xfer(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x0C000000 /* op1(27:25) == 11x */)
    return decode_super_cop(insn);

  if ((insn.Bits() & 0xF0000000) == 0xF0000000 /* cond(31:28) == 1111 */)
    return decode_unconditional(insn);

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: ARMv7 could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table branch_block_xfer.
 * Specified by: ('See Section A5.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_branch_block_xfer(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x02500000) == 0x00000000 /* op(25:20) == 0xx0x0 */)
    return StoreImmediate_None_instance_;

  if ((insn.Bits() & 0x02500000) == 0x00100000 /* op(25:20) == 0xx0x1 */)
    return LoadMultiple_None_instance_;

  if ((insn.Bits() & 0x02400000) == 0x00400000 /* op(25:20) == 0xx1xx */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:20) == 1xxxxx */)
    return Branch_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: branch_block_xfer could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table dp_immed.
 * Specified by: ('See Section A5.2.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_immed(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x00400000 /* op(24:20) == 00100 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00500000 /* op(24:20) == 00101 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00800000 /* op(24:20) == 01000 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00900000 /* op(24:20) == 01001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op(24:20) == 10001 */)
    return MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op(24:20) == 10011 */)
    return BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op(24:20) == 10101 */)
    return BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op(24:20) == 10111 */)
    return BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op(24:20) == 0000x */)
    return Binary2RegisterImmediateOp_And_Rule_11_A1_P34_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op(24:20) == 0001x */)
    return Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op(24:20) == 0010x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op(24:20) == 0011x */)
    return Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op(24:20) == 0100x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op(24:20) == 0101x */)
    return Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op(24:20) == 0110x */)
    return Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op(24:20) == 0111x */)
    return Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op(24:20) == 1100x */)
    return Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op(24:20) == 1101x */)
    return Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op(24:20) == 1110x */)
    return MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op(24:20) == 1111x */)
    return Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_immed could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table dp_misc.
 * Specified by: ('See Section A5.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_misc(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* op2(7:4) == 0xx1 */)
    return decode_dp_reg_shifted(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return decode_dp_reg(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) == 0x01000000 /* op1(24:20) == 10xx0 */ &&
      (insn.Bits() & 0x00000090) == 0x00000080 /* op2(7:4) == 1xx0 */)
    return decode_half_mult(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) == 0x01000000 /* op1(24:20) == 10xx0 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:4) == 0xxx */)
    return decode_misc(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) != 0x00200000 /* op1(24:20) == ~0xx1x */ &&
      (insn.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4) == 1011 */)
    return decode_extra_load_store(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) != 0x00200000 /* op1(24:20) == ~0xx1x */ &&
      (insn.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4) == 11x1 */)
    return decode_extra_load_store(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4) == 1011 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4) == 11x1 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* op1(24:20) == 0xxxx */ &&
      (insn.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4) == 1001 */)
    return decode_mult(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* op1(24:20) == 1xxxx */ &&
      (insn.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4) == 1001 */)
    return decode_sync(insn);

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01F00000) == 0x01000000 /* op1(24:20) == 10000 */)
    return Unary1RegisterImmediateOp_Mov_Rule_96_A2_P_194_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01F00000) == 0x01400000 /* op1(24:20) == 10100 */)
    return DataProc_None_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01B00000) == 0x01200000 /* op1(24:20) == 10x10 */)
    return decode_msr_and_hints(insn);

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */)
    return decode_dp_immed(insn);

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_misc could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table dp_reg.
 * Specified by: ('See Section A5.2.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_reg(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20) == 10001 */)
    return Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20) == 10011 */)
    return Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20) == 10101 */)
    return Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20) == 10111 */)
    return Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20) == 0000x */)
    return Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20) == 0001x */)
    return Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20) == 0010x */)
    return Binary3RegisterImmedShiftedOp_SubRule_213_A1_P422_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20) == 0011x */)
    return Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20) == 0100x */)
    return Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20) == 0101x */)
    return Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20) == 0110x */)
    return Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20) == 0111x */)
    return Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20) == 1100x */)
    return Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */)
    return Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */)
    return Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */)
    return Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */)
    return Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000020 /* op3(6:5) == 01 */)
    return Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op3(6:5) == 10 */)
    return Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */)
    return Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */)
    return Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_reg could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table dp_reg_shifted.
 * Specified by: ('See Section A5.2.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_reg_shifted(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20) == 10001 */)
    return Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20) == 10011 */)
    return Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20) == 10101 */)
    return Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20) == 10111 */)
    return Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20) == 0000x */)
    return Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20) == 0001x */)
    return Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20) == 0010x */)
    return Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20) == 0011x */)
    return Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20) == 0100x */)
    return Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20) == 0101x */)
    return Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20) == 0110x */)
    return Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20) == 0111x */)
    return Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20) == 1100x */)
    return Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(6:5) == 00 */)
    return Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */)
    return Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */)
    return Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */)
    return Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */)
    return Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */)
    return Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_reg_shifted could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table ext_reg_load_store.
 * Specified by: ('A7.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_ext_reg_load_store(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x01B00000) == 0x00900000 /* opcode(24:20) == 01x01 */)
    return LoadCoprocessor_None_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20) == 01x11 */)
    return LoadCoprocessor_None_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20) == 10x10 */)
    return StoreCoprocessor_None_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01300000 /* opcode(24:20) == 10x11 */)
    return LoadCoprocessor_None_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* opcode(24:20) == 0010x */)
    return decode_ext_reg_transfers(insn);

  if ((insn.Bits() & 0x01300000) == 0x01000000 /* opcode(24:20) == 1xx00 */)
    return StoreCoprocessor_None_instance_;

  if ((insn.Bits() & 0x01300000) == 0x01100000 /* opcode(24:20) == 1xx01 */)
    return LoadCoprocessor_None_instance_;

  if ((insn.Bits() & 0x01900000) == 0x00800000 /* opcode(24:20) == 01xx0 */)
    return StoreCoprocessor_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: ext_reg_load_store could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table ext_reg_move.
 * Specified by: ('A7.8 page A7 - 31',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_ext_reg_move(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00000000 /* A(23:21) == 000 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21) == 111 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x00800000) == 0x00000000 /* A(23:21) == 0xx */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x00800000) == 0x00800000 /* A(23:21) == 1xx */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:5) == 0x */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* L(20:20) == 1 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */)
    return CoprocessorOp_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: ext_reg_move could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table ext_reg_transfers.
 * Specified by: ('A7.8 page A7 - 32',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_ext_reg_transfers(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x000000D0) == 0x00000010 /* op(7:4) == 00x1 */)
    return MoveDoubleFromCoprocessor_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: ext_reg_transfers could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table extra_load_store.
 * Specified by: ('See Section A5.2.8',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_extra_load_store(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return Store3RegisterOp_Strh_Rule_208_A1_P412_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */)
    return Load3RegisterOp_Ldrh_Rule_76_A1_P156_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */)
    return Store2RegisterImm8Op_Strh_Rule_207_A1_P410_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */)
    return Load3RegisterOp_Ldrsb_Rule_80_A1_P164_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Load2RegisterImm8Op_ldrsb_Rule_79_A1_162_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return Store3RegisterDoubleOp_Strd_Rule_201_A1_P398_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */)
    return Load3RegisterOp_Ldrsh_Rule_84_A1_P172_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */)
    return Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: extra_load_store could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table half_mult.
 * Specified by: ('See Section A5.2.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_half_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00600000) == 0x00000000 /* op1(22:21) == 00 */)
    return Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:21) == 01 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op(5:5) == 0 */)
    return Binary4RegisterDualOp_Smlawx_Rule_171_A1_340_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:21) == 01 */ &&
      (insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */)
    return Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00400000 /* op1(22:21) == 10 */)
    return Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op1(22:21) == 11 */)
    return Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: half_mult could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table load_store_word_byte.
 * Specified by: ('See Section A5.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_load_store_word_byte(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00000000 /* op1(24:20) == 0x000 */)
    return Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00100000 /* op1(24:20) == 0x001 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00100000 /* op1(24:20) == 0x001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00400000 /* op1(24:20) == 0x100 */)
    return Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01000000 /* op1(24:20) == 1x0x0 */)
    return Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01400000 /* op1(24:20) == 1x1x0 */)
    return Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00000000 /* op1(24:20) == 0x000 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00400000 /* op1(24:20) == 0x100 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x00700000) == 0x00100000 /* op1(24:20) == xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01000000 /* op1(24:20) == 1x0x0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01400000 /* op1(24:20) == 1x1x0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Forbidden_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: load_store_word_byte could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table media.
 * Specified by: ('See Section A5.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_media(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20) == 11000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12) == ~1111 */)
    return Binary4RegisterDualOp_Usda8_Rule_254_A1_P502_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20) == 11000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12) == 1111 */)
    return Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01F00000 /* op1(24:20) == 11111 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Roadblock_None_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(7:5) == x10 */)
    return Binary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0) == ~1111 */)
    return Binary2RegisterBitRange_Bfi_Rule_18_A1_P48_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0) == 1111 */)
    return Unary1RegisterBitRange_Bfc_17_A1_P46_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(7:5) == x10 */)
    return Binary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466_instance_;

  if ((insn.Bits() & 0x01C00000) == 0x00000000 /* op1(24:20) == 000xx */)
    return decode_parallel_add_sub_signed(insn);

  if ((insn.Bits() & 0x01C00000) == 0x00400000 /* op1(24:20) == 001xx */)
    return decode_parallel_add_sub_unsigned(insn);

  if ((insn.Bits() & 0x01800000) == 0x00800000 /* op1(24:20) == 01xxx */)
    return decode_pack_sat_rev(insn);

  if ((insn.Bits() & 0x01800000) == 0x01000000 /* op1(24:20) == 10xxx */)
    return decode_signed_mult(insn);

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: media could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table misc.
 * Specified by: ('See Section A5.2.12',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_misc(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00000000 /* op1(19:16) == xx00 */)
    return MoveToStatusRegister_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* op(22:21) == x0 */)
    return DataProc_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */)
    return BxBlx_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */)
    return DataProc_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000020 /* op2(6:4) == 010 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000030 /* op2(6:4) == 011 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */)
    return BxBlx_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000050 /* op2(6:4) == 101 */)
    return decode_sat_add_sub(insn);

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */)
    return Breakpoint_None_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */)
    return Forbidden_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: misc could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table misc_hints_simd.
 * Specified by: ('See Section A5.7.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_misc_hints_simd(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20) == 0010000 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000000 /* op2(7:4) == 0000 */ &&
      (insn.Bits() & 0x00010000) == 0x00010000 /* Rn(19:16) == xxx1 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20) == 0010000 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:4) == xx0x */ &&
      (insn.Bits() & 0x00010000) == 0x00000000 /* Rn(19:16) == xxx0 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000010 /* op2(7:4) == 0001 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000050 /* op2(7:4) == 0101 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000D0) == 0x00000040 /* op2(7:4) == 01x0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x07700000) == 0x04100000 /* op1(26:20) == 100x001 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x07700000) == 0x04500000 /* op1(26:20) == 100x101 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05100000 /* op1(26:20) == 101x001 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05100000 /* op1(26:20) == 101x001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Unpredictable_None_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05500000 /* op1(26:20) == 101x101 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x07700000) == 0x06500000 /* op1(26:20) == 110x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x07700000) == 0x07500000 /* op1(26:20) == 111x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x06700000) == 0x06100000 /* op1(26:20) == 11xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x06300000) == 0x04300000 /* op1(26:20) == 10xxx11 */)
    return Unpredictable_None_instance_;

  if ((insn.Bits() & 0x06300000) == 0x06300000 /* op1(26:20) == 11xxx11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return Unpredictable_None_instance_;

  if ((insn.Bits() & 0x07100000) == 0x04000000 /* op1(26:20) == 100xxx0 */)
    return decode_simd_load_store(insn);

  if ((insn.Bits() & 0x06000000) == 0x02000000 /* op1(26:20) == 01xxxxx */)
    return decode_simd_dp(insn);

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: misc_hints_simd could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table msr_and_hints.
 * Specified by: ('See Section A5.2.11',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_msr_and_hints(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000000 /* op2(7:0) == 00000000 */)
    return CondNop_Nop_Rule_110_A1_P222_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000001 /* op2(7:0) == 00000001 */)
    return CondNop_Yield_Rule_413_A1_P812_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000002 /* op2(7:0) == 00000010 */)
    return ForbiddenCondNop_Wfe_Rule_411_A1_P808_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000003 /* op2(7:0) == 00000011 */)
    return ForbiddenCondNop_Wfi_Rule_412_A1_P810_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000004 /* op2(7:0) == 00000100 */)
    return ForbiddenCondNop_Sev_Rule_158_A1_P316_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000F0) == 0x000000F0 /* op2(7:0) == 1111xxxx */)
    return CondNop_Dbg_Rule_40_A1_P88_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00040000 /* op1(19:16) == 0100 */)
    return MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000B0000) == 0x00080000 /* op1(19:16) == 1x00 */)
    return MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */)
    return ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */)
    return ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00400000 /* op(22:22) == 1 */)
    return ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if (true)
    return Forbidden_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: msr_and_hints could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table mult.
 * Specified by: ('See Section A5.2.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00400000 /* op(23:20) == 0100 */)
    return Binary4RegisterDualResult_Umaal_Rule_244_A1_P482_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00600000 /* op(23:20) == 0110 */)
    return Binary4RegisterDualOp_Mls_Rule_95_A1_P192_instance_;

  if ((insn.Bits() & 0x00D00000) == 0x00500000 /* op(23:20) == 01x1 */)
    return Undefined_None_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00000000 /* op(23:20) == 000x */)
    return Binary3RegisterOpAltA_Mul_Rule_105_A1_P212_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00200000 /* op(23:20) == 001x */)
    return Binary4RegisterDualOp_Mla_Rule_94_A1_P190_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00800000 /* op(23:20) == 100x */)
    return Binary4RegisterDualResult_Umull_Rule_246_A1_P486_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00A00000 /* op(23:20) == 101x */)
    return Binary4RegisterDualResult_Umlal_Rule_245_A1_P484_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00C00000 /* op(23:20) == 110x */)
    return Binary4RegisterDualResult_Smull_Rule_179_A1_P356_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00E00000 /* op(23:20) == 111x */)
    return Binary4RegisterDualResult_Smlal_Rule_168_A1_P334_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: mult could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table other_vfp_data_proc.
 * Specified by: ('A7.5 Table A7 - 17, page 17 - 25',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_other_vfp_data_proc(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16) == 0001 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00070000 /* opc2(19:16) == 0111 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6) == 11 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00070000) == 0x00000000 /* opc2(19:16) == x000 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x000E0000) == 0x000E0000 /* opc2(19:16) == 111x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00060000) == 0x00020000 /* opc2(19:16) == x01x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00060000) == 0x00040000 /* opc2(19:16) == x10x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */)
    return CoprocessorOp_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: other_vfp_data_proc could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table pack_sat_rev.
 * Specified by: ('See Section A5.4.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_pack_sat_rev(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */)
    return PackSatRev_None_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */)
    return PackSatRev_None_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return PackSatRev_None_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return PackSatRev_None_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(22:20) == x11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return PackSatRev_None_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(22:20) == x11 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */)
    return PackSatRev_None_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:20) == 01x */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return PackSatRev_None_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* op1(22:20) == xx0 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return PackSatRev_None_instance_;

  if ((insn.Bits() & 0x00200000) == 0x00200000 /* op1(22:20) == x1x */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */)
    return PackSatRev_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: pack_sat_rev could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table parallel_add_sub_signed.
 * Specified by: ('See Section A5.4.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_add_sub_signed(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */)
    return Binary3RegisterOpAltB_Sadd16_Rule_148_A1_P296_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return Binary3RegisterOpAltB_Sasx_Rule_150_A1_P300_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */)
    return Binary3RegisterOpAltB_Ssax_Rule_185_A1_P366_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return Binary3RegisterOpAltB_Ssub16_Rule_186_A1_P368_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return Binary3RegisterOpAltB_Ssad8_Rule_149_A1_P298_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Binary3RegisterOpAltB_Ssub8_Rule_187_A1_P370_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */)
    return Binary3RegisterOpAltB_Qadd16_Rule_125_A1_P252_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return Binary3RegisterOpAltB_Qasx_Rule_127_A1_P256_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */)
    return Binary3RegisterOpAltB_Qsax_Rule_130_A1_P262_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return Binary3RegisterOpAltB_Qsub16_Rule_132_A1_P266_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return Binary3RegisterOpAltB_Qadd8_Rule_126_A1_P254_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Binary3RegisterOpAltB_Qsub8_Rule_133_A1_P268_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */)
    return Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: parallel_add_sub_signed could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table parallel_add_sub_unsigned.
 * Specified by: ('See Section A5.4.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_add_sub_unsigned(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return DataProc_None_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return DataProc_None_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return DataProc_None_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return DataProc_None_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return DataProc_None_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return DataProc_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: parallel_add_sub_unsigned could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table sat_add_sub.
 * Specified by: ('See Section A5.2.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_sat_add_sub(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (true)
    return SatAddSub_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: sat_add_sub could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table signed_mult.
 * Specified by: ('See Section A5.4.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_signed_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */)
    return Multiply_None_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */)
    return Multiply_None_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */)
    return Multiply_None_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return LongMultiply_None_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */)
    return Multiply_None_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* op2(7:5) == 11x */)
    return Multiply_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: signed_mult could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp.
 * Specified by: ('See Section A7.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x00B80000) == 0x00800000 /* A(23:19) == 1x000 */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_1imm(insn);

  if ((insn.Bits() & 0x00B80000) == 0x00880000 /* A(23:19) == 1x001 */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if ((insn.Bits() & 0x00B00000) == 0x00900000 /* A(23:19) == 1x01x */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if ((insn.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19) == 1x10x */ &&
      (insn.Bits() & 0x00000050) == 0x00000000 /* C(7:4) == x0x0 */)
    return decode_simd_dp_3diff(insn);

  if ((insn.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19) == 1x10x */ &&
      (insn.Bits() & 0x00000050) == 0x00000040 /* C(7:4) == x1x0 */)
    return decode_simd_dp_2scalar(insn);

  if ((insn.Bits() & 0x00A00000) == 0x00800000 /* A(23:19) == 1x0xx */ &&
      (insn.Bits() & 0x00000050) == 0x00000000 /* C(7:4) == x0x0 */)
    return decode_simd_dp_3diff(insn);

  if ((insn.Bits() & 0x00A00000) == 0x00800000 /* A(23:19) == 1x0xx */ &&
      (insn.Bits() & 0x00000050) == 0x00000040 /* C(7:4) == x1x0 */)
    return decode_simd_dp_2scalar(insn);

  if ((insn.Bits() & 0x00A00000) == 0x00A00000 /* A(23:19) == 1x1xx */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:19) == 0xxxx */)
    return decode_simd_dp_3same(insn);

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:19) == 1xxxx */ &&
      (insn.Bits() & 0x00000090) == 0x00000090 /* C(7:4) == 1xx1 */)
    return decode_simd_dp_2shift(insn);

  if ((insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000F00) == 0x00000C00 /* B(11:8) == 1100 */ &&
      (insn.Bits() & 0x00000090) == 0x00000000 /* C(7:4) == 0xx0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* B(11:8) == 10xx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* B(11:8) == 0xxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */)
    return decode_simd_dp_2misc(insn);

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_1imm.
 * Specified by: ('See Section A7.4.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_1imm(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000020) == 0x00000000 /* op(5:5) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000E00 /* cmode(11:8) == 1110 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000F00 /* cmode(11:8) == 1111 */)
    return Undefined_None_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000C00 /* cmode(11:8) == 110x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* cmode(11:8) == 10xx */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* cmode(11:8) == 0xxx */)
    return EffectiveNoOp_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_1imm could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_2misc.
 * Specified by: ('See Section A7.4.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2misc(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000780) == 0x00000700 /* B(10:6) == 1110x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000380) == 0x00000100 /* B(10:6) == x010x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000580) == 0x00000580 /* B(10:6) == 1x11x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000100) == 0x00000000 /* B(10:6) == xx0xx */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000380) == 0x00000380 /* B(10:6) == x111x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000280) == 0x00000200 /* B(10:6) == x1x0x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000200) == 0x00000000 /* B(10:6) == x0xxx */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x000007C0) == 0x00000300 /* B(10:6) == 01100 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x000006C0) == 0x00000600 /* B(10:6) == 11x00 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(10:6) == 010xx */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(10:6) == 00xxx */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00030000 /* A(17:16) == 11 */ &&
      (insn.Bits() & 0x00000400) == 0x00000400 /* B(10:6) == 1xxxx */)
    return EffectiveNoOp_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2misc could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_2scalar.
 * Specified by: ('See Section A7.4.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2scalar(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8) == 1010 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8) == 1011 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000200 /* A(11:8) == 0x10 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000300 /* A(11:8) == 0x11 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000200) == 0x00000000 /* A(11:8) == xx0x */)
    return EffectiveNoOp_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2scalar could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_2shift.
 * Specified by: ('See Section A7.4.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2shift(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000500 /* A(11:8) == 0101 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* B(6:6) == 1 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* L(7:7) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000900 /* A(11:8) == 1001 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8) == 1010 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000E00) == 0x00000400 /* A(11:8) == 010x */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000600) == 0x00000600 /* A(11:8) == x11x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000C00) == 0x00000000 /* A(11:8) == 00xx */)
    return EffectiveNoOp_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2shift could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_3diff.
 * Specified by: ('See Section A7.4.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_3diff(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000D00) == 0x00000900 /* A(11:8) == 10x1 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000900) == 0x00000800 /* A(11:8) == 1xx0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000800) == 0x00000000 /* A(11:8) == 0xxx */)
    return EffectiveNoOp_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_3diff could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_3same.
 * Specified by: ('See Section A7.4.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_3same(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000100 /* A(11:8) == 0001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000500 /* A(11:8) == 0101 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000900 /* A(11:8) == 1001 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8) == 1011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* C(21:20) == 0x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* C(21:20) == 0x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00200000) == 0x00200000 /* C(21:20) == 1x */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000700) == 0x00000700 /* A(11:8) == x111 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000300 /* A(11:8) == 0x11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000B00 /* A(11:8) == 1x11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000D00) == 0x00000100 /* A(11:8) == 00x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000D00) == 0x00000800 /* A(11:8) == 10x0 */)
    return EffectiveNoOp_None_instance_;

  if ((insn.Bits() & 0x00000900) == 0x00000000 /* A(11:8) == 0xx0 */)
    return EffectiveNoOp_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_3same could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_load_store.
 * Specified by: ('See Section A7.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_load_store(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x00200000) == 0x00000000 /* L(21:21) == 0 */)
    return decode_simd_load_store_l0(insn);

  if ((insn.Bits() & 0x00200000) == 0x00200000 /* L(21:21) == 1 */)
    return decode_simd_load_store_l1(insn);

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_load_store_l0.
 * Specified by: ('See Section A7.7, Table A7 - 20',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_load_store_l0(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000300 /* B(11:8) == 0011 */)
    return VectorStore_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(11:8) == x010 */)
    return VectorStore_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(11:8) == x00x */)
    return VectorStore_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000400 /* B(11:8) == 01xx */)
    return VectorStore_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* B(11:8) == 10xx */)
    return VectorStore_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* B(11:8) == 0xxx */)
    return VectorStore_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store_l0 could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_load_store_l1.
 * Specified by: ('See Section A7.7, Table A7 - 21',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_load_store_l1(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000300 /* B(11:8) == 0011 */)
    return VectorLoad_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(11:8) == x010 */)
    return VectorLoad_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(11:8) == x00x */)
    return VectorLoad_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000400 /* B(11:8) == 01xx */)
    return VectorLoad_None_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */)
    return VectorLoad_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store_l1 could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table super_cop.
 * Specified by: ('See Section A5.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_super_cop(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x03F00000) == 0x00400000 /* op1(25:20) == 000100 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x03F00000) == 0x00500000 /* op1(25:20) == 000101 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return MoveDoubleFromCoprocessor_None_instance_;

  if ((insn.Bits() & 0x03E00000) == 0x00000000 /* op1(25:20) == 00000x */)
    return Undefined_None_instance_;

  if ((insn.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20) == 00010x */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */)
    return decode_ext_reg_transfers(insn);

  if ((insn.Bits() & 0x03100000) == 0x02100000 /* op1(25:20) == 10xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return MoveFromCoprocessor_None_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00000000 /* op1(25:20) == 0xxxx0 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return StoreCoprocessor_None_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00100000 /* op1(25:20) == 0xxxx1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return LoadCoprocessor_None_instance_;

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */)
    return decode_vfp_data_proc(insn);

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */)
    return decode_ext_reg_move(insn);

  if ((insn.Bits() & 0x03000000) == 0x03000000 /* op1(25:20) == 11xxxx */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op1(25:20) == 0xxxxx */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */)
    return decode_ext_reg_load_store(insn);

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: super_cop could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table sync.
 * Specified by: ('See Section A5.2.10',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_sync(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00800000 /* op(23:20) == 1000 */)
    return StoreExclusive_None_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00900000 /* op(23:20) == 1001 */)
    return LoadExclusive_None_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00B00000 /* op(23:20) == 1011 */)
    return LoadDoubleExclusive_None_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00C00000 /* op(23:20) == 1100 */)
    return StoreExclusive_None_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00000000 /* op(23:20) == 0x00 */)
    return Deprecated_None_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00A00000 /* op(23:20) == 1x10 */)
    return StoreExclusive_None_instance_;

  if ((insn.Bits() & 0x00D00000) == 0x00D00000 /* op(23:20) == 11x1 */)
    return LoadExclusive_None_instance_;

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: sync could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table unconditional.
 * Specified by: ('See Section A5.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_unconditional(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x0FF00000) == 0x0C400000 /* op1(27:20) == 11000100 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x0FF00000) == 0x0C500000 /* op1(27:20) == 11000101 */)
    return MoveDoubleFromCoprocessor_None_instance_;

  if ((insn.Bits() & 0x0FB00000) == 0x0C200000 /* op1(27:20) == 11000x10 */)
    return StoreCoprocessor_None_instance_;

  if ((insn.Bits() & 0x0FB00000) == 0x0C300000 /* op1(27:20) == 11000x11 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return LoadCoprocessor_None_instance_;

  if ((insn.Bits() & 0x0F900000) == 0x0C800000 /* op1(27:20) == 11001xx0 */)
    return StoreCoprocessor_None_instance_;

  if ((insn.Bits() & 0x0F900000) == 0x0C900000 /* op1(27:20) == 11001xx1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return LoadCoprocessor_None_instance_;

  if ((insn.Bits() & 0x0E500000) == 0x08100000 /* op1(27:20) == 100xx0x1 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x0E500000) == 0x08400000 /* op1(27:20) == 100xx1x0 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0D000000 /* op1(27:20) == 1101xxx0 */)
    return StoreCoprocessor_None_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0D100000 /* op1(27:20) == 1101xxx1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return LoadCoprocessor_None_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0E000000 /* op1(27:20) == 1110xxx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0E100000 /* op1(27:20) == 1110xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return MoveFromCoprocessor_None_instance_;

  if ((insn.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20) == 1110xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x0E000000) == 0x0A000000 /* op1(27:20) == 101xxxxx */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x08000000) == 0x00000000 /* op1(27:20) == 0xxxxxxx */)
    return decode_misc_hints_simd(insn);

  if (true)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: unconditional could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


/*
 * Implementation of table vfp_data_proc.
 * Specified by: ('A7.5 Table A7 - 16, page A7 - 24',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_vfp_data_proc(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x00B00000) == 0x00800000 /* opc1(23:20) == 1x00 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00B00000 /* opc1(23:20) == 1x11 */)
    return decode_other_vfp_data_proc(insn);

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* opc1(23:20) == 0xxx */)
    return CoprocessorOp_None_instance_;

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: vfp_data_proc could not parse %08X",
          insn.Bits());
  return Forbidden_None_instance_;
}


const NamedClassDecoder& NamedArm32DecoderState::
decode_named(const nacl_arm_dec::Instruction insn) const {
  return decode_ARMv7(insn);
}

const nacl_arm_dec::ClassDecoder& NamedArm32DecoderState::
decode(const nacl_arm_dec::Instruction insn) const {
  return decode_named(insn).named_decoder();
}

}  // namespace nacl_arm_test
