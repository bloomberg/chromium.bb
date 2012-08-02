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
  return not_implemented_;
}


/*
 * Implementation of table branch_block_xfer.
 * Specified by: ('See Section A5.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_branch_block_xfer(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x03D00000) == 0x00000000 /* op(25:20) == 0000x0 */ &&
      (insn.Bits() & 0x0FD00000) == 0x08000000 /* $pattern(31:0) == xxxx100000x0xxxxxxxxxxxxxxxxxxxx */)
    return StoreRegisterList_Stmda_Stmed_Rule_190_A1_P376_instance_;

  if ((insn.Bits() & 0x03D00000) == 0x00100000 /* op(25:20) == 0000x1 */ &&
      (insn.Bits() & 0x0FD00000) == 0x08100000 /* $pattern(31:0) == xxxx100000x1xxxxxxxxxxxxxxxxxxxx */)
    return LoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112_instance_;

  if ((insn.Bits() & 0x03D00000) == 0x00800000 /* op(25:20) == 0010x0 */ &&
      (insn.Bits() & 0x0FD00000) == 0x08800000 /* $pattern(31:0) == xxxx100010x0xxxxxxxxxxxxxxxxxxxx */)
    return StoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374_instance_;

  if ((insn.Bits() & 0x03D00000) == 0x00900000 /* op(25:20) == 0010x1 */ &&
      (insn.Bits() & 0x0FD00000) == 0x08900000 /* $pattern(31:0) == xxxx100010x1xxxxxxxxxxxxxxxxxxxx */)
    return LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_;

  if ((insn.Bits() & 0x03D00000) == 0x01000000 /* op(25:20) == 0100x0 */ &&
      (insn.Bits() & 0x0FD00000) == 0x09000000 /* $pattern(31:0) == xxxx100100x0xxxxxxxxxxxxxxxxxxxx */)
    return StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_;

  if ((insn.Bits() & 0x03D00000) == 0x01100000 /* op(25:20) == 0100x1 */ &&
      (insn.Bits() & 0x0FD00000) == 0x09100000 /* $pattern(31:0) == xxxx100100x1xxxxxxxxxxxxxxxxxxxx */)
    return LoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114_instance_;

  if ((insn.Bits() & 0x03D00000) == 0x01800000 /* op(25:20) == 0110x0 */ &&
      (insn.Bits() & 0x0FD00000) == 0x09800000 /* $pattern(31:0) == xxxx100110x0xxxxxxxxxxxxxxxxxxxx */)
    return StoreRegisterList_Stmid_Stmfa_Rule_192_A1_P380_instance_;

  if ((insn.Bits() & 0x03D00000) == 0x01900000 /* op(25:20) == 0110x1 */ &&
      (insn.Bits() & 0x0FD00000) == 0x09900000 /* $pattern(31:0) == xxxx100110x1xxxxxxxxxxxxxxxxxxxx */)
    return LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116_instance_;

  if ((insn.Bits() & 0x02500000) == 0x00400000 /* op(25:20) == 0xx1x0 */ &&
      (insn.Bits() & 0x0E700000) == 0x08400000 /* $pattern(31:0) == xxxx100xx100xxxxxxxxxxxxxxxxxxxx */)
    return ForbiddenCondNop_Stm_Rule_11_B6_A1_P22_instance_;

  if ((insn.Bits() & 0x02500000) == 0x00500000 /* op(25:20) == 0xx1x1 */ &&
      (insn.Bits() & 0x00008000) == 0x00000000 /* R(15:15) == 0 */ &&
      (insn.Bits() & 0x0E708000) == 0x08500000 /* $pattern(31:0) == xxxx100xx101xxxx0xxxxxxxxxxxxxxx */)
    return ForbiddenCondNop_Ldm_Rule_3_B6_A1_P7_instance_;

  if ((insn.Bits() & 0x02500000) == 0x00500000 /* op(25:20) == 0xx1x1 */ &&
      (insn.Bits() & 0x00008000) == 0x00008000 /* R(15:15) == 1 */ &&
      (insn.Bits() & 0x0E508000) == 0x08508000 /* $pattern(31:0) == xxxx100xx1x1xxxx1xxxxxxxxxxxxxxx */)
    return ForbiddenCondNop_Ldm_Rule_2_B6_A1_P5_instance_;

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x0F000000) == 0x0A000000 /* $pattern(31:0) == xxxx1010xxxxxxxxxxxxxxxxxxxxxxxx */)
    return BranchImmediate24_B_Rule_16_A1_P44_instance_;

  if ((insn.Bits() & 0x03000000) == 0x03000000 /* op(25:20) == 11xxxx */ &&
      (insn.Bits() & 0x0F000000) == 0x0B000000 /* $pattern(31:0) == xxxx1011xxxxxxxxxxxxxxxxxxxxxxxx */)
    return BranchImmediate24_Bl_Blx_Rule_23_A1_P58_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table dp_immed.
 * Specified by: ('See Section A5.2.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_immed(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x00400000 /* op(24:20) == 00100 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF0000) == 0x024F0000 /* $pattern(31:0) == xxxx001001001111xxxxxxxxxxxxxxxx */)
    return Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00500000 /* op(24:20) == 00101 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF0000) == 0x025F0000 /* $pattern(31:0) == xxxx001001011111xxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00800000 /* op(24:20) == 01000 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF0000) == 0x028F0000 /* $pattern(31:0) == xxxx001010001111xxxxxxxxxxxxxxxx */)
    return Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00900000 /* op(24:20) == 01001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF0000) == 0x029F0000 /* $pattern(31:0) == xxxx001010011111xxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op(24:20) == 10001 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x03100000 /* $pattern(31:0) == xxxx00110001xxxx0000xxxxxxxxxxxx */)
    return MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op(24:20) == 10011 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x03300000 /* $pattern(31:0) == xxxx00110011xxxx0000xxxxxxxxxxxx */)
    return BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op(24:20) == 10101 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x03500000 /* $pattern(31:0) == xxxx00110101xxxx0000xxxxxxxxxxxx */)
    return BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op(24:20) == 10111 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x03700000 /* $pattern(31:0) == xxxx00110111xxxx0000xxxxxxxxxxxx */)
    return BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op(24:20) == 0000x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02000000 /* $pattern(31:0) == xxxx0010000xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_And_Rule_11_A1_P34_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op(24:20) == 0001x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02200000 /* $pattern(31:0) == xxxx0010001xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op(24:20) == 0010x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FE00000) == 0x02400000 /* $pattern(31:0) == xxxx0010010xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op(24:20) == 0011x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02600000 /* $pattern(31:0) == xxxx0010011xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op(24:20) == 0100x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FE00000) == 0x02800000 /* $pattern(31:0) == xxxx0010100xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op(24:20) == 0101x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02A00000 /* $pattern(31:0) == xxxx0010101xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op(24:20) == 0110x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02C00000 /* $pattern(31:0) == xxxx0010110xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op(24:20) == 0111x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02E00000 /* $pattern(31:0) == xxxx0010111xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op(24:20) == 1100x */ &&
      (insn.Bits() & 0x0FE00000) == 0x03800000 /* $pattern(31:0) == xxxx0011100xxxxxxxxxxxxxxxxxxxxx */)
    return Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op(24:20) == 1101x */ &&
      (insn.Bits() & 0x0FEF0000) == 0x03A00000 /* $pattern(31:0) == xxxx0011101x0000xxxxxxxxxxxxxxxx */)
    return Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op(24:20) == 1110x */ &&
      (insn.Bits() & 0x0FE00000) == 0x03C00000 /* $pattern(31:0) == xxxx0011110xxxxxxxxxxxxxxxxxxxxx */)
    return MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op(24:20) == 1111x */ &&
      (insn.Bits() & 0x0FEF0000) == 0x03E00000 /* $pattern(31:0) == xxxx0011111x0000xxxxxxxxxxxxxxxx */)
    return Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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
      (insn.Bits() & 0x01F00000) == 0x01000000 /* op1(24:20) == 10000 */ &&
      (insn.Bits() & 0x0FF00000) == 0x03000000 /* $pattern(31:0) == xxxx00110000xxxxxxxxxxxxxxxxxxxx */)
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
  return not_implemented_;
}


