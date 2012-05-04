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

  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0C000000) == 0x00000000) && (true)) {
   return decode_dp_misc(insn);
  }

  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0C000000) == 0x0C000000) && (true)) {
   return decode_super_cop(insn);
  }

  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0E000000) == 0x06000000) && ((insn & 0x00000010) == 0x00000010)) {
   return decode_media(insn);
  }

  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0E000000) == 0x04000000) && (true)) {
   return decode_load_store_word_byte(insn);
  }

  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0E000000) == 0x06000000) && ((insn & 0x00000010) == 0x00000000)) {
   return decode_load_store_word_byte(insn);
  }

  if (((insn & 0xF0000000) != 0xF0000000) && ((insn & 0x0C000000) == 0x08000000) && (true)) {
   return decode_branch_block_xfer(insn);
  }

  if (((insn & 0xF0000000) == 0xF0000000) && (true) && (true)) {
   return decode_unconditional(insn);
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: ARMv7 could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table branch_block_xfer.
 * Specified by: ('See Section A5.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_branch_block_xfer(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x02500000) == 0x00100000)) {
   return LoadMultiple_None_instance_;
  }

  if (((insn & 0x02000000) == 0x02000000)) {
   return Branch_None_instance_;
  }

  if (((insn & 0x02500000) == 0x00000000)) {
   return StoreImmediate_None_instance_;
  }

  if (((insn & 0x02400000) == 0x00400000)) {
   return Forbidden_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: branch_block_xfer could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table dp_immed.
 * Specified by: ('See Section A5.2.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_immed(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x01E00000) == 0x01E00000)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x01C00000) == 0x00000000)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x01400000) == 0x00400000)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x00C00000) == 0x00800000)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x01F00000) == 0x01500000)) {
   return Test_None_instance_;
  }

  if (((insn & 0x01B00000) == 0x01300000)) {
   return Test_None_instance_;
  }

  if (((insn & 0x01E00000) == 0x01C00000)) {
   return MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_;
  }

  if (((insn & 0x01F00000) == 0x01100000)) {
   return MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_immed could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table dp_misc.
 * Specified by: ('See Section A5.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_misc(
     const nacl_arm_dec::Instruction insn) const {

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01900000) != 0x01000000) && ((insn & 0x00000010) == 0x00000000)) {
   return decode_dp_reg(insn);
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01900000) == 0x01000000) && ((insn & 0x00000080) == 0x00000000)) {
   return decode_misc(insn);
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01900000) != 0x01000000) && (true)) {
   return decode_dp_immed(insn);
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01900000) == 0x01000000) && ((insn & 0x00000090) == 0x00000080)) {
   return decode_half_mult(insn);
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01F00000) == 0x01400000) && (true)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01F00000) == 0x01000000) && (true)) {
   return Unary1RegisterImmediateOp_Mov_Rule_96_A2_P_194_instance_;
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) == 0x00200000) && ((insn & 0x000000F0) == 0x000000B0)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) == 0x00200000) && ((insn & 0x000000D0) == 0x000000D0)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) != 0x00200000) && ((insn & 0x000000F0) == 0x000000B0)) {
   return decode_extra_load_store(insn);
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) != 0x00200000) && ((insn & 0x000000D0) == 0x000000D0)) {
   return decode_extra_load_store(insn);
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01B00000) == 0x01200000) && (true)) {
   return decode_msr_and_hints(insn);
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01000000) == 0x01000000) && ((insn & 0x000000F0) == 0x00000090)) {
   return decode_sync(insn);
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01900000) != 0x01000000) && ((insn & 0x00000090) == 0x00000010)) {
   return decode_dp_reg_shifted(insn);
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01000000) == 0x00000000) && ((insn & 0x000000F0) == 0x00000090)) {
   return decode_mult(insn);
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_misc could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table dp_reg.
 * Specified by: ('See Section A5.2.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_reg(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x01F00000) == 0x01100000) && (true) && (true)) {
   return Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && ((insn & 0x00000F80) == 0x00000000) && ((insn & 0x00000060) == 0x00000000)) {
   return Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_;
  }

  if (((insn & 0x01E00000) == 0x01800000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_;
  }

  if (((insn & 0x01F00000) == 0x01300000) && (true) && (true)) {
   return Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_;
  }

  if (((insn & 0x01E00000) == 0x00200000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_;
  }

  if (((insn & 0x01E00000) == 0x00600000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_;
  }

  if (((insn & 0x01E00000) == 0x01E00000) && (true) && (true)) {
   return Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && ((insn & 0x00000F80) == 0x00000000) && ((insn & 0x00000060) == 0x00000060)) {
   return Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_;
  }

  if (((insn & 0x01E00000) == 0x00C00000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && ((insn & 0x00000F80) != 0x00000000) && ((insn & 0x00000060) == 0x00000000)) {
   return Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_;
  }

  if (((insn & 0x01E00000) == 0x00800000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_;
  }

  if (((insn & 0x01E00000) == 0x00A00000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && (true) && ((insn & 0x00000060) == 0x00000040)) {
   return Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_;
  }

  if (((insn & 0x01E00000) == 0x01C00000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_;
  }

  if (((insn & 0x01E00000) == 0x00400000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_SubRule_213_A1_P422_instance_;
  }

  if (((insn & 0x01F00000) == 0x01500000) && (true) && (true)) {
   return Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_;
  }

  if (((insn & 0x01F00000) == 0x01700000) && (true) && (true)) {
   return Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && (true) && ((insn & 0x00000060) == 0x00000020)) {
   return Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_;
  }

  if (((insn & 0x01E00000) == 0x00000000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && ((insn & 0x00000F80) != 0x00000000) && ((insn & 0x00000060) == 0x00000060)) {
   return Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_;
  }

  if (((insn & 0x01E00000) == 0x00E00000) && (true) && (true)) {
   return Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_reg could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table dp_reg_shifted.
 * Specified by: ('See Section A5.2.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_dp_reg_shifted(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x01E00000) == 0x01E00000) && (true)) {
   return Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_;
  }

  if (((insn & 0x01E00000) == 0x00A00000) && (true)) {
   return Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_;
  }

  if (((insn & 0x01F00000) == 0x01100000) && (true)) {
   return Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_;
  }

  if (((insn & 0x01E00000) == 0x00800000) && (true)) {
   return Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_;
  }

  if (((insn & 0x01F00000) == 0x01300000) && (true)) {
   return Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_;
  }

  if (((insn & 0x01E00000) == 0x00000000) && (true)) {
   return Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && ((insn & 0x00000060) == 0x00000060)) {
   return Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && ((insn & 0x00000060) == 0x00000020)) {
   return Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && ((insn & 0x00000060) == 0x00000000)) {
   return Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_;
  }

  if (((insn & 0x01E00000) == 0x01A00000) && ((insn & 0x00000060) == 0x00000040)) {
   return Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_;
  }

  if (((insn & 0x01F00000) == 0x01700000) && (true)) {
   return Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_;
  }

  if (((insn & 0x01E00000) == 0x00C00000) && (true)) {
   return Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_;
  }

  if (((insn & 0x01E00000) == 0x01800000) && (true)) {
   return Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_;
  }

  if (((insn & 0x01F00000) == 0x01500000) && (true)) {
   return Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_;
  }

  if (((insn & 0x01E00000) == 0x00200000) && (true)) {
   return Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_;
  }

  if (((insn & 0x01E00000) == 0x00400000) && (true)) {
   return Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_;
  }

  if (((insn & 0x01E00000) == 0x00600000) && (true)) {
   return Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_;
  }

  if (((insn & 0x01E00000) == 0x00E00000) && (true)) {
   return Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_;
  }

  if (((insn & 0x01E00000) == 0x01C00000) && (true)) {
   return Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_reg_shifted could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table extra_load_store.
 * Specified by: ('See Section A5.2.8',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_extra_load_store(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00000060) == 0x00000040) && ((insn & 0x00500000) == 0x00400000)) {
   return LoadDoubleI_None_instance_;
  }

  if (((insn & 0x00000060) == 0x00000040) && ((insn & 0x00500000) == 0x00100000)) {
   return LoadRegister_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00500000) == 0x00100000)) {
   return LoadRegister_None_instance_;
  }

  if (((insn & 0x00000060) == 0x00000040) && ((insn & 0x00500000) == 0x00500000)) {
   return LoadImmediate_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00500000) == 0x00500000)) {
   return LoadImmediate_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00500000) == 0x00400000)) {
   return StoreImmediate_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00500000) == 0x00000000)) {
   return StoreRegister_None_instance_;
  }

  if (((insn & 0x00000060) == 0x00000040) && ((insn & 0x00500000) == 0x00000000)) {
   return LoadDoubleR_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: extra_load_store could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table half_mult.
 * Specified by: ('See Section A5.2.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_half_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00600000) == 0x00400000)) {
   return LongMultiply_None_instance_;
  }

  if (((insn & 0x00600000) == 0x00600000)) {
   return Multiply_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000)) {
   return Multiply_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: half_mult could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table load_store_word_byte.
 * Specified by: ('See Section A5.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_load_store_word_byte(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01100000) == 0x01100000) && (true)) {
   return LoadImmediate_None_instance_;
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01300000) == 0x00100000) && (true)) {
   return LoadImmediate_None_instance_;
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01100000) == 0x01000000) && (true)) {
   return StoreImmediate_None_instance_;
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01300000) == 0x00000000) && (true)) {
   return StoreImmediate_None_instance_;
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x00700000) == 0x00100000) && ((insn & 0x00000010) == 0x00000000)) {
   return LoadRegister_None_instance_;
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01700000) == 0x00500000) && ((insn & 0x00000010) == 0x00000000)) {
   return LoadRegister_None_instance_;
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01100000) == 0x01100000) && ((insn & 0x00000010) == 0x00000000)) {
   return LoadRegister_None_instance_;
  }

  if (((insn & 0x02000000) == 0x00000000) && ((insn & 0x01200000) == 0x00200000) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01200000) == 0x00200000) && ((insn & 0x00000010) == 0x00000000)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01100000) == 0x01000000) && ((insn & 0x00000010) == 0x00000000)) {
   return StoreRegister_None_instance_;
  }

  if (((insn & 0x02000000) == 0x02000000) && ((insn & 0x01300000) == 0x00000000) && ((insn & 0x00000010) == 0x00000000)) {
   return StoreRegister_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: load_store_word_byte could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table media.
 * Specified by: ('See Section A5.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_media(
     const nacl_arm_dec::Instruction insn) const {

  if (((insn & 0x01800000) == 0x00800000) && (true)) {
   return decode_pack_sat_rev(insn);
  }

  if (((insn & 0x01800000) == 0x00000000) && (true)) {
   return decode_parallel_add_sub(insn);
  }

  if (((insn & 0x01800000) == 0x01000000) && (true)) {
   return decode_signed_mult(insn);
  }

  if (((insn & 0x01A00000) == 0x01A00000) && ((insn & 0x00000060) == 0x00000040)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x01E00000) == 0x01C00000) && ((insn & 0x00000060) == 0x00000000)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x01F00000) == 0x01F00000) && ((insn & 0x000000E0) == 0x000000E0)) {
   return Roadblock_None_instance_;
  }

  if (((insn & 0x01F00000) == 0x01800000) && ((insn & 0x000000E0) == 0x00000000)) {
   return Multiply_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: media could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table misc.
 * Specified by: ('See Section A5.2.12',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_misc(
     const nacl_arm_dec::Instruction insn) const {

  if (((insn & 0x00000070) == 0x00000010) && ((insn & 0x00600000) == 0x00200000) && (true)) {
   return BxBlx_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000010) && ((insn & 0x00600000) == 0x00600000) && (true)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00600000) == 0x00200000) && ((insn & 0x00030000) == 0x00000000)) {
   return MoveToStatusRegister_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00200000) == 0x00000000) && (true)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000050) && (true) && (true)) {
   return decode_sat_add_sub(insn);
  }

  if (((insn & 0x00000070) == 0x00000030) && ((insn & 0x00600000) == 0x00200000) && (true)) {
   return BxBlx_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00600000) == 0x00200000) && ((insn & 0x00030000) == 0x00010000)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00600000) == 0x00200000) && ((insn & 0x00020000) == 0x00020000)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000000) && ((insn & 0x00600000) == 0x00600000) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000020) && ((insn & 0x00600000) == 0x00200000) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000070) && ((insn & 0x00600000) == 0x00600000) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x00000070) == 0x00000070) && ((insn & 0x00600000) == 0x00200000) && (true)) {
   return Breakpoint_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: misc could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table misc_hints_simd.
 * Specified by: ('See Section A5.7.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_misc_hints_simd(
     const nacl_arm_dec::Instruction insn) const {

  if (((insn & 0x06000000) == 0x02000000) && (true) && (true)) {
   return decode_simd_dp(insn);
  }

  if (((insn & 0x07700000) == 0x04500000) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07F00000) == 0x05700000) && ((insn & 0x000000F0) == 0x00000050) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07700000) == 0x06500000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07F00000) == 0x05700000) && ((insn & 0x000000D0) == 0x00000040) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07100000) == 0x04000000) && (true) && (true)) {
   return decode_simd_load_store(insn);
  }

  if (((insn & 0x07700000) == 0x05100000) && (true) && ((insn & 0x000F0000) == 0x000F0000)) {
   return Unpredictable_None_instance_;
  }

  if (((insn & 0x06300000) == 0x04300000) && (true) && (true)) {
   return Unpredictable_None_instance_;
  }

  if (((insn & 0x06300000) == 0x06300000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
   return Unpredictable_None_instance_;
  }

  if (((insn & 0x07700000) == 0x07500000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07700000) == 0x05500000) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07F00000) == 0x05700000) && ((insn & 0x000000F0) == 0x00000010) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07700000) == 0x04100000) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07700000) == 0x05100000) && (true) && ((insn & 0x000F0000) != 0x000F0000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x06700000) == 0x06100000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x07F00000) == 0x01000000) && ((insn & 0x00000020) == 0x00000000) && ((insn & 0x00010000) == 0x00000000)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x07F00000) == 0x01000000) && ((insn & 0x000000F0) == 0x00000000) && ((insn & 0x00010000) == 0x00010000)) {
   return Forbidden_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: misc_hints_simd could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table msr_and_hints.
 * Specified by: ('See Section A5.2.11',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_msr_and_hints(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000F0) == 0x000000F0)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000FF) == 0x00000002)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000FF) == 0x00000004)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000FD) == 0x00000001)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00000000) && ((insn & 0x000000FF) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x00030000) == 0x00010000) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x00020000) == 0x00020000) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00400000) && (true) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000F0000) == 0x00040000) && (true)) {
   return MoveToStatusRegister_None_instance_;
  }

  if (((insn & 0x00400000) == 0x00000000) && ((insn & 0x000B0000) == 0x00080000) && (true)) {
   return MoveToStatusRegister_None_instance_;
  }

  if ((true)) {
   return Forbidden_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: msr_and_hints could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table mult.
 * Specified by: ('See Section A5.2.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00C00000) == 0x00000000)) {
   return Multiply_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00800000)) {
   return LongMultiply_None_instance_;
  }

  if (((insn & 0x00D00000) == 0x00500000)) {
   return Undefined_None_instance_;
  }

  if (((insn & 0x00F00000) == 0x00400000)) {
   return LongMultiply_None_instance_;
  }

  if (((insn & 0x00F00000) == 0x00600000)) {
   return Multiply_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: mult could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table pack_sat_rev.
 * Specified by: ('See Section A5.4.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_pack_sat_rev(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00700000) == 0x00700000) && ((insn & 0x000000E0) == 0x00000020)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00700000) == 0x00000000) && ((insn & 0x00000020) == 0x00000000)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00700000) == 0x00000000) && ((insn & 0x000000E0) == 0x000000A0)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00200000) == 0x00200000) && ((insn & 0x00000020) == 0x00000000)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00600000) == 0x00200000) && ((insn & 0x000000E0) == 0x00000020)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00300000) == 0x00300000) && ((insn & 0x000000E0) == 0x000000A0)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00700000) == 0x00400000) && ((insn & 0x000000E0) == 0x00000060)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00700000) == 0x00600000) && ((insn & 0x000000A0) == 0x00000020)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00500000) == 0x00000000) && ((insn & 0x000000E0) == 0x00000060)) {
   return PackSatRev_None_instance_;
  }

  if (((insn & 0x00300000) == 0x00300000) && ((insn & 0x000000E0) == 0x00000060)) {
   return PackSatRev_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: pack_sat_rev could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table parallel_add_sub.
 * Specified by: ('See Sections A5.4.1, A5.4.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_add_sub(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00300000) == 0x00200000) && ((insn & 0x000000E0) == 0x00000080)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x00300000) == 0x00200000) && ((insn & 0x000000E0) == 0x000000E0)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x00100000) == 0x00100000) && ((insn & 0x000000E0) == 0x00000080)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x00100000) == 0x00100000) && ((insn & 0x000000E0) == 0x000000E0)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x00300000) == 0x00200000) && ((insn & 0x00000080) == 0x00000000)) {
   return DataProc_None_instance_;
  }

  if (((insn & 0x00100000) == 0x00100000) && ((insn & 0x00000080) == 0x00000000)) {
   return DataProc_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: parallel_add_sub could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table sat_add_sub.
 * Specified by: ('See Section A5.2.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_sat_add_sub(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if ((true)) {
   return SatAddSub_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: sat_add_sub could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table signed_mult.
 * Specified by: ('See Section A5.4.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_signed_mult(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00700000) == 0x00400000) && ((insn & 0x00000080) == 0x00000000) && (true)) {
   return LongMultiply_None_instance_;
  }

  if (((insn & 0x00700000) == 0x00000000) && ((insn & 0x000000C0) == 0x00000040) && ((insn & 0x0000F000) != 0x0000F000)) {
   return Multiply_None_instance_;
  }

  if (((insn & 0x00700000) == 0x00500000) && ((insn & 0x000000C0) == 0x000000C0) && (true)) {
   return Multiply_None_instance_;
  }

  if (((insn & 0x00700000) == 0x00000000) && ((insn & 0x00000080) == 0x00000000) && (true)) {
   return Multiply_None_instance_;
  }

  if (((insn & 0x00700000) == 0x00500000) && ((insn & 0x000000C0) == 0x00000000) && (true)) {
   return Multiply_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: signed_mult could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp.
 * Specified by: ('See Section A7.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp(
     const nacl_arm_dec::Instruction insn) const {

  if ((true) && ((insn & 0x00A00000) == 0x00800000) && (true) && ((insn & 0x00000050) == 0x00000000)) {
   return decode_simd_dp_3diff(insn);
  }

  if ((true) && ((insn & 0x00B00000) == 0x00A00000) && (true) && ((insn & 0x00000050) == 0x00000000)) {
   return decode_simd_dp_3diff(insn);
  }

  if ((true) && ((insn & 0x00800000) == 0x00000000) && (true) && (true)) {
   return decode_simd_dp_3same(insn);
  }

  if ((true) && ((insn & 0x00B80000) == 0x00880000) && (true) && ((insn & 0x00000090) == 0x00000010)) {
   return decode_simd_dp_2shift(insn);
  }

  if ((true) && ((insn & 0x00B00000) == 0x00900000) && (true) && ((insn & 0x00000090) == 0x00000010)) {
   return decode_simd_dp_2shift(insn);
  }

  if ((true) && ((insn & 0x00A00000) == 0x00A00000) && (true) && ((insn & 0x00000090) == 0x00000010)) {
   return decode_simd_dp_2shift(insn);
  }

  if ((true) && ((insn & 0x00800000) == 0x00800000) && (true) && ((insn & 0x00000090) == 0x00000090)) {
   return decode_simd_dp_2shift(insn);
  }

  if ((true) && ((insn & 0x00A00000) == 0x00800000) && (true) && ((insn & 0x00000050) == 0x00000040)) {
   return decode_simd_dp_2scalar(insn);
  }

  if ((true) && ((insn & 0x00B00000) == 0x00A00000) && (true) && ((insn & 0x00000050) == 0x00000040)) {
   return decode_simd_dp_2scalar(insn);
  }

  if ((true) && ((insn & 0x00B80000) == 0x00800000) && (true) && ((insn & 0x00000090) == 0x00000010)) {
   return decode_simd_dp_1imm(insn);
  }

  if (((insn & 0x01000000) == 0x00000000) && ((insn & 0x00B00000) == 0x00B00000) && (true) && ((insn & 0x00000010) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x01000000) == 0x01000000) && ((insn & 0x00B00000) == 0x00B00000) && ((insn & 0x00000C00) == 0x00000800) && ((insn & 0x00000010) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x01000000) == 0x01000000) && ((insn & 0x00B00000) == 0x00B00000) && ((insn & 0x00000F00) == 0x00000C00) && ((insn & 0x00000090) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x01000000) == 0x01000000) && ((insn & 0x00B00000) == 0x00B00000) && ((insn & 0x00000800) == 0x00000000) && ((insn & 0x00000010) == 0x00000000)) {
   return decode_simd_dp_2misc(insn);
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_1imm.
 * Specified by: ('See Section A7.4.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_1imm(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000F00) == 0x00000F00)) {
   return Undefined_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000E00) == 0x00000C00)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000F00) == 0x00000E00)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000800) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000020) && ((insn & 0x00000C00) == 0x00000800)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000020) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_1imm could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_2misc.
 * Specified by: ('See Section A7.4.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2misc(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00030000) == 0x00000000) && ((insn & 0x00000780) == 0x00000700)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00010000) && ((insn & 0x00000380) == 0x00000380)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00020000) && ((insn & 0x000007C0) == 0x00000300)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00020000) && ((insn & 0x000006C0) == 0x00000600)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00000000) && ((insn & 0x00000380) == 0x00000100)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00000000) && ((insn & 0x00000580) == 0x00000580)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00010000) && ((insn & 0x00000280) == 0x00000200)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00020000) && ((insn & 0x00000700) == 0x00000200)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00030000) && ((insn & 0x00000400) == 0x00000400)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00000000) && ((insn & 0x00000100) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00010000) && ((insn & 0x00000200) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00030000) == 0x00020000) && ((insn & 0x00000600) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2misc could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_2scalar.
 * Specified by: ('See Section A7.4.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2scalar(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00000B00) == 0x00000200) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000B00) == 0x00000300) && ((insn & 0x01000000) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000A00) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000B00) && ((insn & 0x01000000) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000200) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2scalar could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_2shift.
 * Specified by: ('See Section A7.4.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2shift(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00000F00) == 0x00000500) && ((insn & 0x01000000) == 0x00000000) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000800) && ((insn & 0x01000000) == 0x00000000) && ((insn & 0x00000040) == 0x00000000) && ((insn & 0x00000080) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000800) && ((insn & 0x01000000) == 0x01000000) && ((insn & 0x00000040) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000A00) && (true) && ((insn & 0x00000040) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000E00) == 0x00000400) && ((insn & 0x01000000) == 0x01000000) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000600) == 0x00000600) && (true) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000800) && (true) && ((insn & 0x00000040) == 0x00000040) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000900) && (true) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000C00) == 0x00000000) && (true) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2shift could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_3diff.
 * Specified by: ('See Section A7.4.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_3diff(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00000D00) == 0x00000900) && ((insn & 0x01000000) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000D00) && ((insn & 0x01000000) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000900) == 0x00000800) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000800) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_3diff could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_dp_3same.
 * Specified by: ('See Section A7.4.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_3same(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00000F00) == 0x00000D00) && ((insn & 0x00000010) == 0x00000010) && ((insn & 0x01000000) == 0x01000000) && ((insn & 0x00200000) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000E00) && ((insn & 0x00000010) == 0x00000000) && ((insn & 0x01000000) == 0x01000000) && ((insn & 0x00200000) == 0x00200000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000E00) && ((insn & 0x00000010) == 0x00000010) && ((insn & 0x01000000) == 0x01000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000D00) == 0x00000100) && ((insn & 0x00000010) == 0x00000000) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000B00) == 0x00000300) && ((insn & 0x00000010) == 0x00000010) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000500) && (true) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000900) && (true) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000B00) && ((insn & 0x00000010) == 0x00000000) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000D00) && ((insn & 0x00000010) == 0x00000000) && ((insn & 0x01000000) == 0x01000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000D00) && (true) && ((insn & 0x01000000) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000E00) && ((insn & 0x00000010) == 0x00000000) && (true) && ((insn & 0x00200000) == 0x00000000)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000700) == 0x00000700) && ((insn & 0x00000010) == 0x00000000) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000B00) == 0x00000B00) && ((insn & 0x00000010) == 0x00000010) && ((insn & 0x01000000) == 0x00000000) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000900) == 0x00000000) && (true) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000D00) == 0x00000800) && (true) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if (((insn & 0x00000F00) == 0x00000100) && ((insn & 0x00000010) == 0x00000010) && (true) && (true)) {
   return EffectiveNoOp_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_3same could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_load_store.
 * Specified by: ('See Section A7.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_load_store(
     const nacl_arm_dec::Instruction insn) const {

  if (((insn & 0x00200000) == 0x00000000)) {
   return decode_simd_load_store_l0(insn);
  }

  if (((insn & 0x00200000) == 0x00200000)) {
   return decode_simd_load_store_l1(insn);
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_load_store_l0.
 * Specified by: ('See Section A7.7, Table A7 - 20',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_load_store_l0(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000F00) == 0x00000300)) {
   return VectorStore_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000700) == 0x00000200)) {
   return VectorStore_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000C00) == 0x00000400)) {
   return VectorStore_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000600) == 0x00000000)) {
   return VectorStore_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00800000) && ((insn & 0x00000800) == 0x00000000)) {
   return VectorStore_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00800000) && ((insn & 0x00000C00) == 0x00000800)) {
   return VectorStore_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store_l0 could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table simd_load_store_l1.
 * Specified by: ('See Section A7.7, Table A7 - 21',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_load_store_l1(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000F00) == 0x00000300)) {
   return VectorLoad_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000700) == 0x00000200)) {
   return VectorLoad_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000C00) == 0x00000400)) {
   return VectorLoad_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00000000) && ((insn & 0x00000600) == 0x00000000)) {
   return VectorLoad_None_instance_;
  }

  if (((insn & 0x00800000) == 0x00800000) && (true)) {
   return VectorLoad_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store_l1 could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table super_cop.
 * Specified by: ('See Section A5.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_super_cop(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x03100000) == 0x02100000) && ((insn & 0x00000010) == 0x00000010)) {
   return MoveFromCoprocessor_None_instance_;
  }

  if (((insn & 0x02100000) == 0x00000000) && (true)) {
   return StoreCoprocessor_None_instance_;
  }

  if (((insn & 0x03F00000) == 0x00400000) && (true)) {
   return CoprocessorOp_None_instance_;
  }

  if (((insn & 0x03E00000) == 0x00000000) && (true)) {
   return Undefined_None_instance_;
  }

  if (((insn & 0x02100000) == 0x00100000) && (true)) {
   return LoadCoprocessor_None_instance_;
  }

  if (((insn & 0x03000000) == 0x02000000) && ((insn & 0x00000010) == 0x00000000)) {
   return CoprocessorOp_None_instance_;
  }

  if (((insn & 0x03100000) == 0x02000000) && ((insn & 0x00000010) == 0x00000010)) {
   return CoprocessorOp_None_instance_;
  }

  if (((insn & 0x03F00000) == 0x00500000) && (true)) {
   return MoveDoubleFromCoprocessor_None_instance_;
  }

  if (((insn & 0x03000000) == 0x03000000) && (true)) {
   return Forbidden_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: super_cop could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table sync.
 * Specified by: ('See Section A5.2.10',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_sync(
     const nacl_arm_dec::Instruction insn) const {
  UNREFERENCED_PARAMETER(insn);
  if (((insn & 0x00B00000) == 0x00000000)) {
   return Deprecated_None_instance_;
  }

  if (((insn & 0x00F00000) == 0x00B00000)) {
   return LoadDoubleExclusive_None_instance_;
  }

  if (((insn & 0x00F00000) == 0x00C00000)) {
   return StoreExclusive_None_instance_;
  }

  if (((insn & 0x00B00000) == 0x00A00000)) {
   return StoreExclusive_None_instance_;
  }

  if (((insn & 0x00F00000) == 0x00900000)) {
   return LoadExclusive_None_instance_;
  }

  if (((insn & 0x00F00000) == 0x00800000)) {
   return StoreExclusive_None_instance_;
  }

  if (((insn & 0x00D00000) == 0x00D00000)) {
   return LoadExclusive_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: sync could not parse %08X",
          insn.bits(31,0));
  return Forbidden_None_instance_;
}


/*
 * Implementation of table unconditional.
 * Specified by: ('See Section A5.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_unconditional(
     const nacl_arm_dec::Instruction insn) const {

  if (((insn & 0x08000000) == 0x00000000) && (true) && (true)) {
   return decode_misc_hints_simd(insn);
  }

  if (((insn & 0x0F100000) == 0x0E100000) && ((insn & 0x00000010) == 0x00000010) && (true)) {
   return MoveFromCoprocessor_None_instance_;
  }

  if (((insn & 0x0FB00000) == 0x0C300000) && (true) && ((insn & 0x000F0000) != 0x000F0000)) {
   return LoadCoprocessor_None_instance_;
  }

  if (((insn & 0x0F900000) == 0x0C900000) && (true) && ((insn & 0x000F0000) == 0x000F0000)) {
   return LoadCoprocessor_None_instance_;
  }

  if (((insn & 0x0F100000) == 0x0D100000) && (true) && ((insn & 0x000F0000) == 0x000F0000)) {
   return LoadCoprocessor_None_instance_;
  }

  if (((insn & 0x0FF00000) == 0x0C500000) && (true) && (true)) {
   return MoveDoubleFromCoprocessor_None_instance_;
  }

  if (((insn & 0x0F000000) == 0x0E000000) && ((insn & 0x00000010) == 0x00000000) && (true)) {
   return CoprocessorOp_None_instance_;
  }

  if (((insn & 0x0F100000) == 0x0E000000) && ((insn & 0x00000010) == 0x00000010) && (true)) {
   return CoprocessorOp_None_instance_;
  }

  if (((insn & 0x0FB00000) == 0x0C200000) && (true) && (true)) {
   return StoreCoprocessor_None_instance_;
  }

  if (((insn & 0x0F900000) == 0x0C800000) && (true) && (true)) {
   return StoreCoprocessor_None_instance_;
  }

  if (((insn & 0x0F100000) == 0x0D000000) && (true) && (true)) {
   return StoreCoprocessor_None_instance_;
  }

  if (((insn & 0x0FF00000) == 0x0C400000) && (true) && (true)) {
   return CoprocessorOp_None_instance_;
  }

  if (((insn & 0x0E500000) == 0x08400000) && (true) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x0E500000) == 0x08100000) && (true) && (true)) {
   return Forbidden_None_instance_;
  }

  if (((insn & 0x0E000000) == 0x0A000000) && (true) && (true)) {
   return Forbidden_None_instance_;
  }

  if ((true)) {
   return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  fprintf(stderr, "TABLE IS INCOMPLETE: unconditional could not parse %08X",
          insn.bits(31,0));
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
