/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE


#include "native_client/src/trusted/validator_arm/gen/arm32_decode.h"

namespace nacl_arm_dec {


Arm32DecoderState::Arm32DecoderState() : DecoderState()
  , Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_instance_()
  , Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_instance_()
  , Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_instance_()
  , Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_instance_()
  , Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_instance_()
  , Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_instance_()
  , Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_instance_()
  , Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1_instance_()
  , Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_instance_()
  , Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_instance_()
  , Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_instance_()
  , Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_instance_()
  , Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1_instance_()
  , Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_instance_()
  , Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_instance_()
  , Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_instance_()
  , Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1_instance_()
  , Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1_instance_()
  , Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_instance_()
  , Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_instance_()
  , Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_instance_()
  , Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1_instance_()
  , Actual_Unnamed_case_1_instance_()
  , Binary2RegisterBitRangeMsbGeLsb_instance_()
  , Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_instance_()
  , Binary3RegisterOpAltA_instance_()
  , Binary3RegisterOpAltANoCondsUpdate_instance_()
  , Binary3RegisterOpAltBNoCondUpdates_instance_()
  , Binary4RegisterDualOp_instance_()
  , Binary4RegisterDualOpNoCondsUpdate_instance_()
  , Binary4RegisterDualResult_instance_()
  , Binary4RegisterDualResultNoCondsUpdate_instance_()
  , BranchImmediate24_instance_()
  , BranchToRegister_instance_()
  , BreakPointAndConstantPoolHead_instance_()
  , CondDecoder_instance_()
  , DataBarrier_instance_()
  , Deprecated_instance_()
  , DuplicateToAdvSIMDRegisters_instance_()
  , Forbidden_instance_()
  , ForbiddenCondDecoder_instance_()
  , InstructionBarrier_instance_()
  , LdrImmediateOp_instance_()
  , Load2RegisterImm12Op_instance_()
  , Load2RegisterImm8DoubleOp_instance_()
  , Load2RegisterImm8Op_instance_()
  , Load3RegisterDoubleOp_instance_()
  , Load3RegisterImm5Op_instance_()
  , Load3RegisterOp_instance_()
  , LoadExclusive2RegisterDoubleOp_instance_()
  , LoadExclusive2RegisterOp_instance_()
  , LoadRegisterImm8DoubleOp_instance_()
  , LoadRegisterImm8Op_instance_()
  , LoadRegisterList_instance_()
  , LoadVectorRegister_instance_()
  , LoadVectorRegisterList_instance_()
  , MoveDoubleVfpRegisterOp_instance_()
  , MoveImmediate12ToApsr_instance_()
  , MoveVfpRegisterOp_instance_()
  , MoveVfpRegisterOpWithTypeSel_instance_()
  , PermanentlyUndefined_instance_()
  , PreloadRegisterImm12Op_instance_()
  , PreloadRegisterPairOp_instance_()
  , Store2RegisterImm12Op_instance_()
  , Store2RegisterImm8DoubleOp_instance_()
  , Store2RegisterImm8Op_instance_()
  , Store3RegisterDoubleOp_instance_()
  , Store3RegisterImm5Op_instance_()
  , Store3RegisterOp_instance_()
  , StoreExclusive3RegisterDoubleOp_instance_()
  , StoreExclusive3RegisterOp_instance_()
  , StoreRegisterList_instance_()
  , StoreVectorRegister_instance_()
  , StoreVectorRegisterList_instance_()
  , Unary1RegisterBitRangeMsbGeLsb_instance_()
  , Unary1RegisterSet_instance_()
  , Unary1RegisterUse_instance_()
  , Unary2RegisterImmedShiftedOp_instance_()
  , Unary2RegisterOpNotRmIsPc_instance_()
  , Unary2RegisterSatImmedShiftedOp_instance_()
  , Undefined_instance_()
  , Unpredictable_instance_()
  , Vector1RegisterImmediate_BIT_instance_()
  , Vector1RegisterImmediate_MOV_instance_()
  , Vector1RegisterImmediate_MVN_instance_()
  , Vector2RegisterMiscellaneous_CVT_F2I_instance_()
  , Vector2RegisterMiscellaneous_CVT_H2S_instance_()
  , Vector2RegisterMiscellaneous_F32_instance_()
  , Vector2RegisterMiscellaneous_I16_32_64N_instance_()
  , Vector2RegisterMiscellaneous_I8_16_32L_instance_()
  , Vector2RegisterMiscellaneous_RG_instance_()
  , Vector2RegisterMiscellaneous_V16_32_64N_instance_()
  , Vector2RegisterMiscellaneous_V8_instance_()
  , Vector2RegisterMiscellaneous_V8S_instance_()
  , Vector2RegisterMiscellaneous_V8_16_32_instance_()
  , Vector2RegisterMiscellaneous_V8_16_32I_instance_()
  , Vector2RegisterMiscellaneous_V8_16_32T_instance_()
  , VectorBinary2RegisterScalar_F32_instance_()
  , VectorBinary2RegisterScalar_I16_32_instance_()
  , VectorBinary2RegisterScalar_I16_32L_instance_()
  , VectorBinary2RegisterShiftAmount_CVT_instance_()
  , VectorBinary2RegisterShiftAmount_E8_16_32L_instance_()
  , VectorBinary2RegisterShiftAmount_I_instance_()
  , VectorBinary2RegisterShiftAmount_ILS_instance_()
  , VectorBinary2RegisterShiftAmount_N16_32_64R_instance_()
  , VectorBinary2RegisterShiftAmount_N16_32_64RS_instance_()
  , VectorBinary3RegisterDifferentLength_I16_32L_instance_()
  , VectorBinary3RegisterDifferentLength_I16_32_64_instance_()
  , VectorBinary3RegisterDifferentLength_I8_16_32_instance_()
  , VectorBinary3RegisterDifferentLength_I8_16_32L_instance_()
  , VectorBinary3RegisterDifferentLength_P8_instance_()
  , VectorBinary3RegisterImmOp_instance_()
  , VectorBinary3RegisterLookupOp_instance_()
  , VectorBinary3RegisterSameLength32P_instance_()
  , VectorBinary3RegisterSameLength32_DQ_instance_()
  , VectorBinary3RegisterSameLengthDI_instance_()
  , VectorBinary3RegisterSameLengthDQ_instance_()
  , VectorBinary3RegisterSameLengthDQI16_32_instance_()
  , VectorBinary3RegisterSameLengthDQI8P_instance_()
  , VectorBinary3RegisterSameLengthDQI8_16_32_instance_()
  , VectorLoadSingle1AllLanes_instance_()
  , VectorLoadSingle2AllLanes_instance_()
  , VectorLoadSingle3AllLanes_instance_()
  , VectorLoadSingle4AllLanes_instance_()
  , VectorLoadStoreMultiple1_instance_()
  , VectorLoadStoreMultiple2_instance_()
  , VectorLoadStoreMultiple3_instance_()
  , VectorLoadStoreMultiple4_instance_()
  , VectorLoadStoreSingle1_instance_()
  , VectorLoadStoreSingle2_instance_()
  , VectorLoadStoreSingle3_instance_()
  , VectorLoadStoreSingle4_instance_()
  , VectorUnary2RegisterDup_instance_()
  , VfpMrsOp_instance_()
  , VfpOp_instance_()
  , VfpUsesRegOp_instance_()
  , not_implemented_()
{}

// Implementation of table: ARMv7.
// Specified by: See Section A5.1
const ClassDecoder& Arm32DecoderState::decode_ARMv7(
     const Instruction inst) const
{
  if ((inst.Bits() & 0xF0000000)  !=
          0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000)  ==
          0x04000000 /* op1(27:25)=010 */) {
    return decode_load_store_word_and_unsigned_byte(inst);
  }