/*
 * Implementation of table dp_reg.
 * Specified by: ('See Section A5.2.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_reg(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20) == 10001 */ &&
      (insn.Bits() & 0x0FF0F010) == 0x01100000 /* $pattern(31:0) == xxxx00010001xxxx0000xxxxxxx0xxxx */)
    return Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20) == 10011 */ &&
      (insn.Bits() & 0x0FF0F010) == 0x01300000 /* $pattern(31:0) == xxxx00010011xxxx0000xxxxxxx0xxxx */)
    return Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20) == 10101 */ &&
      (insn.Bits() & 0x0FF0F010) == 0x01500000 /* $pattern(31:0) == xxxx00010101xxxx0000xxxxxxx0xxxx */)
    return Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20) == 10111 */ &&
      (insn.Bits() & 0x0FF0F010) == 0x01700000 /* $pattern(31:0) == xxxx00010111xxxx0000xxxxxxx0xxxx */)
    return Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20) == 0000x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00000000 /* $pattern(31:0) == xxxx0000000xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20) == 0001x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00200000 /* $pattern(31:0) == xxxx0000001xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20) == 0010x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00400000 /* $pattern(31:0) == xxxx0000010xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Sub_Rule_213_A1_P422_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20) == 0011x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00600000 /* $pattern(31:0) == xxxx0000011xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20) == 0100x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00800000 /* $pattern(31:0) == xxxx0000100xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20) == 0101x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00A00000 /* $pattern(31:0) == xxxx0000101xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20) == 0110x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00C00000 /* $pattern(31:0) == xxxx0000110xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20) == 0111x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00E00000 /* $pattern(31:0) == xxxx0000111xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20) == 1100x */ &&
      (insn.Bits() & 0x0FE00010) == 0x01800000 /* $pattern(31:0) == xxxx0001100xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */ &&
      (insn.Bits() & 0x0FEF0070) == 0x01A00000 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxxx000xxxx */)
    return Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */ &&
      (insn.Bits() & 0x0FEF0070) == 0x01A00060 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxxx110xxxx */)
    return Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */ &&
      (insn.Bits() & 0x0FEF0FF0) == 0x01A00000 /* $pattern(31:0) == xxxx0001101x0000xxxx00000000xxxx */)
    return Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */ &&
      (insn.Bits() & 0x0FEF0FF0) == 0x01A00060 /* $pattern(31:0) == xxxx0001101x0000xxxx00000110xxxx */)
    return Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000020 /* op3(6:5) == 01 */ &&
      (insn.Bits() & 0x0FEF0070) == 0x01A00020 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxxx010xxxx */)
    return Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op3(6:5) == 10 */ &&
      (insn.Bits() & 0x0FEF0070) == 0x01A00040 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxxx100xxxx */)
    return Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x0FE00010) == 0x01C00000 /* $pattern(31:0) == xxxx0001110xxxxxxxxxxxxxxxx0xxxx */)
    return Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */ &&
      (insn.Bits() & 0x0FEF0010) == 0x01E00000 /* $pattern(31:0) == xxxx0001111x0000xxxxxxxxxxx0xxxx */)
    return Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table dp_reg_shifted.
 * Specified by: ('See Section A5.2.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_reg_shifted(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20) == 10001 */ &&
      (insn.Bits() & 0x0FF0F090) == 0x01100010 /* $pattern(31:0) == xxxx00010001xxxx0000xxxx0xx1xxxx */)
    return Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20) == 10011 */ &&
      (insn.Bits() & 0x0FF0F090) == 0x01300010 /* $pattern(31:0) == xxxx00010011xxxx0000xxxx0xx1xxxx */)
    return Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20) == 10101 */ &&
      (insn.Bits() & 0x0FF0F090) == 0x01500010 /* $pattern(31:0) == xxxx00010101xxxx0000xxxx0xx1xxxx */)
    return Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20) == 10111 */ &&
      (insn.Bits() & 0x0FF0F090) == 0x01700010 /* $pattern(31:0) == xxxx00010111xxxx0000xxxx0xx1xxxx */)
    return Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20) == 0000x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00000010 /* $pattern(31:0) == xxxx0000000xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20) == 0001x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00200010 /* $pattern(31:0) == xxxx0000001xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20) == 0010x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00400010 /* $pattern(31:0) == xxxx0000010xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20) == 0011x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00600010 /* $pattern(31:0) == xxxx0000011xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20) == 0100x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00800010 /* $pattern(31:0) == xxxx0000100xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20) == 0101x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00A00010 /* $pattern(31:0) == xxxx0000101xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20) == 0110x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00C00010 /* $pattern(31:0) == xxxx0000110xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20) == 0111x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00E00010 /* $pattern(31:0) == xxxx0000111xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20) == 1100x */ &&
      (insn.Bits() & 0x0FE00090) == 0x01800010 /* $pattern(31:0) == xxxx0001100xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(6:5) == 00 */ &&
      (insn.Bits() & 0x0FEF00F0) == 0x01A00010 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxx0001xxxx */)
    return Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x0FEF00F0) == 0x01A00030 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxx0011xxxx */)
    return Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x0FEF00F0) == 0x01A00050 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxx0101xxxx */)
    return Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x0FEF00F0) == 0x01A00070 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxx0111xxxx */)
    return Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x0FE00090) == 0x01C00010 /* $pattern(31:0) == xxxx0001110xxxxxxxxxxxxx0xx1xxxx */)
    return Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */ &&
      (insn.Bits() & 0x0FEF0090) == 0x01E00010 /* $pattern(31:0) == xxxx0001111x0000xxxxxxxx0xx1xxxx */)
    return Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table ext_reg_load_store.
 * Specified by: ('A7.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_ext_reg_load_store(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x01B00000) == 0x00800000 /* opcode(24:20) == 01x00 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0C800A00 /* $pattern(31:0) == xxxx11001x00xxxxxxxx101xxxxxxxxx */)
    return StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x00900000 /* opcode(24:20) == 01x01 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0C900A00 /* $pattern(31:0) == xxxx11001x01xxxxxxxx101xxxxxxxxx */)
    return LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x00A00000 /* opcode(24:20) == 01x10 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0CA00A00 /* $pattern(31:0) == xxxx11001x10xxxxxxxx101xxxxxxxxx */)
    return StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20) == 01x11 */ &&
      (insn.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16) == ~1101 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0CB00A00 /* $pattern(31:0) == xxxx11001x11xxxxxxxx101xxxxxxxxx */)
    return LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20) == 01x11 */ &&
      (insn.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16) == 1101 */ &&
      (insn.Bits() & 0x0FBF0E00) == 0x0CBD0A00 /* $pattern(31:0) == xxxx11001x111101xxxx101xxxxxxxxx */)
    return LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20) == 10x10 */ &&
      (insn.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16) == ~1101 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0D200A00 /* $pattern(31:0) == xxxx11010x10xxxxxxxx101xxxxxxxxx */)
    return StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20) == 10x10 */ &&
      (insn.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16) == 1101 */ &&
      (insn.Bits() & 0x0FBF0E00) == 0x0D2D0A00 /* $pattern(31:0) == xxxx11010x101101xxxx101xxxxxxxxx */)
    return StoreVectorRegisterList_Vpush_355_A1_A2_P696_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01300000 /* opcode(24:20) == 10x11 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0D300A00 /* $pattern(31:0) == xxxx11010x11xxxxxxxx101xxxxxxxxx */)
    return LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* opcode(24:20) == 0010x */)
    return decode_ext_reg_transfers(insn);

  if ((insn.Bits() & 0x01300000) == 0x01000000 /* opcode(24:20) == 1xx00 */ &&
      (insn.Bits() & 0x0F300E00) == 0x0D000A00 /* $pattern(31:0) == xxxx1101xx00xxxxxxxx101xxxxxxxxx */)
    return StoreVectorRegister_Vstr_Rule_400_A1_A2_P786_instance_;

  if ((insn.Bits() & 0x01300000) == 0x01100000 /* opcode(24:20) == 1xx01 */ &&
      (insn.Bits() & 0x0F300E00) == 0x0D100A00 /* $pattern(31:0) == xxxx1101xx01xxxxxxxx101xxxxxxxxx */)
    return LoadVectorRegister_Vldr_Rule_320_A1_A2_P628_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table ext_reg_move.
 * Specified by: ('A7.8 page A7 - 31',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_ext_reg_move(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00000000 /* A(23:21) == 000 */ &&
      (insn.Bits() & 0x0FF00F7F) == 0x0E000A10 /* $pattern(31:0) == xxxx11100000xxxxxxxx1010x0010000 */)
    return MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21) == 111 */ &&
      (insn.Bits() & 0x0FFF0FFF) == 0x0EE10A10 /* $pattern(31:0) == xxxx111011100001xxxx101000010000 */)
    return VfpUsesRegOp_Vmsr_Rule_336_A1_P660_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x00800000) == 0x00000000 /* A(23:21) == 0xx */ &&
      (insn.Bits() & 0x0F900F1F) == 0x0E000B10 /* $pattern(31:0) == xxxx11100xx0xxxxxxxx1011xxx10000 */)
    return MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x00800000) == 0x00800000 /* A(23:21) == 1xx */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:5) == 0x */ &&
      (insn.Bits() & 0x0F900F5F) == 0x0E800B10 /* $pattern(31:0) == xxxx11101xx0xxxxxxxx1011x0x10000 */)
    return DuplicateToVfpRegisters_Vdup_Rule_303_A1_P594_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* L(20:20) == 1 */ &&
      (insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00000000 /* A(23:21) == 000 */ &&
      (insn.Bits() & 0x0FF00F7F) == 0x0E100A10 /* $pattern(31:0) == xxxx11100001xxxxxxxx1010x0010000 */)
    return MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* L(20:20) == 1 */ &&
      (insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21) == 111 */ &&
      (insn.Bits() & 0x0FFF0FFF) == 0x0EF10A10 /* $pattern(31:0) == xxxx111011110001xxxx101000010000 */)
    return VfpMrsOp_Vmrs_Rule_335_A1_P658_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* L(20:20) == 1 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x0F100F1F) == 0x0E100B10 /* $pattern(31:0) == xxxx1110xxx1xxxxxxxx1011xxx10000 */)
    return MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646_instance_;

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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
  return not_implemented_;
}


/*
 * Implementation of table extra_load_store.
 * Specified by: ('See Section A5.2.8',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_extra_load_store(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x000000B0 /* $pattern(31:0) == xxxx000xx0x0xxxxxxxx00001011xxxx */)
    return Store3RegisterOp_Strh_Rule_208_A1_P412_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x001000B0 /* $pattern(31:0) == xxxx000xx0x1xxxxxxxx00001011xxxx */)
    return Load3RegisterOp_Ldrh_Rule_76_A1_P156_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x004000B0 /* $pattern(31:0) == xxxx000xx1x0xxxxxxxxxxxx1011xxxx */)
    return Store2RegisterImm8Op_Strh_Rule_207_A1_P410_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x005000B0 /* $pattern(31:0) == xxxx000xx1x1xxxxxxxxxxxx1011xxxx */)
    return Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F00F0) == 0x015F00B0 /* $pattern(31:0) == xxxx0001x1011111xxxxxxxx1011xxxx */)
    return Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x000000D0 /* $pattern(31:0) == xxxx000xx0x0xxxxxxxx00001101xxxx */)
    return Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x001000D0 /* $pattern(31:0) == xxxx000xx0x1xxxxxxxx00001101xxxx */)
    return Load3RegisterOp_Ldrsb_Rule_80_A1_P164_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x004000D0 /* $pattern(31:0) == xxxx000xx1x0xxxxxxxxxxxx1101xxxx */)
    return Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F00F0) == 0x014F00D0 /* $pattern(31:0) == xxxx0001x1001111xxxxxxxx1101xxxx */)
    return Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x005000D0 /* $pattern(31:0) == xxxx000xx1x1xxxxxxxxxxxx1101xxxx */)
    return Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F00F0) == 0x015F00D0 /* $pattern(31:0) == xxxx0001x1011111xxxxxxxx1101xxxx */)
    return Load2RegisterImm8Op_ldrsb_Rule_79_A1_162_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x000000F0 /* $pattern(31:0) == xxxx000xx0x0xxxxxxxx00001111xxxx */)
    return Store3RegisterDoubleOp_Strd_Rule_201_A1_P398_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x001000F0 /* $pattern(31:0) == xxxx000xx0x1xxxxxxxx00001111xxxx */)
    return Load3RegisterOp_Ldrsh_Rule_84_A1_P172_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x004000F0 /* $pattern(31:0) == xxxx000xx1x0xxxxxxxxxxxx1111xxxx */)
    return Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x005000F0 /* $pattern(31:0) == xxxx000xx1x1xxxxxxxxxxxx1111xxxx */)
    return Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F00F0) == 0x015F00F0 /* $pattern(31:0) == xxxx0001x1011111xxxxxxxx1111xxxx */)
    return Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table half_mult.
 * Specified by: ('See Section A5.2.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_half_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00600000) == 0x00000000 /* op1(22:21) == 00 */ &&
      (insn.Bits() & 0x0FF00090) == 0x01000080 /* $pattern(31:0) == xxxx00010000xxxxxxxxxxxx1xx0xxxx */)
    return Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:21) == 01 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op(5:5) == 0 */ &&
      (insn.Bits() & 0x0FF000B0) == 0x01200080 /* $pattern(31:0) == xxxx00010010xxxxxxxxxxxx1x00xxxx */)
    return Binary4RegisterDualOp_Smlawx_Rule_171_A1_340_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:21) == 01 */ &&
      (insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x0FF000B0) == 0x012000A0 /* $pattern(31:0) == xxxx00010010xxxxxxxxxxxx1x10xxxx */)
    return Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00400000 /* op1(22:21) == 10 */ &&
      (insn.Bits() & 0x0FF00090) == 0x01400080 /* $pattern(31:0) == xxxx00010100xxxxxxxxxxxx1xx0xxxx */)
    return Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op1(22:21) == 11 */ &&
      (insn.Bits() & 0x0FF00090) == 0x01600080 /* $pattern(31:0) == xxxx00010110xxxxxxxxxxxx1xx0xxxx */)
    return Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table load_store_word_byte.
 * Specified by: ('See Section A5.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_load_store_word_byte(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00000000 /* op1(24:20) == 0x000 */ &&
      (insn.Bits() & 0x0E500000) == 0x04000000 /* $pattern(31:0) == xxxx010xx0x0xxxxxxxxxxxxxxxxxxxx */)
    return Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00100000 /* op1(24:20) == 0x001 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E500000) == 0x04100000 /* $pattern(31:0) == xxxx010xx0x1xxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00100000 /* op1(24:20) == 0x001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F0000) == 0x051F0000 /* $pattern(31:0) == xxxx0101x0011111xxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00400000 /* op1(24:20) == 0x100 */ &&
      (insn.Bits() & 0x0E500000) == 0x04400000 /* $pattern(31:0) == xxxx010xx1x0xxxxxxxxxxxxxxxxxxxx */)
    return Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E500000) == 0x04500000 /* $pattern(31:0) == xxxx010xx1x1xxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F0000) == 0x055F0000 /* $pattern(31:0) == xxxx0101x1011111xxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01000000 /* op1(24:20) == 1x0x0 */ &&
      (insn.Bits() & 0x0E500000) == 0x04000000 /* $pattern(31:0) == xxxx010xx0x0xxxxxxxxxxxxxxxxxxxx */)
    return Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E500000) == 0x04100000 /* $pattern(31:0) == xxxx010xx0x1xxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F0000) == 0x051F0000 /* $pattern(31:0) == xxxx0101x0011111xxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01400000 /* op1(24:20) == 1x1x0 */ &&
      (insn.Bits() & 0x0E500000) == 0x04400000 /* $pattern(31:0) == xxxx010xx1x0xxxxxxxxxxxxxxxxxxxx */)
    return Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E500000) == 0x04500000 /* $pattern(31:0) == xxxx010xx1x1xxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F0000) == 0x055F0000 /* $pattern(31:0) == xxxx0101x1011111xxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00000000 /* op1(24:20) == 0x000 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06000000 /* $pattern(31:0) == xxxx011xx0x0xxxxxxxxxxxxxxx0xxxx */)
    return Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00400000 /* op1(24:20) == 0x100 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06400000 /* $pattern(31:0) == xxxx011xx1x0xxxxxxxxxxxxxxx0xxxx */)
    return Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06500000 /* $pattern(31:0) == xxxx011xx1x1xxxxxxxxxxxxxxx0xxxx */)
    return Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x00700000) == 0x00100000 /* op1(24:20) == xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06100000 /* $pattern(31:0) == xxxx011xx0x1xxxxxxxxxxxxxxx0xxxx */)
    return Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01000000 /* op1(24:20) == 1x0x0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06000000 /* $pattern(31:0) == xxxx011xx0x0xxxxxxxxxxxxxxx0xxxx */)
    return Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06100000 /* $pattern(31:0) == xxxx011xx0x1xxxxxxxxxxxxxxx0xxxx */)
    return Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01400000 /* op1(24:20) == 1x1x0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06400000 /* $pattern(31:0) == xxxx011xx1x0xxxxxxxxxxxxxxx0xxxx */)
    return Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06500000 /* $pattern(31:0) == xxxx011xx1x1xxxxxxxxxxxxxxx0xxxx */)
    return Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Forbidden_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table media.
 * Specified by: ('See Section A5.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_media(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20) == 11000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12) == ~1111 */ &&
      (insn.Bits() & 0x0FF000F0) == 0x07800010 /* $pattern(31:0) == xxxx01111000xxxxxxxxxxxx0001xxxx */)
    return Binary4RegisterDualOp_Usda8_Rule_254_A1_P502_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20) == 11000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12) == 1111 */ &&
      (insn.Bits() & 0x0FF0F0F0) == 0x0780F010 /* $pattern(31:0) == xxxx01111000xxxx1111xxxx0001xxxx */)
    return Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01F00000 /* op1(24:20) == 11111 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Roadblock_None_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(7:5) == x10 */ &&
      (insn.Bits() & 0x0FE00070) == 0x07A00050 /* $pattern(31:0) == xxxx0111101xxxxxxxxxxxxxx101xxxx */)
    return Binary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0) == ~1111 */ &&
      (insn.Bits() & 0x0FE00070) == 0x07C00010 /* $pattern(31:0) == xxxx0111110xxxxxxxxxxxxxx001xxxx */)
    return Binary2RegisterBitRange_Bfi_Rule_18_A1_P48_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0) == 1111 */ &&
      (insn.Bits() & 0x0FE0007F) == 0x07C0001F /* $pattern(31:0) == xxxx0111110xxxxxxxxxxxxxx0011111 */)
    return Unary1RegisterBitRange_Bfc_17_A1_P46_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(7:5) == x10 */ &&
      (insn.Bits() & 0x0FE00070) == 0x07E00050 /* $pattern(31:0) == xxxx0111111xxxxxxxxxxxxxx101xxxx */)
    return Binary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466_instance_;

  if ((insn.Bits() & 0x01C00000) == 0x00000000 /* op1(24:20) == 000xx */)
    return decode_parallel_add_sub_signed(insn);

  if ((insn.Bits() & 0x01C00000) == 0x00400000 /* op1(24:20) == 001xx */)
    return decode_parallel_add_sub_unsigned(insn);

  if ((insn.Bits() & 0x01800000) == 0x00800000 /* op1(24:20) == 01xxx */)
    return decode_pack_sat_rev(insn);

  if ((insn.Bits() & 0x01800000) == 0x01000000 /* op1(24:20) == 10xxx */)
    return decode_signed_mult(insn);

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table misc.
 * Specified by: ('See Section A5.2.12',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_misc(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00000000 /* op1(19:16) == xx00 */ &&
      (insn.Bits() & 0x0FF3FFF0) == 0x0120F000 /* $pattern(31:0) == xxxx00010010xx00111100000000xxxx */)
    return Unary1RegisterUse_Msr_Rule_104_A1_P210_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */ &&
      (insn.Bits() & 0x0FF3FFF0) == 0x0121F000 /* $pattern(31:0) == xxxx00010010xx01111100000000xxxx */)
    return ForbiddenCondNop_Msr_Rule_B6_1_7_P14_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */ &&
      (insn.Bits() & 0x0FF2FFF0) == 0x0122F000 /* $pattern(31:0) == xxxx00010010xx1x111100000000xxxx */)
    return ForbiddenCondNop_Msr_Rule_B6_1_7_P14_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      (insn.Bits() & 0x0FF0FFF0) == 0x0160F000 /* $pattern(31:0) == xxxx00010110xxxx111100000000xxxx */)
    return ForbiddenCondNop_Msr_Rule_B6_1_7_P14_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* op(22:21) == x0 */ &&
      (insn.Bits() & 0x0FBF0FFF) == 0x010F0000 /* $pattern(31:0) == xxxx00010x001111xxxx000000000000 */)
    return Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x012FFF10 /* $pattern(31:0) == xxxx000100101111111111110001xxxx */)
    return BranchToRegister_Bx_Rule_25_A1_P62_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x016F0F10 /* $pattern(31:0) == xxxx000101101111xxxx11110001xxxx */)
    return Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000020 /* op2(6:4) == 010 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x012FFF20 /* $pattern(31:0) == xxxx000100101111111111110010xxxx */)
    return ForbiddenCondNop_Bxj_Rule_26_A1_P64_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000030 /* op2(6:4) == 011 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x012FFF30 /* $pattern(31:0) == xxxx000100101111111111110011xxxx */)
    return BranchToRegister_Blx_Rule_24_A1_P60_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000050 /* op2(6:4) == 101 */)
    return decode_sat_add_sub(insn);

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FF000F0) == 0x01200070 /* $pattern(31:0) == xxxx00010010xxxxxxxxxxxx0111xxxx */)
    return BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x01600070 /* $pattern(31:0) == xxxx000101100000000000000111xxxx */)
    return ForbiddenCondNop_Smc_Rule_B6_1_9_P18_instance_;

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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
      (insn.Bits() & 0x000000FF) == 0x00000000 /* op2(7:0) == 00000000 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F000 /* $pattern(31:0) == xxxx0011001000001111000000000000 */)
    return CondNop_Nop_Rule_110_A1_P222_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000001 /* op2(7:0) == 00000001 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F001 /* $pattern(31:0) == xxxx0011001000001111000000000001 */)
    return CondNop_Yield_Rule_413_A1_P812_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000002 /* op2(7:0) == 00000010 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F002 /* $pattern(31:0) == xxxx0011001000001111000000000010 */)
    return ForbiddenCondNop_Wfe_Rule_411_A1_P808_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000003 /* op2(7:0) == 00000011 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F003 /* $pattern(31:0) == xxxx0011001000001111000000000011 */)
    return ForbiddenCondNop_Wfi_Rule_412_A1_P810_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000004 /* op2(7:0) == 00000100 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F004 /* $pattern(31:0) == xxxx0011001000001111000000000100 */)
    return ForbiddenCondNop_Sev_Rule_158_A1_P316_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000F0) == 0x000000F0 /* op2(7:0) == 1111xxxx */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x0320F0F0 /* $pattern(31:0) == xxxx001100100000111100001111xxxx */)
    return CondNop_Dbg_Rule_40_A1_P88_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00040000 /* op1(19:16) == 0100 */)
    return MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000B0000) == 0x00080000 /* op1(19:16) == 1x00 */)
    return MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */ &&
      (insn.Bits() & 0x0FF3F000) == 0x0321F000 /* $pattern(31:0) == xxxx00110010xx011111xxxxxxxxxxxx */)
    return ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */ &&
      (insn.Bits() & 0x0FF2F000) == 0x0322F000 /* $pattern(31:0) == xxxx00110010xx1x1111xxxxxxxxxxxx */)
    return ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00400000 /* op(22:22) == 1 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x0360F000 /* $pattern(31:0) == xxxx00110110xxxx1111xxxxxxxxxxxx */)
    return ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Forbidden_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table mult.
 * Specified by: ('See Section A5.2.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00400000 /* op(23:20) == 0100 */ &&
      (insn.Bits() & 0x0FF000F0) == 0x00400090 /* $pattern(31:0) == xxxx00000100xxxxxxxxxxxx1001xxxx */)
    return Binary4RegisterDualResult_Umaal_Rule_244_A1_P482_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00600000 /* op(23:20) == 0110 */ &&
      (insn.Bits() & 0x0FF000F0) == 0x00600090 /* $pattern(31:0) == xxxx00000110xxxxxxxxxxxx1001xxxx */)
    return Binary4RegisterDualOp_Mls_Rule_95_A1_P192_instance_;

  if ((insn.Bits() & 0x00D00000) == 0x00500000 /* op(23:20) == 01x1 */)
    return Undefined_None_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00000000 /* op(23:20) == 000x */ &&
      (insn.Bits() & 0x0FE0F0F0) == 0x00000090 /* $pattern(31:0) == xxxx0000000xxxxx0000xxxx1001xxxx */)
    return Binary3RegisterOpAltA_Mul_Rule_105_A1_P212_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00200000 /* op(23:20) == 001x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00200090 /* $pattern(31:0) == xxxx0000001xxxxxxxxxxxxx1001xxxx */)
    return Binary4RegisterDualOp_Mla_Rule_94_A1_P190_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00800000 /* op(23:20) == 100x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00800090 /* $pattern(31:0) == xxxx0000100xxxxxxxxxxxxx1001xxxx */)
    return Binary4RegisterDualResult_Umull_Rule_246_A1_P486_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00A00000 /* op(23:20) == 101x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00A00090 /* $pattern(31:0) == xxxx0000101xxxxxxxxxxxxx1001xxxx */)
    return Binary4RegisterDualResult_Umlal_Rule_245_A1_P484_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00C00000 /* op(23:20) == 110x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00C00090 /* $pattern(31:0) == xxxx0000110xxxxxxxxxxxxx1001xxxx */)
    return Binary4RegisterDualResult_Smull_Rule_179_A1_P356_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00E00000 /* op(23:20) == 111x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00E00090 /* $pattern(31:0) == xxxx0000111xxxxxxxxxxxxx1001xxxx */)
    return Binary4RegisterDualResult_Smlal_Rule_168_A1_P334_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table other_vfp_data_proc.
 * Specified by: ('A7.5 Table A7 - 17, page 17 - 25',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_other_vfp_data_proc(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x000F0000) == 0x00000000 /* opc2(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* opc3(7:6) == 01 */ &&
      (insn.Bits() & 0x0FBF0ED0) == 0x0EB00A40 /* $pattern(31:0) == xxxx11101x110000xxxx101x01x0xxxx */)
    return CondVfpOp_Vmov_Rule_327_A2_P642_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00000000 /* opc2(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6) == 11 */)
    return CondVfpOp_Vabs_Rule_269_A2_P532_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16) == 0001 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* opc3(7:6) == 01 */ &&
      (insn.Bits() & 0x0FBF0ED0) == 0x0EB10A40 /* $pattern(31:0) == xxxx11101x110001xxxx101x01x0xxxx */)
    return CondVfpOp_Vneg_Rule_342_A2_P672_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16) == 0001 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6) == 11 */ &&
      (insn.Bits() & 0x0FBF0ED0) == 0x0EB10AC0 /* $pattern(31:0) == xxxx11101x110001xxxx101x11x0xxxx */)
    return CondVfpOp_Vsqrt_Rule_388_A1_P762_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00040000 /* opc2(19:16) == 0100 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBF0E50) == 0x0EB40A40 /* $pattern(31:0) == xxxx11101x110100xxxx101xx1x0xxxx */)
    return CondVfpOp_Vcmp_Vcmpe_Rule_292_A1_P572_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00050000 /* opc2(19:16) == 0101 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBF0E7F) == 0x0EB50A40 /* $pattern(31:0) == xxxx11101x110101xxxx101xx1000000 */)
    return CondVfpOp_Vcmp_Vcmpe_Rule_292_A2_P572_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00070000 /* opc2(19:16) == 0111 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6) == 11 */ &&
      (insn.Bits() & 0x0FBF0ED0) == 0x0EB70AC0 /* $pattern(31:0) == xxxx11101x110111xxxx101x11x0xxxx */)
    return CondVfpOp_Vcvt_Rule_298_A1_P584_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00080000 /* opc2(19:16) == 1000 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBF0E50) == 0x0EB80A40 /* $pattern(31:0) == xxxx11101x111000xxxx101xx1x0xxxx */)
    return CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_;

  if ((insn.Bits() & 0x000E0000) == 0x00020000 /* opc2(19:16) == 001x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBE0F50) == 0x0EB20A40 /* $pattern(31:0) == xxxx11101x11001xxxxx1010x1x0xxxx */)
    return CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588_instance_;

  if ((insn.Bits() & 0x000E0000) == 0x000A0000 /* opc2(19:16) == 101x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBE0E50) == 0x0EBA0A40 /* $pattern(31:0) == xxxx11101x11101xxxxx101xx1x0xxxx */)
    return CondVfpOp_Vcvt_Rule_297_A1_P582_instance_;

  if ((insn.Bits() & 0x000E0000) == 0x000C0000 /* opc2(19:16) == 110x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBE0E50) == 0x0EBC0A40 /* $pattern(31:0) == xxxx11101x11110xxxxx101xx1x0xxxx */)
    return CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_;

  if ((insn.Bits() & 0x000E0000) == 0x000E0000 /* opc2(19:16) == 111x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBE0E50) == 0x0EBE0A40 /* $pattern(31:0) == xxxx11101x11111xxxxx101xx1x0xxxx */)
    return CondVfpOp_Vcvt_Rule_297_A1_P582_instance_;

  if ((insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */ &&
      (insn.Bits() & 0x0FB00EF0) == 0x0EB00A00 /* $pattern(31:0) == xxxx11101x11xxxxxxxx101x0000xxxx */)
    return CondVfpOp_Vmov_Rule_326_A2_P640_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table pack_sat_rev.
 * Specified by: ('See Section A5.4.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_pack_sat_rev(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06800070 /* $pattern(31:0) == xxxx01101000xxxxxxxxxx000111xxxx */)
    return Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x068F0070 /* $pattern(31:0) == xxxx011010001111xxxxxx000111xxxx */)
    return Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06800FB0 /* $pattern(31:0) == xxxx01101000xxxxxxxx11111011xxxx */)
    return Binary3RegisterOpAltB_Sel_Rule_156_A1_P312_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */ &&
      (insn.Bits() & 0x0FF00030) == 0x06800010 /* $pattern(31:0) == xxxx01101000xxxxxxxxxxxxxx01xxxx */)
    return Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00200000 /* op1(22:20) == 010 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06A00F30 /* $pattern(31:0) == xxxx01101010xxxxxxxx11110011xxxx */)
    return Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00200000 /* op1(22:20) == 010 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06A00070 /* $pattern(31:0) == xxxx01101010xxxxxxxxxx000111xxxx */)
    return Binary3RegisterOpAltB_Sxtab_Rule_220_A1_P434_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00200000 /* op1(22:20) == 010 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06AF0070 /* $pattern(31:0) == xxxx011010101111xxxxxx000111xxxx */)
    return Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00300000 /* op1(22:20) == 011 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x06BF0F30 /* $pattern(31:0) == xxxx011010111111xxxx11110011xxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00300000 /* op1(22:20) == 011 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06B00070 /* $pattern(31:0) == xxxx01101011xxxxxxxxxx000111xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00300000 /* op1(22:20) == 011 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06BF0070 /* $pattern(31:0) == xxxx011010111111xxxxxx000111xxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00300000 /* op1(22:20) == 011 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x06BF0FB0 /* $pattern(31:0) == xxxx011010111111xxxx11111011xxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06C00070 /* $pattern(31:0) == xxxx01101100xxxxxxxxxx000111xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P516_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06CF0070 /* $pattern(31:0) == xxxx011011001111xxxxxx000111xxxx */)
    return Unary2RegisterOpNotRmIsPc_Uxtb16_Rule_264_A1_P522_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06E00F30 /* $pattern(31:0) == xxxx01101110xxxxxxxx11110011xxxx */)
    return Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06E00070 /* $pattern(31:0) == xxxx01101110xxxxxxxxxx000111xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06EF0070 /* $pattern(31:0) == xxxx011011101111xxxxxx000111xxxx */)
    return Unary2RegisterOpNotRmIsPc_Uxtb_Rule_263_A1_P520_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x06FF0F30 /* $pattern(31:0) == xxxx011011111111xxxx11110011xxxx */)
    return Unary2RegisterOpNotRmIsPc_Rbit_Rule_134_A1_P270_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06F00070 /* $pattern(31:0) == xxxx01101111xxxxxxxxxx000111xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06FF0070 /* $pattern(31:0) == xxxx011011111111xxxxxx000111xxxx */)
    return Unary2RegisterOpNotRmIsPc_Uxth_Rule_265_A1_P524_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x06FF0FB0 /* $pattern(31:0) == xxxx011011111111xxxx11111011xxxx */)
    return Unary2RegisterOpNotRmIsPc_Revsh_Rule_137_A1_P276_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:20) == 01x */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */ &&
      (insn.Bits() & 0x0FE00030) == 0x06A00010 /* $pattern(31:0) == xxxx0110101xxxxxxxxxxxxxxx01xxxx */)
    return Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op1(22:20) == 11x */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */ &&
      (insn.Bits() & 0x0FE00030) == 0x06E00010 /* $pattern(31:0) == xxxx0110111xxxxxxxxxxxxxxx01xxxx */)
    return Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504_instance_;

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table parallel_add_sub_signed.
 * Specified by: ('See Section A5.4.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_add_sub_signed(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F10 /* $pattern(31:0) == xxxx01100001xxxxxxxx11110001xxxx */)
    return Binary3RegisterOpAltB_Sadd16_Rule_148_A1_P296_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F30 /* $pattern(31:0) == xxxx01100001xxxxxxxx11110011xxxx */)
    return Binary3RegisterOpAltB_Sasx_Rule_150_A1_P300_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F50 /* $pattern(31:0) == xxxx01100001xxxxxxxx11110101xxxx */)
    return Binary3RegisterOpAltB_Ssax_Rule_185_A1_P366_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F70 /* $pattern(31:0) == xxxx01100001xxxxxxxx11110111xxxx */)
    return Binary3RegisterOpAltB_Ssub16_Rule_186_A1_P368_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F90 /* $pattern(31:0) == xxxx01100001xxxxxxxx11111001xxxx */)
    return Binary3RegisterOpAltB_Ssad8_Rule_149_A1_P298_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100FF0 /* $pattern(31:0) == xxxx01100001xxxxxxxx11111111xxxx */)
    return Binary3RegisterOpAltB_Ssub8_Rule_187_A1_P370_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F10 /* $pattern(31:0) == xxxx01100010xxxxxxxx11110001xxxx */)
    return Binary3RegisterOpAltB_Qadd16_Rule_125_A1_P252_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F30 /* $pattern(31:0) == xxxx01100010xxxxxxxx11110011xxxx */)
    return Binary3RegisterOpAltB_Qasx_Rule_127_A1_P256_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F50 /* $pattern(31:0) == xxxx01100010xxxxxxxx11110101xxxx */)
    return Binary3RegisterOpAltB_Qsax_Rule_130_A1_P262_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F70 /* $pattern(31:0) == xxxx01100010xxxxxxxx11110111xxxx */)
    return Binary3RegisterOpAltB_Qsub16_Rule_132_A1_P266_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F90 /* $pattern(31:0) == xxxx01100010xxxxxxxx11111001xxxx */)
    return Binary3RegisterOpAltB_Qadd8_Rule_126_A1_P254_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200FF0 /* $pattern(31:0) == xxxx01100010xxxxxxxx11111111xxxx */)
    return Binary3RegisterOpAltB_Qsub8_Rule_133_A1_P268_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F10 /* $pattern(31:0) == xxxx01100011xxxxxxxx11110001xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F30 /* $pattern(31:0) == xxxx01100011xxxxxxxx11110011xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F50 /* $pattern(31:0) == xxxx01100011xxxxxxxx11110101xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F70 /* $pattern(31:0) == xxxx01100011xxxxxxxx11110111xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F90 /* $pattern(31:0) == xxxx01100011xxxxxxxx11111001xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300FF0 /* $pattern(31:0) == xxxx01100011xxxxxxxx11111111xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_;

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table parallel_add_sub_unsigned.
 * Specified by: ('See Section A5.4.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_add_sub_unsigned(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F10 /* $pattern(31:0) == xxxx01100101xxxxxxxx11110001xxxx */)
    return Binary3RegisterOpAltB_Uadd16_Rule_233_A1_P460_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F30 /* $pattern(31:0) == xxxx01100101xxxxxxxx11110011xxxx */)
    return Binary3RegisterOpAltB_Uasx_Rule_235_A1_P464_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F50 /* $pattern(31:0) == xxxx01100101xxxxxxxx11110101xxxx */)
    return Binary3RegisterOpAltB_Usax_Rule_257_A1_P508_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F70 /* $pattern(31:0) == xxxx01100101xxxxxxxx11110111xxxx */)
    return Binary3RegisterOpAltB_Usub16_Rule_258_A1_P510_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F90 /* $pattern(31:0) == xxxx01100101xxxxxxxx11111001xxxx */)
    return Binary3RegisterOpAltB_Uadd8_Rule_234_A1_P462_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500FF0 /* $pattern(31:0) == xxxx01100101xxxxxxxx11111111xxxx */)
    return Binary3RegisterOpAltB_Usub8_Rule_259_A1_P512_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F10 /* $pattern(31:0) == xxxx01100110xxxxxxxx11110001xxxx */)
    return Binary3RegisterOpAltB_Uqadd16_Rule_247_A1_P488_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F30 /* $pattern(31:0) == xxxx01100110xxxxxxxx11110011xxxx */)
    return Binary3RegisterOpAltB_Uqasx_Rule_249_A1_P492_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F50 /* $pattern(31:0) == xxxx01100110xxxxxxxx11110101xxxx */)
    return Binary3RegisterOpAltB_Uqsax_Rule_250_A1_P494_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F70 /* $pattern(31:0) == xxxx01100110xxxxxxxx11110111xxxx */)
    return Binary3RegisterOpAltB_Uqsub16_Rule_251_A1_P496_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F90 /* $pattern(31:0) == xxxx01100110xxxxxxxx11111001xxxx */)
    return Binary3RegisterOpAltB_Uqadd8_Rule_248_A1_P490_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600FF0 /* $pattern(31:0) == xxxx01100110xxxxxxxx11111111xxxx */)
    return Binary3RegisterOpAltB_Uqsub8_Rule_252_A1_P498_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F10 /* $pattern(31:0) == xxxx01100111xxxxxxxx11110001xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F30 /* $pattern(31:0) == xxxx01100111xxxxxxxx11110011xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F50 /* $pattern(31:0) == xxxx01100111xxxxxxxx11110101xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F70 /* $pattern(31:0) == xxxx01100111xxxxxxxx11110111xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F90 /* $pattern(31:0) == xxxx01100111xxxxxxxx11111001xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700FF0 /* $pattern(31:0) == xxxx01100111xxxxxxxx11111111xxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480_instance_;

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table sat_add_sub.
 * Specified by: ('See Section A5.2.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_sat_add_sub(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00600000) == 0x00000000 /* op(22:21) == 00 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01000050 /* $pattern(31:0) == xxxx00010000xxxxxxxx00000101xxxx */)
    return Binary3RegisterOpAltB_Qadd_Rule_124_A1_P250_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01200050 /* $pattern(31:0) == xxxx00010010xxxxxxxx00000101xxxx */)
    return Binary3RegisterOpAltB_Qsub_Rule_131_A1_P264_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00400000 /* op(22:21) == 10 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01400050 /* $pattern(31:0) == xxxx00010100xxxxxxxx00000101xxxx */)
    return Binary3RegisterOpAltB_Qdadd_Rule_128_A1_P258_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01600050 /* $pattern(31:0) == xxxx00010110xxxxxxxx00000101xxxx */)
    return Binary3RegisterOpAltB_Qdsub_Rule_129_A1_P260_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table signed_mult.
 * Specified by: ('See Section A5.4.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_signed_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07000010 /* $pattern(31:0) == xxxx01110000xxxxxxxxxxxx00x1xxxx */)
    return Binary4RegisterDualOp_Smlad_Rule_167_P332_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */ &&
      (insn.Bits() & 0x0FF0F0D0) == 0x0700F010 /* $pattern(31:0) == xxxx01110000xxxx1111xxxx00x1xxxx */)
    return Binary3RegisterOpAltA_Smuad_Rule_177_P352_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07000050 /* $pattern(31:0) == xxxx01110000xxxxxxxxxxxx01x1xxxx */)
    return Binary4RegisterDualOp_Smlsd_Rule_172_P342_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */ &&
      (insn.Bits() & 0x0FF0F0D0) == 0x0700F050 /* $pattern(31:0) == xxxx01110000xxxx1111xxxx01x1xxxx */)
    return Binary3RegisterOpAltA_Smusd_Rule_181_P360_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07400010 /* $pattern(31:0) == xxxx01110100xxxxxxxxxxxx00x1xxxx */)
    return Binary4RegisterDualResult_Smlald_Rule_170_P336_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07400050 /* $pattern(31:0) == xxxx01110100xxxxxxxxxxxx01x1xxxx */)
    return Binary4RegisterDualResult_Smlsld_Rule_173_P344_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07500010 /* $pattern(31:0) == xxxx01110101xxxxxxxxxxxx00x1xxxx */)
    return Binary4RegisterDualOp_Smmla_Rule_174_P346_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */ &&
      (insn.Bits() & 0x0FF0F0D0) == 0x0750F010 /* $pattern(31:0) == xxxx01110101xxxx1111xxxx00x1xxxx */)
    return Binary3RegisterOpAltA_Smmul_Rule_176_P350_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* op2(7:5) == 11x */ &&
      (insn.Bits() & 0x0FF000D0) == 0x075000D0 /* $pattern(31:0) == xxxx01110101xxxxxxxxxxxx11x1xxxx */)
    return Binary4RegisterDualOp_Smmls_Rule_175_P348_instance_;

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
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

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table super_cop.
 * Specified by: ('See Section A5.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_super_cop(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x03E00000) == 0x00000000 /* op1(25:20) == 00000x */)
    return Undefined_None_instance_;

  if ((insn.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20) == 00010x */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20) == 00010x */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */)
    return decode_ext_reg_transfers(insn);

  if ((insn.Bits() & 0x03100000) == 0x02100000 /* op1(25:20) == 10xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00000000 /* op1(25:20) == 0xxxx0 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00100000 /* op1(25:20) == 0xxxx1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00100000 /* op1(25:20) == 0xxxx1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Forbidden_None_instance_;

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return Forbidden_None_instance_;

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
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */)
    return decode_ext_reg_load_store(insn);

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table sync.
 * Specified by: ('See Section A5.2.10',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_sync(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00800000 /* op(23:20) == 1000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01800F90 /* $pattern(31:0) == xxxx00011000xxxxxxxx11111001xxxx */)
    return StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00900000 /* op(23:20) == 1001 */ &&
      (insn.Bits() & 0x0FF00FFF) == 0x01900F9F /* $pattern(31:0) == xxxx00011001xxxxxxxx111110011111 */)
    return LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00A00000 /* op(23:20) == 1010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01A00F90 /* $pattern(31:0) == xxxx00011010xxxxxxxx11111001xxxx */)
    return StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00B00000 /* op(23:20) == 1011 */ &&
      (insn.Bits() & 0x0FF00FFF) == 0x01B00F9F /* $pattern(31:0) == xxxx00011011xxxxxxxx111110011111 */)
    return LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00C00000 /* op(23:20) == 1100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01C00F90 /* $pattern(31:0) == xxxx00011100xxxxxxxx11111001xxxx */)
    return StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00D00000 /* op(23:20) == 1101 */ &&
      (insn.Bits() & 0x0FF00FFF) == 0x01D00F9F /* $pattern(31:0) == xxxx00011101xxxxxxxx111110011111 */)
    return LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00E00000 /* op(23:20) == 1110 */)
    return StoreExclusive3RegisterOp_cccc00011110nnnndddd11111001tttt_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00F00000 /* op(23:20) == 1111 */ &&
      (insn.Bits() & 0x0FF00FFF) == 0x01F00F9F /* $pattern(31:0) == xxxx00011111xxxxxxxx111110011111 */)
    return LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00000000 /* op(23:20) == 0x00 */)
    return Deprecated_None_instance_;

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table unconditional.
 * Specified by: ('See Section A5.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_unconditional(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x0FB00000) == 0x0C200000 /* op1(27:20) == 11000x10 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0FB00000) == 0x0C300000 /* op1(27:20) == 11000x11 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0FE00000) == 0x0C400000 /* op1(27:20) == 1100010x */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0F900000) == 0x0C800000 /* op1(27:20) == 11001xx0 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0F900000) == 0x0C900000 /* op1(27:20) == 11001xx1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0E500000) == 0x08100000 /* op1(27:20) == 100xx0x1 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0E500000) == 0x08400000 /* op1(27:20) == 100xx1x0 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0D000000 /* op1(27:20) == 1101xxx0 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0D100000 /* op1(27:20) == 1101xxx1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20) == 1110xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */)
    return CoprocessorOp_None_instance_;

  if ((insn.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20) == 1110xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return ForbiddenUncondNop_None_instance_;

  if ((insn.Bits() & 0x0E000000) == 0x0A000000 /* op1(27:20) == 101xxxxx */ &&
      (insn.Bits() & 0xFE000000) == 0xFA000000 /* $pattern(31:0) == 1111101xxxxxxxxxxxxxxxxxxxxxxxxx */)
    return ForbiddenUncondNop_Blx_Rule_23_A2_P58_instance_;

  if ((insn.Bits() & 0x08000000) == 0x00000000 /* op1(27:20) == 0xxxxxxx */)
    return decode_misc_hints_simd(insn);

  if (true &&
      true /* $pattern(31:0) == xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table vfp_data_proc.
 * Specified by: ('A7.5 Table A7 - 16, page A7 - 24',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_vfp_data_proc(
     const nacl_arm_dec::Instruction insn) const {

  if ((insn.Bits() & 0x00B00000) == 0x00000000 /* opc1(23:20) == 0x00 */ &&
      (insn.Bits() & 0x0FB00E10) == 0x0E000A00 /* $pattern(31:0) == xxxx11100x00xxxxxxxx101xxxx0xxxx */)
    return CondVfpOp_Vm_la_ls_Rule_423_A2_P636_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00100000 /* opc1(23:20) == 0x01 */ &&
      (insn.Bits() & 0x0FB00E10) == 0x0E100A00 /* $pattern(31:0) == xxxx11100x01xxxxxxxx101xxxx0xxxx */)
    return CondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00200000 /* opc1(23:20) == 0x10 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E200A00 /* $pattern(31:0) == xxxx11100x10xxxxxxxx101xx0x0xxxx */)
    return CondVfpOp_Vmul_Rule_338_A2_P664_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00200000 /* opc1(23:20) == 0x10 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E200A40 /* $pattern(31:0) == xxxx11100x10xxxxxxxx101xx1x0xxxx */)
    return CondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20) == 0x11 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E300A00 /* $pattern(31:0) == xxxx11100x11xxxxxxxx101xx0x0xxxx */)
    return CondVfpOp_Vadd_Rule_271_A2_P536_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20) == 0x11 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E300A40 /* $pattern(31:0) == xxxx11100x11xxxxxxxx101xx1x0xxxx */)
    return CondVfpOp_Vsub_Rule_402_A2_P790_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00800000 /* opc1(23:20) == 1x00 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E800A00 /* $pattern(31:0) == xxxx11101x00xxxxxxxx101xx0x0xxxx */)
    return CondVfpOp_Vdiv_Rule_301_A1_P590_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00B00000 /* opc1(23:20) == 1x11 */)
    return decode_other_vfp_data_proc(insn);

  // Catch any attempt to fall through...
  return not_implemented_;
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