  if ((inst.Bits() & 0xF0000000)  !=
          0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000)  ==
          0x06000000 /* op1(27:25)=011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op(4)=0 */) {
    return decode_load_store_word_and_unsigned_byte(inst);
  }

  if ((inst.Bits() & 0xF0000000)  !=
          0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000)  ==
          0x06000000 /* op1(27:25)=011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* op(4)=1 */) {
    return decode_media_instructions(inst);
  }

  if ((inst.Bits() & 0xF0000000)  !=
          0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000)  ==
          0x00000000 /* op1(27:25)=00x */) {
    return decode_data_processing_and_miscellaneous_instructions(inst);
  }

  if ((inst.Bits() & 0xF0000000)  !=
          0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000)  ==
          0x08000000 /* op1(27:25)=10x */) {
    return decode_branch_branch_with_link_and_block_data_transfer(inst);
  }

  if ((inst.Bits() & 0xF0000000)  !=
          0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000)  ==
          0x0C000000 /* op1(27:25)=11x */) {
    return decode_coprocessor_instructions_and_supervisor_call(inst);
  }

  if ((inst.Bits() & 0xF0000000)  ==
          0xF0000000 /* cond(31:28)=1111 */) {
    return decode_unconditional_instructions(inst);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: advanced_simd_data_processing_instructions.
// Specified by: See Section A7.4
const ClassDecoder& Arm32DecoderState::decode_advanced_simd_data_processing_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x00B80000)  ==
          0x00800000 /* A(23:19)=1x000 */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000010 /* C(7:4)=0xx1 */) {
    return decode_simd_dp_1imm(inst);
  }

  if ((inst.Bits() & 0x00B80000)  ==
          0x00880000 /* A(23:19)=1x001 */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000010 /* C(7:4)=0xx1 */) {
    return decode_simd_dp_2shift(inst);
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00900000 /* A(23:19)=1x01x */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000010 /* C(7:4)=0xx1 */) {
    return decode_simd_dp_2shift(inst);
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00A00000 /* A(23:19)=1x10x */ &&
      (inst.Bits() & 0x00000050)  ==
          0x00000000 /* C(7:4)=x0x0 */) {
    return decode_simd_dp_3diff(inst);
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00A00000 /* A(23:19)=1x10x */ &&
      (inst.Bits() & 0x00000050)  ==
          0x00000040 /* C(7:4)=x1x0 */) {
    return decode_simd_dp_2scalar(inst);
  }

  if ((inst.Bits() & 0x00A00000)  ==
          0x00800000 /* A(23:19)=1x0xx */ &&
      (inst.Bits() & 0x00000050)  ==
          0x00000000 /* C(7:4)=x0x0 */) {
    return decode_simd_dp_3diff(inst);
  }

  if ((inst.Bits() & 0x00A00000)  ==
          0x00800000 /* A(23:19)=1x0xx */ &&
      (inst.Bits() & 0x00000050)  ==
          0x00000040 /* C(7:4)=x1x0 */) {
    return decode_simd_dp_2scalar(inst);
  }

  if ((inst.Bits() & 0x00A00000)  ==
          0x00A00000 /* A(23:19)=1x1xx */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000010 /* C(7:4)=0xx1 */) {
    return decode_simd_dp_2shift(inst);
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23:19)=0xxxx */) {
    return decode_simd_dp_3same(inst);
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23:19)=1xxxx */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000090 /* C(7:4)=1xx1 */) {
    return decode_simd_dp_2shift(inst);
  }

  if ((inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00B00000)  ==
          0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* C(7:4)=xxx0 */) {
    return VectorBinary3RegisterImmOp_instance_;
  }

  if ((inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00B00000)  ==
          0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* B(11:8)=1100 */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000000 /* C(7:4)=0xx0 */) {
    return VectorUnary2RegisterDup_instance_;
  }

  if ((inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00B00000)  ==
          0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000C00)  ==
          0x00000800 /* B(11:8)=10xx */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* C(7:4)=xxx0 */) {
    return VectorBinary3RegisterLookupOp_instance_;
  }

  if ((inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00B00000)  ==
          0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000800)  ==
          0x00000000 /* B(11:8)=0xxx */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* C(7:4)=xxx0 */) {
    return decode_simd_dp_2misc(inst);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: advanced_simd_element_or_structure_load_store_instructions.
// Specified by: See Section A7.7
const ClassDecoder& Arm32DecoderState::decode_advanced_simd_element_or_structure_load_store_instructions(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* B(11:8)=1100 */) {
    return VectorLoadSingle1AllLanes_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* B(11:8)=1101 */) {
    return VectorLoadSingle2AllLanes_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* B(11:8)=1110 */) {
    return VectorLoadSingle3AllLanes_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* B(11:8)=1111 */) {
    return VectorLoadSingle4AllLanes_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000300 /* B(11:8)=0011 */) {
    return VectorLoadStoreMultiple2_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000200 /* B(11:8)=x010 */) {
    return VectorLoadStoreMultiple1_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000000 /* B(11:8)=000x */) {
    return VectorLoadStoreMultiple4_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000400 /* B(11:8)=010x */) {
    return VectorLoadStoreMultiple3_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000600 /* B(11:8)=011x */) {
    return VectorLoadStoreMultiple1_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000800 /* B(11:8)=100x */) {
    return VectorLoadStoreMultiple2_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000800 /* B(11:8)=1000 */) {
    return VectorLoadStoreSingle1_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000900 /* B(11:8)=1001 */) {
    return VectorLoadStoreSingle2_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* B(11:8)=1010 */) {
    return VectorLoadStoreSingle3_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* B(11:8)=1011 */) {
    return VectorLoadStoreSingle4_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000000 /* B(11:8)=0x00 */) {
    return VectorLoadStoreSingle1_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000100 /* B(11:8)=0x01 */) {
    return VectorLoadStoreSingle2_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000200 /* B(11:8)=0x10 */) {
    return VectorLoadStoreSingle3_instance_;
  }

  if ((inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000300 /* B(11:8)=0x11 */) {
    return VectorLoadStoreSingle4_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: branch_branch_with_link_and_block_data_transfer.
// Specified by: See Section A5.5
const ClassDecoder& Arm32DecoderState::decode_branch_branch_with_link_and_block_data_transfer(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x02500000)  ==
          0x00000000 /* op(25:20)=0xx0x0 */) {
    return StoreRegisterList_instance_;
  }

  if ((inst.Bits() & 0x02500000)  ==
          0x00100000 /* op(25:20)=0xx0x1 */) {
    return LoadRegisterList_instance_;
  }

  if ((inst.Bits() & 0x02500000)  ==
          0x00400000 /* op(25:20)=0xx1x0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_instance_;
  }

  if ((inst.Bits() & 0x02500000)  ==
          0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000)  ==
          0x00000000 /* R(15)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_instance_;
  }

  if ((inst.Bits() & 0x02500000)  ==
          0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000)  ==
          0x00008000 /* R(15)=1 */) {
    return ForbiddenCondDecoder_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* op(25:20)=1xxxxx */) {
    return BranchImmediate24_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: coprocessor_instructions_and_supervisor_call.
// Specified by: See Section A5.6
const ClassDecoder& Arm32DecoderState::decode_coprocessor_instructions_and_supervisor_call(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x03E00000)  ==
          0x00000000 /* op1(25:20)=00000x */) {
    return Undefined_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03E00000)  ==
          0x00400000 /* op1(25:20)=00010x */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000)  ==
          0x00000000 /* op1(25:20)=0xxxx0 */ &&
      (inst.Bits() & 0x03B00000)  !=
          0x00000000 /* op1_repeated(25:20)=~000x00 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000)  ==
          0x00100000 /* op1(25:20)=0xxxx1 */ &&
      (inst.Bits() & 0x03B00000)  !=
          0x00100000 /* op1_repeated(25:20)=~000x01 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03000000)  ==
          0x02000000 /* op1(25:20)=10xxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03E00000)  ==
          0x00400000 /* op1(25:20)=00010x */) {
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03000000)  ==
          0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op(4)=0 */) {
    return decode_floating_point_data_processing_instructions(inst);
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03000000)  ==
          0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* op(4)=1 */) {
    return decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(inst);
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x02000000)  ==
          0x00000000 /* op1(25:20)=0xxxxx */ &&
      (inst.Bits() & 0x03A00000)  !=
          0x00000000 /* op1_repeated(25:20)=~000x0x */) {
    return decode_extension_register_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x03000000)  ==
          0x03000000 /* op1(25:20)=11xxxx */) {
    return Forbidden_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: data_processing_and_miscellaneous_instructions.
// Specified by: See Section A5.2
const ClassDecoder& Arm32DecoderState::decode_data_processing_and_miscellaneous_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000)  !=
          0x01000000 /* op1(24:20)=~10xx0 */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000010 /* op2(7:4)=0xx1 */) {
    return decode_data_processing_register_shifted_register(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000)  !=
          0x01000000 /* op1(24:20)=~10xx0 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */) {
    return decode_data_processing_register(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000)  ==
          0x01000000 /* op1(24:20)=10xx0 */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000080 /* op2(7:4)=1xx0 */) {
    return decode_halfword_multiply_and_multiply_accumulate(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000)  ==
          0x01000000 /* op1(24:20)=10xx0 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* op2(7:4)=0xxx */) {
    return decode_miscellaneous_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000)  !=
          0x00200000 /* op1(24:20)=~0xx1x */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x000000B0 /* op2(7:4)=1011 */) {
    return decode_extra_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000)  !=
          0x00200000 /* op1(24:20)=~0xx1x */ &&
      (inst.Bits() & 0x000000D0)  ==
          0x000000D0 /* op2(7:4)=11x1 */) {
    return decode_extra_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x000000B0 /* op2(7:4)=1011 */) {
    return ForbiddenCondDecoder_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000D0)  ==
          0x000000D0 /* op2(7:4)=11x1 */) {
    return ForbiddenCondDecoder_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* op1(24:20)=0xxxx */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000090 /* op2(7:4)=1001 */) {
    return decode_multiply_and_multiply_accumulate(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* op1(24:20)=1xxxx */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000090 /* op2(7:4)=1001 */) {
    return decode_synchronization_primitives(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01B00000)  ==
          0x01000000 /* op1(24:20)=10x00 */) {
    return Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01B00000)  ==
          0x01200000 /* op1(24:20)=10x10 */) {
    return decode_msr_immediate_and_hints(inst);
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01900000)  !=
          0x01000000 /* op1(24:20)=~10xx0 */) {
    return decode_data_processing_immediate(inst);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: data_processing_immediate.
// Specified by: See Section A5.2.3
const ClassDecoder& Arm32DecoderState::decode_data_processing_immediate(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01F00000)  ==
          0x01100000 /* op(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01500000 /* op(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01B00000)  ==
          0x01300000 /* op(24:20)=10x11 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00400000 /* op(24:20)=0010x */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00400000 /* op(24:20)=0010x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */) {
    return Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00800000 /* op(24:20)=0100x */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00800000 /* op(24:20)=0100x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */) {
    return Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00A00000 /* op(24:20)=0101x */) {
    return Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00C00000 /* op(24:20)=0110x */) {
    return Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01800000 /* op(24:20)=1100x */) {
    return Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01C00000 /* op(24:20)=1110x */) {
    return Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01600000)  ==
          0x00600000 /* op(24:20)=0x11x */) {
    return Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01A00000)  ==
          0x01A00000 /* op(24:20)=11x1x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_instance_;
  }

  if ((inst.Bits() & 0x01C00000)  ==
          0x00000000 /* op(24:20)=000xx */) {
    return Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: data_processing_register.
// Specified by: See Section A5.2.1
const ClassDecoder& Arm32DecoderState::decode_data_processing_register(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80)  !=
          0x00000000 /* op2(11:7)=~00000 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op3(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80)  !=
          0x00000000 /* op2(11:7)=~00000 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000060 /* op3(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80)  ==
          0x00000000 /* op2(11:7)=00000 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op3(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80)  ==
          0x00000000 /* op2(11:7)=00000 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000060 /* op3(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000020 /* op3(6:5)=01 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000040 /* op3(6:5)=10 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01900000)  ==
          0x01100000 /* op1(24:20)=10xx1 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01A00000)  ==
          0x01800000 /* op1(24:20)=11x0x */) {
    return Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01000000)  ==
          0x00000000 /* op1(24:20)=0xxxx */) {
    return Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: data_processing_register_shifted_register.
// Specified by: See Section A5.2.2
const ClassDecoder& Arm32DecoderState::decode_data_processing_register_shifted_register(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01900000)  ==
          0x01100000 /* op1(24:20)=10xx1 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01A00000)  ==
          0x01800000 /* op1(24:20)=11x0x */) {
    return Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_instance_;
  }

  if ((inst.Bits() & 0x01A00000)  ==
          0x01A00000 /* op1(24:20)=11x1x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_instance_;
  }

  if ((inst.Bits() & 0x01000000)  ==
          0x00000000 /* op1(24:20)=0xxxx */) {
    return Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: extension_register_load_store_instructions.
// Specified by: A7.6
const ClassDecoder& Arm32DecoderState::decode_extension_register_load_store_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x01B00000)  ==
          0x01200000 /* opcode(24:20)=10x10 */) {
    return StoreVectorRegisterList_instance_;
  }

  if ((inst.Bits() & 0x01B00000)  ==
          0x01300000 /* opcode(24:20)=10x11 */) {
    return LoadVectorRegisterList_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00400000 /* opcode(24:20)=0010x */) {
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);
  }

  if ((inst.Bits() & 0x01300000)  ==
          0x01000000 /* opcode(24:20)=1xx00 */) {
    return StoreVectorRegister_instance_;
  }

  if ((inst.Bits() & 0x01300000)  ==
          0x01100000 /* opcode(24:20)=1xx01 */) {
    return LoadVectorRegister_instance_;
  }

  if ((inst.Bits() & 0x01900000)  ==
          0x00800000 /* opcode(24:20)=01xx0 */) {
    return StoreVectorRegisterList_instance_;
  }

  if ((inst.Bits() & 0x01900000)  ==
          0x00900000 /* opcode(24:20)=01xx1 */) {
    return LoadVectorRegisterList_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: extra_load_store_instructions.
// Specified by: See Section A5.2.8
const ClassDecoder& Arm32DecoderState::decode_extra_load_store_instructions(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Store3RegisterOp_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */) {
    return Store2RegisterImm8Op_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */) {
    return LoadRegisterImm8Op_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterDoubleOp_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterOp_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8DoubleOp_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return LoadRegisterImm8DoubleOp_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8Op_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Store3RegisterDoubleOp_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */) {
    return Store2RegisterImm8DoubleOp_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op2(6:5)=x1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterOp_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op2(6:5)=x1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8Op_instance_;
  }

  if ((inst.Bits() & 0x00000040)  ==
          0x00000040 /* op2(6:5)=1x */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return LoadRegisterImm8Op_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: floating_point_data_processing_instructions.
// Specified by: A7.5 Table A7 - 16
const ClassDecoder& Arm32DecoderState::decode_floating_point_data_processing_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x00B00000)  ==
          0x00300000 /* opc1(23:20)=0x11 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00800000 /* opc1(23:20)=1x00 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* opc3(7:6)=x0 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00900000 /* opc1(23:20)=1x01 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00B00000 /* opc1(23:20)=1x11 */) {
    return decode_other_floating_point_data_processing_instructions(inst);
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* opc1(23:20)=xx10 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00A00000)  ==
          0x00000000 /* opc1(23:20)=0x0x */) {
    return VfpOp_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: halfword_multiply_and_multiply_accumulate.
// Specified by: See Section A5.2.7
const ClassDecoder& Arm32DecoderState::decode_halfword_multiply_and_multiply_accumulate(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00600000)  ==
          0x00000000 /* op1(22:21)=00 */) {
    return Binary4RegisterDualOp_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */) {
    return Binary4RegisterDualOp_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltA_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00400000 /* op1(22:21)=10 */) {
    return Binary4RegisterDualResult_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00600000 /* op1(22:21)=11 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltA_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: load_store_word_and_unsigned_byte.
// Specified by: See Section A5.3
const ClassDecoder& Arm32DecoderState::decode_load_store_word_and_unsigned_byte(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00200000 /* op1_repeated(24:20)=~0x010 */) {
    return Store2RegisterImm12Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00300000 /* op1_repeated(24:20)=~0x011 */) {
    return LdrImmediateOp_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00300000 /* op1_repeated(24:20)=~0x011 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return Load2RegisterImm12Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00600000 /* op1_repeated(24:20)=~0x110 */) {
    return Store2RegisterImm12Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00700000 /* op1_repeated(24:20)=~0x111 */) {
    return Load2RegisterImm12Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00700000 /* op1_repeated(24:20)=~0x111 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return Load2RegisterImm12Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x00200000 /* op1(24:20)=0xx1x */) {
    return ForbiddenCondDecoder_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00200000 /* op1_repeated(24:20)=~0x010 */) {
    return Store3RegisterImm5Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00300000 /* op1_repeated(24:20)=~0x011 */) {
    return Load3RegisterImm5Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00600000 /* op1_repeated(24:20)=~0x110 */) {
    return Store3RegisterImm5Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00700000 /* op1_repeated(24:20)=~0x111 */) {
    return Load3RegisterImm5Op_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: media_instructions.
// Specified by: See Section A5.4
const ClassDecoder& Arm32DecoderState::decode_media_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x01F00000)  ==
          0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000)  !=
          0x0000F000 /* Rd(15:12)=~1111 */) {
    return Binary4RegisterDualOp_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* Rd(15:12)=1111 */) {
    return Binary3RegisterOpAltA_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01F00000 /* op1(24:20)=11111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */) {
    return PermanentlyUndefined_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F)  !=
          0x0000000F /* Rn(3:0)=~1111 */) {
    return Binary2RegisterBitRangeMsbGeLsb_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F)  ==
          0x0000000F /* Rn(3:0)=1111 */) {
    return Unary1RegisterBitRangeMsbGeLsb_instance_;
  }

  if ((inst.Bits() & 0x01A00000)  ==
          0x01A00000 /* op1(24:20)=11x1x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(7:5)=x10 */) {
    return Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_instance_;
  }

  if ((inst.Bits() & 0x01C00000)  ==
          0x00000000 /* op1(24:20)=000xx */) {
    return decode_parallel_addition_and_subtraction_signed(inst);
  }

  if ((inst.Bits() & 0x01C00000)  ==
          0x00400000 /* op1(24:20)=001xx */) {
    return decode_parallel_addition_and_subtraction_unsigned(inst);
  }

  if ((inst.Bits() & 0x01800000)  ==
          0x00800000 /* op1(24:20)=01xxx */) {
    return decode_packing_unpacking_saturation_and_reversal(inst);
  }

  if ((inst.Bits() & 0x01800000)  ==
          0x01000000 /* op1(24:20)=10xxx */) {
    return decode_signed_multiply_signed_and_unsigned_divide(inst);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: memory_hints_advanced_simd_instructions_and_miscellaneous_instructions.
// Specified by: See Section A5.7.1
const ClassDecoder& Arm32DecoderState::decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x07F00000)  ==
          0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000000 /* op2(7:4)=0000 */ &&
      (inst.Bits() & 0x00010000)  ==
          0x00010000 /* Rn(19:16)=xxx1 */ &&
      (inst.Bits() & 0x000EFD0F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op2(7:4)=xx0x */ &&
      (inst.Bits() & 0x00010000)  ==
          0x00000000 /* Rn(19:16)=xxx0 */ &&
      (inst.Bits() & 0x0000FE00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05300000 /* op1(26:20)=1010011 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000000 /* op2(7:4)=0000 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000010 /* op2(7:4)=0001 */ &&
      (inst.Bits() & 0x000FFF0F)  ==
          0x000FF00F /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000060 /* op2(7:4)=0110 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return InstructionBarrier_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000070 /* op2(7:4)=0111 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:4)=001x */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000040 /* op2(7:4)=010x */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return DataBarrier_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000080 /* op2(7:4)=1xxx */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x04100000 /* op1(26:20)=100x001 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x04500000 /* op1(26:20)=100x101 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x05100000 /* op1(26:20)=101x001 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x05500000 /* op1(26:20)=101x101 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x06100000 /* op1(26:20)=110x001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x07100000 /* op1(26:20)=111x001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOp_instance_;
  }

  if ((inst.Bits() & 0x07B00000)  ==
          0x05B00000 /* op1(26:20)=1011x11 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x06700000)  ==
          0x06500000 /* op1(26:20)=11xx101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOp_instance_;
  }

  if ((inst.Bits() & 0x07300000)  ==
          0x04300000 /* op1(26:20)=100xx11 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07300000)  ==
          0x05100000 /* op1(26:20)=101xx01 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_instance_;
  }

  if ((inst.Bits() & 0x06300000)  ==
          0x06300000 /* op1(26:20)=11xxx11 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07100000)  ==
          0x04000000 /* op1(26:20)=100xxx0 */) {
    return decode_advanced_simd_element_or_structure_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x06000000)  ==
          0x02000000 /* op1(26:20)=01xxxxx */) {
    return decode_advanced_simd_data_processing_instructions(inst);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: miscellaneous_instructions.
// Specified by: See Section A5.2.12
const ClassDecoder& Arm32DecoderState::decode_miscellaneous_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00030000)  ==
          0x00000000 /* op1(19:16)=xx00 */ &&
      (inst.Bits() & 0x0000FD00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Unary1RegisterUse_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00030000)  ==
          0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000FD00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00020000)  ==
          0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000FD00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x0000FD00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x000F0D0F)  ==
          0x000F0000 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000 */) {
    return Unary1RegisterSet_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x00000C0F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* op(22:21)=x1 */ &&
      (inst.Bits() & 0x0000FC00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000010 /* op2(6:4)=001 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000F0F00)  ==
          0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPc_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000020 /* op2(6:4)=010 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000050 /* op2(6:4)=101 */) {
    return decode_saturating_addition_and_subtraction(inst);
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000060 /* op2(6:4)=110 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF0F)  ==
          0x0000000E /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */) {
    return BreakPointAndConstantPoolHead_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00400000 /* op(22:21)=10 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000050)  ==
          0x00000010 /* op2(6:4)=0x1 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return BranchToRegister_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: msr_immediate_and_hints.
// Specified by: See Section A5.2.11
const ClassDecoder& Arm32DecoderState::decode_msr_immediate_and_hints(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF)  ==
          0x00000004 /* op2(7:0)=00000100 */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FE)  ==
          0x00000000 /* op2(7:0)=0000000x */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return CondDecoder_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FE)  ==
          0x00000002 /* op2(7:0)=0000001x */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x000000F0 /* op2(7:0)=1111xxxx */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00040000 /* op1(19:16)=0100 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return MoveImmediate12ToApsr_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000B0000)  ==
          0x00080000 /* op1(19:16)=1x00 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return MoveImmediate12ToApsr_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00030000)  ==
          0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00020000)  ==
          0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00400000 /* op(22)=1 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if (true) {
    return Forbidden_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: multiply_and_multiply_accumulate.
// Specified by: See Section A5.2.5
const ClassDecoder& Arm32DecoderState::decode_multiply_and_multiply_accumulate(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00F00000)  ==
          0x00400000 /* op(23:20)=0100 */) {
    return Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00600000 /* op(23:20)=0110 */) {
    return Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_instance_;
  }

  if ((inst.Bits() & 0x00D00000)  ==
          0x00500000 /* op(23:20)=01x1 */) {
    return Actual_Unnamed_case_1_instance_;
  }

  if ((inst.Bits() & 0x00E00000)  ==
          0x00000000 /* op(23:20)=000x */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1_instance_;
  }

  if ((inst.Bits() & 0x00E00000)  ==
          0x00200000 /* op(23:20)=001x */) {
    return Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1_instance_;
  }

  if ((inst.Bits() & 0x00A00000)  ==
          0x00800000 /* op(23:20)=1x0x */) {
    return Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_instance_;
  }

  if ((inst.Bits() & 0x00A00000)  ==
          0x00A00000 /* op(23:20)=1x1x */) {
    return Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: other_floating_point_data_processing_instructions.
// Specified by: A7.5 Table A7 - 17
const ClassDecoder& Arm32DecoderState::decode_other_floating_point_data_processing_instructions(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x000F0000)  ==
          0x00010000 /* opc2(19:16)=0001 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00050000 /* opc2(19:16)=0101 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x0000002F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00070000 /* opc2(19:16)=0111 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x000000C0 /* opc3(7:6)=11 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00080000 /* opc2(19:16)=1000 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000B0000)  ==
          0x00000000 /* opc2(19:16)=0x00 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000E0000)  ==
          0x00020000 /* opc2(19:16)=001x */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000E0000)  ==
          0x000C0000 /* opc2(19:16)=110x */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000A0000)  ==
          0x000A0000 /* opc2(19:16)=1x1x */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00000040)  ==
          0x00000000 /* opc3(7:6)=x0 */ &&
      (inst.Bits() & 0x000000A0)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx */) {
    return VfpOp_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: packing_unpacking_saturation_and_reversal.
// Specified by: See Section A5.4.3
const ClassDecoder& Arm32DecoderState::decode_packing_unpacking_saturation_and_reversal(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op2(7:5)=xx0 */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(22:20)=x10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Unary2RegisterSatImmedShiftedOp_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(22:20)=x11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(22:20)=x11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(22:20)=x11 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(7:5)=x01 */ &&
      (inst.Bits() & 0x000F0F00)  ==
          0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00000000 /* op1(22:20)=xx0 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00000000 /* op1(22:20)=xx0 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* op1(22:20)=x1x */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op2(7:5)=xx0 */) {
    return Unary2RegisterSatImmedShiftedOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: parallel_addition_and_subtraction_signed.
// Specified by: See Section A5.4.1
const ClassDecoder& Arm32DecoderState::decode_parallel_addition_and_subtraction_signed(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: parallel_addition_and_subtraction_unsigned.
// Specified by: See Section A5.4.2
const ClassDecoder& Arm32DecoderState::decode_parallel_addition_and_subtraction_unsigned(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: saturating_addition_and_subtraction.
// Specified by: See Section A5.2.6
const ClassDecoder& Arm32DecoderState::decode_saturating_addition_and_subtraction(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: signed_multiply_signed_and_unsigned_divide.
// Specified by: See Section A5.4.4
const ClassDecoder& Arm32DecoderState::decode_signed_multiply_signed_and_unsigned_divide(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x0000F000)  !=
          0x0000F000 /* A(15:12)=~1111 */) {
    return Binary4RegisterDualOpNoCondsUpdate_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* A(15:12)=1111 */) {
    return Binary3RegisterOpAltANoCondsUpdate_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* op2(7:5)=0xx */) {
    return Binary4RegisterDualResultNoCondsUpdate_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000)  !=
          0x0000F000 /* A(15:12)=~1111 */) {
    return Binary4RegisterDualOpNoCondsUpdate_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* A(15:12)=1111 */) {
    return Binary3RegisterOpAltANoCondsUpdate_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x000000C0 /* op2(7:5)=11x */) {
    return Binary4RegisterDualOpNoCondsUpdate_instance_;
  }

  if ((inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(22:20)=0x1 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltANoCondsUpdate_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_1imm.
// Specified by: See Section A7.4.6
const ClassDecoder& Arm32DecoderState::decode_simd_dp_1imm(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000D00)  ==
          0x00000900 /* cmode(11:8)=10x1 */) {
    return Vector1RegisterImmediate_BIT_instance_;
  }

  if ((inst.Bits() & 0x00000900)  ==
          0x00000100 /* cmode(11:8)=0xx1 */) {
    return Vector1RegisterImmediate_BIT_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000D00)  ==
          0x00000800 /* cmode(11:8)=10x0 */) {
    return Vector1RegisterImmediate_MOV_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000900)  ==
          0x00000000 /* cmode(11:8)=0xx0 */) {
    return Vector1RegisterImmediate_MOV_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000C00)  ==
          0x00000C00 /* cmode(11:8)=11xx */) {
    return Vector1RegisterImmediate_MOV_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* cmode(11:8)=1110 */) {
    return Vector1RegisterImmediate_MOV_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* cmode(11:8)=1111 */) {
    return Undefined_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000D00)  ==
          0x00000800 /* cmode(11:8)=10x0 */) {
    return Vector1RegisterImmediate_MVN_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000C00 /* cmode(11:8)=110x */) {
    return Vector1RegisterImmediate_MVN_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000900)  ==
          0x00000000 /* cmode(11:8)=0xx0 */) {
    return Vector1RegisterImmediate_MVN_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_2misc.
// Specified by: See Section A7.4.5
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2misc(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000100 /* B(10:6)=0010x */) {
    return Vector2RegisterMiscellaneous_RG_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000000 /* B(10:6)=000xx */) {
    return Vector2RegisterMiscellaneous_RG_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000400 /* B(10:6)=100xx */) {
    return Vector2RegisterMiscellaneous_V8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000500 /* B(10:6)=101xx */) {
    return Vector2RegisterMiscellaneous_V8_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000700 /* B(10:6)=111xx */) {
    return Vector2RegisterMiscellaneous_V8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000200 /* B(10:6)=x10xx */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx */) {
    return Vector2RegisterMiscellaneous_V8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000380 /* B(10:6)=0111x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000780 /* B(10:6)=1111x */) {
    return Vector2RegisterMiscellaneous_F32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000680)  ==
          0x00000200 /* B(10:6)=01x0x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000680)  ==
          0x00000600 /* B(10:6)=11x0x */) {
    return Vector2RegisterMiscellaneous_F32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000600)  ==
          0x00000000 /* B(10:6)=00xxx */) {
    return Vector2RegisterMiscellaneous_V8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0)  ==
          0x00000200 /* B(10:6)=01000 */) {
    return Vector2RegisterMiscellaneous_V16_32_64N_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0)  ==
          0x00000240 /* B(10:6)=01001 */) {
    return Vector2RegisterMiscellaneous_I16_32_64N_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0)  ==
          0x00000300 /* B(10:6)=01100 */) {
    return Vector2RegisterMiscellaneous_I8_16_32L_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000006C0)  ==
          0x00000600 /* B(10:6)=11x00 */) {
    return Vector2RegisterMiscellaneous_CVT_H2S_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000000 /* B(10:6)=0000x */) {
    return Vector2RegisterMiscellaneous_V8S_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000080 /* B(10:6)=0001x */) {
    return Vector2RegisterMiscellaneous_V8_16_32T_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000280 /* B(10:6)=0101x */) {
    return Vector2RegisterMiscellaneous_I16_32_64N_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000100 /* B(10:6)=001xx */) {
    return Vector2RegisterMiscellaneous_V8_16_32I_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00030000 /* A(17:16)=11 */ &&
      (inst.Bits() & 0x00000600)  ==
          0x00000600 /* B(10:6)=11xxx */) {
    return Vector2RegisterMiscellaneous_CVT_F2I_instance_;
  }

  if ((inst.Bits() & 0x00010000)  ==
          0x00010000 /* A(17:16)=x1 */ &&
      (inst.Bits() & 0x00000600)  ==
          0x00000400 /* B(10:6)=10xxx */) {
    return Vector2RegisterMiscellaneous_F32_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_2scalar.
// Specified by: See Section A7.4.3
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2scalar(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */) {
    return VectorBinary2RegisterScalar_F32_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* A(11:8)=1010 */) {
    return VectorBinary2RegisterScalar_I16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterScalar_I16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */) {
    return VectorBinary2RegisterScalar_I16_32_instance_;
  }

  if ((inst.Bits() & 0x00000B00)  ==
          0x00000100 /* A(11:8)=0x01 */) {
    return VectorBinary2RegisterScalar_F32_instance_;
  }

  if ((inst.Bits() & 0x00000B00)  ==
          0x00000200 /* A(11:8)=0x10 */) {
    return VectorBinary2RegisterScalar_I16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000B00)  ==
          0x00000300 /* A(11:8)=0x11 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterScalar_I16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000300)  ==
          0x00000000 /* A(11:8)=xx00 */) {
    return VectorBinary2RegisterScalar_I16_32_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_2shift.
// Specified by: See Section A7.4.4
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2shift(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterShiftAmount_I_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64R_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* A(11:8)=1010 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_E8_16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000400 /* A(11:8)=010x */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary2RegisterShiftAmount_I_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000600 /* A(11:8)=011x */) {
    return VectorBinary2RegisterShiftAmount_ILS_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000E00 /* A(11:8)=111x */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_CVT_instance_;
  }

  if ((inst.Bits() & 0x00000C00)  ==
          0x00000000 /* A(11:8)=00xx */) {
    return VectorBinary2RegisterShiftAmount_I_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_3diff.
// Specified by: See Section A7.4.2
const ClassDecoder& Arm32DecoderState::decode_simd_dp_3diff(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* A(11:8)=1100 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */) {
    return VectorBinary3RegisterDifferentLength_P8_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000400 /* A(11:8)=01x0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000500 /* A(11:8)=01x1 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000800 /* A(11:8)=10x0 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000900 /* A(11:8)=10x1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32L_instance_;
  }

  if ((inst.Bits() & 0x00000C00)  ==
          0x00000000 /* A(11:8)=00xx */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_3same.
// Specified by: See Section A7.4.1
const ClassDecoder& Arm32DecoderState::decode_simd_dp_3same(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000600 /* A(11:8)=0110 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000700 /* A(11:8)=0111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQ_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8P_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* A(11:8)=1010 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx */) {
    return VectorBinary3RegisterSameLengthDI_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI16_32_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx */) {
    return VectorBinary3RegisterSameLengthDI_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* A(11:8)=1100 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00100000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx */) {
    return VectorBinary3RegisterSameLength32_DQ_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32P_instance_;
  }

  if ((inst.Bits() & 0x00000B00)  ==
          0x00000300 /* A(11:8)=0x11 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000000 /* A(11:8)=00x0 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000D00 /* A(11:8)=11x1 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32P_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000D00 /* A(11:8)=11x1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLength32_DQ_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000400 /* A(11:8)=010x */) {
    return VectorBinary3RegisterSameLengthDQ_instance_;
  }

  if ((inst.Bits() & 0x00000C00)  ==
          0x00000000 /* A(11:8)=00xx */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: synchronization_primitives.
// Specified by: See Section A5.2.10
const ClassDecoder& Arm32DecoderState::decode_synchronization_primitives(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00F00000)  ==
          0x00A00000 /* op(23:20)=1010 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterDoubleOp_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00B00000 /* op(23:20)=1011 */ &&
      (inst.Bits() & 0x00000F0F)  ==
          0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterDoubleOp_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00E00000 /* op(23:20)=1110 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterOp_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00F00000 /* op(23:20)=1111 */ &&
      (inst.Bits() & 0x00000F0F)  ==
          0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterOp_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00000000 /* op(23:20)=0x00 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Deprecated_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00800000 /* op(23:20)=1x00 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterOp_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00900000 /* op(23:20)=1x01 */ &&
      (inst.Bits() & 0x00000F0F)  ==
          0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: transfer_between_arm_core_and_extension_register_8_16_and_32_bit.
// Specified by: A7.8
const ClassDecoder& Arm32DecoderState::decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000100)  ==
          0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000)  ==
          0x00000000 /* A(23:21)=000 */ &&
      (inst.Bits() & 0x0000006F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000 */) {
    return MoveVfpRegisterOp_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000)  ==
          0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF)  ==
          0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */) {
    return VfpUsesRegOp_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23:21)=0xx */ &&
      (inst.Bits() & 0x0000000F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return MoveVfpRegisterOpWithTypeSel_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23:21)=1xx */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* B(6:5)=0x */ &&
      (inst.Bits() & 0x0000000F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return DuplicateToAdvSIMDRegisters_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000)  ==
          0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF)  ==
          0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */) {
    return VfpMrsOp_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x0000000F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return MoveVfpRegisterOpWithTypeSel_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: transfer_between_arm_core_and_extension_registers_64_bit.
// Specified by: A7.9
const ClassDecoder& Arm32DecoderState::decode_transfer_between_arm_core_and_extension_registers_64_bit(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x000000D0)  ==
          0x00000010 /* op(7:4)=00x1 */) {
    return MoveDoubleVfpRegisterOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: unconditional_instructions.
// Specified by: See Section A5.7
const ClassDecoder& Arm32DecoderState::decode_unconditional_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x0FE00000)  ==
          0x0C400000 /* op1(27:20)=1100010x */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E500000)  ==
          0x08100000 /* op1(27:20)=100xx0x1 */ &&
      (inst.Bits() & 0x0000FFFF)  ==
          0x00000A00 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E500000)  ==
          0x08400000 /* op1(27:20)=100xx1x0 */ &&
      (inst.Bits() & 0x000FFFE0)  ==
          0x000D0500 /* $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E100000)  ==
          0x0C000000 /* op1(27:20)=110xxxx0 */ &&
      (inst.Bits() & 0x0FB00000)  !=
          0x0C000000 /* op1_repeated(27:20)=~11000x00 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E100000)  ==
          0x0C100000 /* op1(27:20)=110xxxx1 */ &&
      (inst.Bits() & 0x0FB00000)  !=
          0x0C100000 /* op1_repeated(27:20)=~11000x01 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0F000000)  ==
          0x0E000000 /* op1(27:20)=1110xxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E000000)  ==
          0x0A000000 /* op1(27:20)=101xxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x08000000)  ==
          0x00000000 /* op1(27:20)=0xxxxxxx */) {
    return decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(inst);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

const ClassDecoder& Arm32DecoderState::decode(const Instruction inst) const {
  return decode_ARMv7(inst);
}

}  // namespace nacl_arm_dec
