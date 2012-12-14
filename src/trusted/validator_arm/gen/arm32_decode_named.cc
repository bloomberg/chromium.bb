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

/*
 * Implementation of table ARMv7.
 * Specified by: ('See Section A5.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_ARMv7(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x04000000 /* op1(27:25)=010 */) {
    return decode_load_store_word_and_unsigned_byte(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25)=011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */) {
    return decode_load_store_word_and_unsigned_byte(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25)=011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */) {
    return decode_media_instructions(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x00000000 /* op1(27:25)=00x */) {
    return decode_data_processing_and_miscellaneous_instructions(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x08000000 /* op1(27:25)=10x */) {
    return decode_branch_branch_with_link_and_block_data_transfer(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x0C000000 /* op1(27:25)=11x */) {
    return decode_coprocessor_instructions_and_supervisor_call(inst);
  }

  if ((inst.Bits() & 0xF0000000) == 0xF0000000 /* cond(31:28)=1111 */) {
    return decode_unconditional_instructions(inst);
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table advanced_simd_data_processing_instructions.
 * Specified by: ('See Section A7.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_advanced_simd_data_processing_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x00B80000) == 0x00800000 /* A(23:19)=1x000 */ &&
      (inst.Bits() & 0x00000090) == 0x00000010 /* C(7:4)=0xx1 */) {
    return decode_simd_dp_1imm(inst);
  }

  if ((inst.Bits() & 0x00B80000) == 0x00880000 /* A(23:19)=1x001 */ &&
      (inst.Bits() & 0x00000090) == 0x00000010 /* C(7:4)=0xx1 */) {
    return decode_simd_dp_2shift(inst);
  }

  if ((inst.Bits() & 0x00B00000) == 0x00900000 /* A(23:19)=1x01x */ &&
      (inst.Bits() & 0x00000090) == 0x00000010 /* C(7:4)=0xx1 */) {
    return decode_simd_dp_2shift(inst);
  }

  if ((inst.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19)=1x10x */ &&
      (inst.Bits() & 0x00000050) == 0x00000000 /* C(7:4)=x0x0 */) {
    return decode_simd_dp_3diff(inst);
  }

  if ((inst.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19)=1x10x */ &&
      (inst.Bits() & 0x00000050) == 0x00000040 /* C(7:4)=x1x0 */) {
    return decode_simd_dp_2scalar(inst);
  }

  if ((inst.Bits() & 0x00A00000) == 0x00800000 /* A(23:19)=1x0xx */ &&
      (inst.Bits() & 0x00000050) == 0x00000000 /* C(7:4)=x0x0 */) {
    return decode_simd_dp_3diff(inst);
  }

  if ((inst.Bits() & 0x00A00000) == 0x00800000 /* A(23:19)=1x0xx */ &&
      (inst.Bits() & 0x00000050) == 0x00000040 /* C(7:4)=x1x0 */) {
    return decode_simd_dp_2scalar(inst);
  }

  if ((inst.Bits() & 0x00A00000) == 0x00A00000 /* A(23:19)=1x1xx */ &&
      (inst.Bits() & 0x00000090) == 0x00000010 /* C(7:4)=0xx1 */) {
    return decode_simd_dp_2shift(inst);
  }

  if ((inst.Bits() & 0x00800000) == 0x00000000 /* A(23:19)=0xxxx */) {
    return decode_simd_dp_3same(inst);
  }

  if ((inst.Bits() & 0x00800000) == 0x00800000 /* A(23:19)=1xxxx */ &&
      (inst.Bits() & 0x00000090) == 0x00000090 /* C(7:4)=1xx1 */) {
    return decode_simd_dp_2shift(inst);
  }

  if ((inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* C(7:4)=xxx0 */) {
    return VectorBinary3RegisterImmOp_Vext_Rule_305_A1_P598_instance_;
  }

  if ((inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000F00) == 0x00000C00 /* B(11:8)=1100 */ &&
      (inst.Bits() & 0x00000090) == 0x00000000 /* C(7:4)=0xx0 */) {
    return VectorUnary2RegisterDup_Vdup_Rule_302_A1_P592_instance_;
  }

  if ((inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000C00) == 0x00000800 /* B(11:8)=10xx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* C(7:4)=xxx0 */) {
    return VectorBinary3RegisterLookupOp_Vtbl_Vtbx_Rule_406_A1_P798_instance_;
  }

  if ((inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000800) == 0x00000000 /* B(11:8)=0xxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* C(7:4)=xxx0 */) {
    return decode_simd_dp_2misc(inst);
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table advanced_simd_element_or_structure_load_store_instructions.
 * Specified by: ('See Section A7.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_advanced_simd_element_or_structure_load_store_instructions(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000300 /* B(11:8)=0011 */) {
    return VectorLoadStoreMultiple2_VST2_multiple_2_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000700) == 0x00000200 /* B(11:8)=x010 */) {
    return VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000000 /* B(11:8)=000x */) {
    return VectorLoadStoreMultiple4_VST4_multiple_4_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000400 /* B(11:8)=010x */) {
    return VectorLoadStoreMultiple3_VST3_multiple_3_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000600 /* B(11:8)=011x */) {
    return VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000800 /* B(11:8)=100x */) {
    return VectorLoadStoreMultiple2_VST2_multiple_2_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000800 /* B(11:8)=1000 */) {
    return VectorLoadStoreSingle1_VST1_single_element_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000900 /* B(11:8)=1001 */) {
    return VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000A00 /* B(11:8)=1010 */) {
    return VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000B00 /* B(11:8)=1011 */) {
    return VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00) == 0x00000000 /* B(11:8)=0x00 */) {
    return VectorLoadStoreSingle1_VST1_single_element_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00) == 0x00000100 /* B(11:8)=0x01 */) {
    return VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00) == 0x00000200 /* B(11:8)=0x10 */) {
    return VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00) == 0x00000300 /* B(11:8)=0x11 */) {
    return VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000300 /* B(11:8)=0011 */) {
    return VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000700) == 0x00000200 /* B(11:8)=x010 */) {
    return VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000000 /* B(11:8)=000x */) {
    return VectorLoadStoreMultiple4_VLD4_multiple_4_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000400 /* B(11:8)=010x */) {
    return VectorLoadStoreMultiple3_VLD3_multiple_3_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000600 /* B(11:8)=011x */) {
    return VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000800 /* B(11:8)=100x */) {
    return VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000800 /* B(11:8)=1000 */) {
    return VectorLoadStoreSingle1_VLD1_single_element_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000900 /* B(11:8)=1001 */) {
    return VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000A00 /* B(11:8)=1010 */) {
    return VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000B00 /* B(11:8)=1011 */) {
    return VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000C00 /* B(11:8)=1100 */) {
    return VectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000D00 /* B(11:8)=1101 */) {
    return VectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000E00 /* B(11:8)=1110 */) {
    return VectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* B(11:8)=1111 */) {
    return VectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00) == 0x00000000 /* B(11:8)=0x00 */) {
    return VectorLoadStoreSingle1_VLD1_single_element_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00) == 0x00000100 /* B(11:8)=0x01 */) {
    return VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00) == 0x00000200 /* B(11:8)=0x10 */) {
    return VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00) == 0x00000300 /* B(11:8)=0x11 */) {
    return VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table branch_branch_with_link_and_block_data_transfer.
 * Specified by: ('See Section A5.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_branch_branch_with_link_and_block_data_transfer(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x03F00000) == 0x00900000 /* op(25:20)=001001 */) {
    return LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_;
  }

  if ((inst.Bits() & 0x03F00000) == 0x00B00000 /* op(25:20)=001011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) {
    return LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_;
  }

  if ((inst.Bits() & 0x03F00000) == 0x00B00000 /* op(25:20)=001011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) {
    return LoadRegisterList_Pop_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x03F00000) == 0x01000000 /* op(25:20)=010000 */) {
    return StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_;
  }

  if ((inst.Bits() & 0x03F00000) == 0x01200000 /* op(25:20)=010010 */ &&
      (inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) {
    return StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_;
  }

  if ((inst.Bits() & 0x03F00000) == 0x01200000 /* op(25:20)=010010 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) {
    return StoreRegisterList_Push_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x03D00000) == 0x00000000 /* op(25:20)=0000x0 */) {
    return StoreRegisterList_Stmda_Stmed_Rule_190_A1_P376_instance_;
  }

  if ((inst.Bits() & 0x03D00000) == 0x00100000 /* op(25:20)=0000x1 */) {
    return LoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112_instance_;
  }

  if ((inst.Bits() & 0x03D00000) == 0x00800000 /* op(25:20)=0010x0 */) {
    return StoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374_instance_;
  }

  if ((inst.Bits() & 0x03D00000) == 0x01100000 /* op(25:20)=0100x1 */) {
    return LoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114_instance_;
  }

  if ((inst.Bits() & 0x03D00000) == 0x01800000 /* op(25:20)=0110x0 */) {
    return StoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380_instance_;
  }

  if ((inst.Bits() & 0x03D00000) == 0x01900000 /* op(25:20)=0110x1 */) {
    return LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116_instance_;
  }

  if ((inst.Bits() & 0x02500000) == 0x00400000 /* op(25:20)=0xx1x0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22_instance_;
  }

  if ((inst.Bits() & 0x02500000) == 0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000) == 0x00000000 /* R(15)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7_instance_;
  }

  if ((inst.Bits() & 0x02500000) == 0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000) == 0x00008000 /* R(15)=1 */) {
    return ForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5_instance_;
  }

  if ((inst.Bits() & 0x03000000) == 0x02000000 /* op(25:20)=10xxxx */) {
    return BranchImmediate24_B_Rule_16_A1_P44_instance_;
  }

  if ((inst.Bits() & 0x03000000) == 0x03000000 /* op(25:20)=11xxxx */) {
    return BranchImmediate24_Bl_Blx_Rule_23_A1_P58_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table coprocessor_instructions_and_supervisor_call.
 * Specified by: ('See Section A5.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_coprocessor_instructions_and_supervisor_call(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x03E00000) == 0x00000000 /* op1(25:20)=00000x */) {
    return UndefinedCondDecoder_Undefined_A5_6_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03F00000) == 0x00400000 /* op1(25:20)=000100 */) {
    return ForbiddenCondDecoder_Mcrr_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03F00000) == 0x00500000 /* op1(25:20)=000101 */) {
    return ForbiddenCondDecoder_Mrrc_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03100000) == 0x02000000 /* op1(25:20)=10xxx0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */) {
    return ForbiddenCondDecoder_Mcr_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03100000) == 0x02100000 /* op1(25:20)=10xxx1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */) {
    return ForbiddenCondDecoder_Mrc_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000) == 0x00000000 /* op1(25:20)=0xxxx0 */ &&
      (inst.Bits() & 0x03B00000) != 0x00000000 /* op1_repeated(25:20)=~000x00 */) {
    return ForbiddenCondDecoder_Stc_Rule_A2_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000) == 0x00100000 /* op1(25:20)=0xxxx1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x03B00000) != 0x00100000 /* op1_repeated(25:20)=~000x01 */) {
    return ForbiddenCondDecoder_Ldc_immediate_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000) == 0x00100000 /* op1(25:20)=0xxxx1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x03B00000) != 0x00100000 /* op1_repeated(25:20)=~000x01 */) {
    return ForbiddenCondDecoder_Ldc_literal_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */) {
    return ForbiddenCondDecoder_Cdp_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20)=00010x */) {
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */) {
    return decode_floating_point_data_processing_instructions(inst);
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */) {
    return decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(inst);
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x02000000) == 0x00000000 /* op1(25:20)=0xxxxx */ &&
      (inst.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20)=~000x0x */) {
    return decode_extension_register_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x03000000) == 0x03000000 /* op1(25:20)=11xxxx */) {
    return ForbiddenCondDecoder_Svc_Rule_A1_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table data_processing_and_miscellaneous_instructions.
 * Specified by: ('See Section A5.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_data_processing_and_miscellaneous_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */ &&
      (inst.Bits() & 0x00000090) == 0x00000010 /* op2(7:4)=0xx1 */) {
    return decode_data_processing_register_shifted_register(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */) {
    return decode_data_processing_register(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) == 0x01000000 /* op1(24:20)=10xx0 */ &&
      (inst.Bits() & 0x00000090) == 0x00000080 /* op2(7:4)=1xx0 */) {
    return decode_halfword_multiply_and_multiply_accumulate(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) == 0x01000000 /* op1(24:20)=10xx0 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:4)=0xxx */) {
    return decode_miscellaneous_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20)=~0xx1x */ &&
      (inst.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4)=1011 */) {
    return decode_extra_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20)=~0xx1x */ &&
      (inst.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4)=11x1 */) {
    return decode_extra_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) == 0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4)=1011 */) {
    return ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) == 0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4)=11x1 */) {
    return ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* op1(24:20)=0xxxx */ &&
      (inst.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4)=1001 */) {
    return decode_multiply_and_multiply_accumulate(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* op1(24:20)=1xxxx */ &&
      (inst.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4)=1001 */) {
    return decode_synchronization_primitives(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01F00000) == 0x01000000 /* op1(24:20)=10000 */) {
    return Unary1RegisterImmediateOpDynCodeReplace_MOVW_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01F00000) == 0x01400000 /* op1(24:20)=10100 */) {
    return Unary1RegisterImmediateOpDynCodeReplace_MOVT_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01B00000) == 0x01200000 /* op1(24:20)=10x10 */) {
    return decode_msr_immediate_and_hints(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */) {
    return decode_data_processing_immediate(inst);
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table data_processing_immediate.
 * Specified by: ('See Section A5.2.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_data_processing_immediate(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01F00000) == 0x01100000 /* op(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return MaskedBinaryRegisterImmediateTest_TST_immediate_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01300000 /* op(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return BinaryRegisterImmediateTest_TEQ_immediate_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01500000 /* op(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return BinaryRegisterImmediateTest_CMP_immediate_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01700000 /* op(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return BinaryRegisterImmediateTest_CMN_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00000000 /* op(24:20)=0000x */) {
    return Binary2RegisterImmediateOp_AND_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00200000 /* op(24:20)=0001x */) {
    return Binary2RegisterImmediateOp_EOR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* op(24:20)=0010x */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) {
    return Binary2RegisterImmediateOpAddSub_SUB_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* op(24:20)=0010x */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) {
    return Unary1RegisterImmediateOpPc_ADR_A2_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00600000 /* op(24:20)=0011x */) {
    return Binary2RegisterImmediateOp_RSB_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00800000 /* op(24:20)=0100x */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) {
    return Binary2RegisterImmediateOpAddSub_ADD_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00800000 /* op(24:20)=0100x */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) {
    return Unary1RegisterImmediateOpPc_ADR_A1_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00A00000 /* op(24:20)=0101x */) {
    return Binary2RegisterImmediateOp_ADC_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00C00000 /* op(24:20)=0110x */) {
    return Binary2RegisterImmediateOp_SBC_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00E00000 /* op(24:20)=0111x */) {
    return Binary2RegisterImmediateOp_RSC_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01800000 /* op(24:20)=1100x */) {
    return Binary2RegisterImmediateOpDynCodeReplace_ORR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op(24:20)=1101x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op(24:20)=1110x */) {
    return MaskedBinary2RegisterImmediateOp_BIC_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01E00000 /* op(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary1RegisterImmediateOp12DynCodeReplace_MVN_immediate_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table data_processing_register.
 * Specified by: ('See Section A5.2.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_data_processing_register(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary2RegisterImmedShiftedTest_TST_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary2RegisterImmedShiftedTest_TEQ_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary2RegisterImmedShiftedTest_CMP_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary2RegisterImmedShiftedTest_CMN_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20)=0000x */) {
    return Binary3RegisterShiftedOp_AND_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20)=0001x */) {
    return Binary3RegisterShiftedOp_EOR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20)=0010x */) {
    return Binary3RegisterShiftedOp_SUB_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20)=0011x */) {
    return Binary3RegisterShiftedOp_RSB_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20)=0100x */) {
    return Binary3RegisterShiftedOp_ADD_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20)=0101x */) {
    return Binary3RegisterShiftedOp_ADC_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20)=0110x */) {
    return Binary3RegisterShiftedOp_SBC_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20)=0111x */) {
    return Binary3RegisterShiftedOp_RSC_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20)=1100x */) {
    return Binary3RegisterShiftedOp_ORR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op3(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOpImmNotZero_LSL_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */ &&
      (inst.Bits() & 0x00000060) == 0x00000060 /* op3(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOpImmNotZero_ROR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op3(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterOp_MOV_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */ &&
      (inst.Bits() & 0x00000060) == 0x00000060 /* op3(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterOp_RRX_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000020 /* op3(6:5)=01 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOp_LSR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op3(6:5)=10 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOp_ASR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */) {
    return Binary3RegisterShiftedOp_BIC_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOp_MVN_register_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table data_processing_register_shifted_register.
 * Specified by: ('See Section A5.2.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_data_processing_register_shifted_register(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterShiftedTest_TST_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterShiftedTest_TEQ_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterShiftedTest_CMP_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterShiftedTest_CMN_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20)=0000x */) {
    return Binary4RegisterShiftedOp_AND_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20)=0001x */) {
    return Binary4RegisterShiftedOp_EOR_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20)=0010x */) {
    return Binary4RegisterShiftedOp_SUB_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20)=0011x */) {
    return Binary4RegisterShiftedOp_RSB_register_shfited_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20)=0100x */) {
    return Binary4RegisterShiftedOp_ADD_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20)=0101x */) {
    return Binary4RegisterShiftedOp_ADC_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20)=0110x */) {
    return Binary4RegisterShiftedOp_SBC_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20)=0111x */) {
    return Binary4RegisterShiftedOp_RSC_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20)=1100x */) {
    return Binary4RegisterShiftedOp_ORR_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op2(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Binary3RegisterOp_LSL_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Binary3RegisterOp_LSR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Binary3RegisterOp_ASR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Binary3RegisterOp_ROR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */) {
    return Binary4RegisterShiftedOp_BIC_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary3RegisterShiftedOp_MVN_register_shifted_register_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table extension_register_load_store_instructions.
 * Specified by: ('A7.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_extension_register_load_store_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x01B00000) == 0x00900000 /* opcode(24:20)=01x01 */) {
    return LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_;
  }

  if ((inst.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20)=01x11 */ &&
      (inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) {
    return LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_;
  }

  if ((inst.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20)=01x11 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) {
    return LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694_instance_;
  }

  if ((inst.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20)=10x10 */ &&
      (inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) {
    return StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_;
  }

  if ((inst.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20)=10x10 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) {
    return StoreVectorRegisterList_Vpush_355_A1_A2_P696_instance_;
  }

  if ((inst.Bits() & 0x01B00000) == 0x01300000 /* opcode(24:20)=10x11 */) {
    return LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* opcode(24:20)=0010x */) {
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);
  }

  if ((inst.Bits() & 0x01300000) == 0x01000000 /* opcode(24:20)=1xx00 */) {
    return StoreVectorRegister_Vstr_Rule_400_A1_A2_P786_instance_;
  }

  if ((inst.Bits() & 0x01300000) == 0x01100000 /* opcode(24:20)=1xx01 */) {
    return LoadVectorRegister_Vldr_Rule_320_A1_A2_P628_instance_;
  }

  if ((inst.Bits() & 0x01900000) == 0x00800000 /* opcode(24:20)=01xx0 */) {
    return StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table extra_load_store_instructions.
 * Specified by: ('See Section A5.2.8',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_extra_load_store_instructions(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Store3RegisterOp_STRH_register_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterOp_LDRH_register_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */) {
    return Store2RegisterImm8Op_STRH_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8Op_LDRH_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) {
    return LoadRegisterImm8Op_LDRH_literal_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterDoubleOp_LDRD_register_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterOp_LDRSB_register_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8DoubleOp_LDRD_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return LoadRegisterImm8DoubleOp_LDRD_literal_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8Op_LDRSB_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Store3RegisterDoubleOp_STRD_register_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterOp_LDRSH_register_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */) {
    return Store2RegisterImm8DoubleOp_STRD_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8Op_LDRSH_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000040) == 0x00000040 /* op2(6:5)=1x */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return LoadRegisterImm8Op_LDRSB_literal_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table floating_point_data_processing_instructions.
 * Specified by: ('A7.5 Table A7 - 16',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_floating_point_data_processing_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x00B00000) == 0x00000000 /* opc1(23:20)=0x00 */) {
    return CondVfpOp_Vmla_vmls_Rule_423_A2_P636_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00100000 /* opc1(23:20)=0x01 */) {
    return CondVfpOp_Vnmla_vnmls_Rule_343_A1_P674_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00200000 /* opc1(23:20)=0x10 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */) {
    return CondVfpOp_Vmul_Rule_338_A2_P664_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00200000 /* opc1(23:20)=0x10 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_Vnmul_Rule_343_A2_P674_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20)=0x11 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */) {
    return CondVfpOp_Vadd_Rule_271_A2_P536_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20)=0x11 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_Vsub_Rule_402_A2_P790_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00800000 /* opc1(23:20)=1x00 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */) {
    return CondVfpOp_Vdiv_Rule_301_A1_P590_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00900000 /* opc1(23:20)=1x01 */) {
    return CondVfpOp_Vfnma_vfnms_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00A00000 /* opc1(23:20)=1x10 */) {
    return CondVfpOp_Vfma_vfms_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00B00000 /* opc1(23:20)=1x11 */) {
    return decode_other_floating_point_data_processing_instructions(inst);
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table halfword_multiply_and_multiply_accumulate.
 * Specified by: ('See Section A5.2.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_halfword_multiply_and_multiply_accumulate(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00600000) == 0x00000000 /* op1(22:21)=00 */) {
    return Binary4RegisterDualOp_SMLABB_SMLABT_SMLATB_SMLATT_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op(5)=0 */) {
    return Binary4RegisterDualOp_SMLAWB_SMLAWT_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltA_SMULWB_SMULWT_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00400000 /* op1(22:21)=10 */) {
    return Binary4RegisterDualResult_SMLALBB_SMLALBT_SMLALTB_SMLALTT_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00600000 /* op1(22:21)=11 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltA_SMULBB_SMULBT_SMULTB_SMULTT_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table load_store_word_and_unsigned_byte.
 * Specified by: ('See Section A5.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_load_store_word_and_unsigned_byte(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000) == 0x00200000 /* op1(24:20)=0x010 */) {
    return ForbiddenCondDecoder_STRT_A1_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000) == 0x00300000 /* op1(24:20)=0x011 */) {
    return ForbiddenCondDecoder_LDRT_A1_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000) == 0x00600000 /* op1(24:20)=0x110 */) {
    return ForbiddenCondDecoder_STRBT_A1_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000) == 0x00700000 /* op1(24:20)=0x111 */) {
    return ForbiddenCondDecoder_LDRBT_A1_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x01700000) != 0x00200000 /* op1_repeated(24:20)=~0x010 */) {
    return Store2RegisterImm12Op_STR_immediate_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */) {
    return LdrImmediateOp_LDR_immediate_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return Load2RegisterImm12Op_LDR_literal_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x01700000) != 0x00600000 /* op1_repeated(24:20)=~0x110 */) {
    return Store2RegisterImm12Op_STRB_immediate_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */) {
    return Load2RegisterImm12Op_LDRB_immediate_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return Load2RegisterImm12Op_LDRB_literal_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000) == 0x00200000 /* op1(24:20)=0x010 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_STRT_A2_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000) == 0x00300000 /* op1(24:20)=0x011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_LDRT_A2_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000) == 0x00600000 /* op1(24:20)=0x110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_STRBT_A2_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000) == 0x00700000 /* op1(24:20)=0x111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_LDRBT_A2_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00200000 /* op1_repeated(24:20)=~0x010 */) {
    return Store3RegisterImm5Op_STR_register_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */) {
    return Load3RegisterImm5Op_LDR_register_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00600000 /* op1_repeated(24:20)=~0x110 */) {
    return Store3RegisterImm5Op_STRB_register_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */) {
    return Load3RegisterImm5Op_LDRB_register_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table media_instructions.
 * Specified by: ('See Section A5.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_media_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=~1111 */) {
    return Binary4RegisterDualOp_Usada8_Rule_254_A1_P502_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12)=1111 */) {
    return Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01F00000 /* op1(24:20)=11111 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */) {
    return PermanentlyUndefined_Udf_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op2(7:5)=x10 */) {
    return Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0)=~1111 */) {
    return Binary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0)=1111 */) {
    return Unary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op2(7:5)=x10 */) {
    return Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466_instance_;
  }

  if ((inst.Bits() & 0x01C00000) == 0x00000000 /* op1(24:20)=000xx */) {
    return decode_parallel_addition_and_subtraction_signed(inst);
  }

  if ((inst.Bits() & 0x01C00000) == 0x00400000 /* op1(24:20)=001xx */) {
    return decode_parallel_addition_and_subtraction_unsigned(inst);
  }

  if ((inst.Bits() & 0x01800000) == 0x00800000 /* op1(24:20)=01xxx */) {
    return decode_packing_unpacking_saturation_and_reversal(inst);
  }

  if ((inst.Bits() & 0x01800000) == 0x01000000 /* op1(24:20)=10xxx */) {
    return decode_signed_multiply_signed_and_unsigned_divide(inst);
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table memory_hints_advanced_simd_instructions_and_miscellaneous_instructions.
 * Specified by: ('See Section A5.7.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000000 /* op2(7:4)=0000 */ &&
      (inst.Bits() & 0x00010000) == 0x00010000 /* Rn(19:16)=xxx1 */ &&
      (inst.Bits() & 0x000EFD0F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000 */) {
    return ForbiddenUncondDecoder_Setend_Rule_157_P314_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:4)=xx0x */ &&
      (inst.Bits() & 0x00010000) == 0x00000000 /* Rn(19:16)=xxx0 */ &&
      (inst.Bits() & 0x0000FE00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx */) {
    return ForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05300000 /* op1(26:20)=1010011 */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000000 /* op2(7:4)=0000 */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000010 /* op2(7:4)=0001 */ &&
      (inst.Bits() & 0x000FFF0F) == 0x000FF00F /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111 */) {
    return ForbiddenUncondDecoder_Clrex_Rule_30_A1_P70_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000040 /* op2(7:4)=0100 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return DataBarrier_Dsb_Rule_42_A1_P92_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000050 /* op2(7:4)=0101 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return DataBarrier_Dmb_Rule_41_A1_P90_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000060 /* op2(7:4)=0110 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return InstructionBarrier_Isb_Rule_49_A1_P102_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000070 /* op2(7:4)=0111 */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:4)=001x */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x00000080) == 0x00000080 /* op2(7:4)=1xxx */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x04100000 /* op1(26:20)=100x001 */) {
    return ForbiddenUncondDecoder_Unallocated_hints_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x04500000 /* op1(26:20)=100x101 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_Pli_Rule_120_A1_P242_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x05100000 /* op1(26:20)=101x001 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_Pldw_Rule_117_A1_P236_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x05100000 /* op1(26:20)=101x001 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x05500000 /* op1(26:20)=101x101 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_Pld_Rule_117_A1_P236_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x05500000 /* op1(26:20)=101x101 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_Pld_Rule_118_A1_P238_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x06100000 /* op1(26:20)=110x001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */) {
    return ForbiddenUncondDecoder_Unallocated_hints_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x06500000 /* op1(26:20)=110x101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOp_Pli_Rule_121_A1_P244_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x07100000 /* op1(26:20)=111x001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOp_Pldw_Rule_119_A1_P240_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x07500000 /* op1(26:20)=111x101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOp_Pld_Rule_119_A1_P240_instance_;
  }

  if ((inst.Bits() & 0x07B00000) == 0x05B00000 /* op1(26:20)=1011x11 */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07300000) == 0x04300000 /* op1(26:20)=100xx11 */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x06300000) == 0x06300000 /* op1(26:20)=11xxx11 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */) {
    return UnpredictableUncondDecoder_Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07100000) == 0x04000000 /* op1(26:20)=100xxx0 */) {
    return decode_advanced_simd_element_or_structure_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x06000000) == 0x02000000 /* op1(26:20)=01xxxxx */) {
    return decode_advanced_simd_data_processing_instructions(inst);
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table miscellaneous_instructions.
 * Specified by: ('See Section A5.2.12',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_miscellaneous_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00030000) == 0x00000000 /* op1(19:16)=xx00 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Unary1RegisterUse_MSR_register_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00030000) == 0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return ForbiddenCondDecoder_MSR_register_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00020000) == 0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return ForbiddenCondDecoder_MSR_register_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return ForbiddenCondDecoder_MSR_register_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x000F0D0F) == 0x000F0000 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000 */) {
    return Unary1RegisterSet_MRS_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x00000C0F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000 */) {
    return ForbiddenCondDecoder_MRS_Banked_register_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* op(22:21)=x1 */ &&
      (inst.Bits() & 0x0000FC00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx */) {
    return ForbiddenCondDecoder_MSR_Banked_register_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000010 /* op2(6:4)=001 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return BranchToRegister_Bx_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000010 /* op2(6:4)=001 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPc_CLZ_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000020 /* op2(6:4)=010 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return ForbiddenCondDecoder_BXJ_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000030 /* op2(6:4)=011 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return BranchToRegister_BLX_register_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000050 /* op2(6:4)=101 */) {
    return decode_saturating_addition_and_subtraction(inst);
  }

  if ((inst.Bits() & 0x00000070) == 0x00000060 /* op2(6:4)=110 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF0F) == 0x0000000E /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110 */) {
    return ForbiddenCondDecoder_ERET_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */) {
    return BreakPointAndConstantPoolHead_BKPT_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00400000 /* op(22:21)=10 */) {
    return ForbiddenCondDecoder_HVC_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx */) {
    return ForbiddenCondDecoder_SMC_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table msr_immediate_and_hints.
 * Specified by: ('See Section A5.2.11',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_msr_immediate_and_hints(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000000 /* op2(7:0)=00000000 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return CondDecoder_Nop_Rule_110_A1_P222_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000001 /* op2(7:0)=00000001 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return CondDecoder_Yield_Rule_413_A1_P812_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000002 /* op2(7:0)=00000010 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return ForbiddenCondDecoder_Wfe_Rule_411_A1_P808_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000003 /* op2(7:0)=00000011 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return ForbiddenCondDecoder_Wfi_Rule_412_A1_P810_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000004 /* op2(7:0)=00000100 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return ForbiddenCondDecoder_Sev_Rule_158_A1_P316_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000F0) == 0x000000F0 /* op2(7:0)=1111xxxx */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return ForbiddenCondDecoder_Dbg_Rule_40_A1_P88_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00040000 /* op1(19:16)=0100 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000B0000) == 0x00080000 /* op1(19:16)=1x00 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00030000) == 0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00020000) == 0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00400000 /* op(22)=1 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Forbidden_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table multiply_and_multiply_accumulate.
 * Specified by: ('See Section A5.2.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_multiply_and_multiply_accumulate(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00F00000) == 0x00400000 /* op(23:20)=0100 */) {
    return Binary4RegisterDualResult_UMAAL_A1_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00600000 /* op(23:20)=0110 */) {
    return Binary4RegisterDualOp_MLS_A1_instance_;
  }

  if ((inst.Bits() & 0x00D00000) == 0x00500000 /* op(23:20)=01x1 */) {
    return UndefinedCondDecoder_None_instance_;
  }

  if ((inst.Bits() & 0x00E00000) == 0x00000000 /* op(23:20)=000x */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltA_MUL_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000) == 0x00200000 /* op(23:20)=001x */) {
    return Binary4RegisterDualOpLtV6RdNotRn_MLA_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000) == 0x00800000 /* op(23:20)=100x */) {
    return Binary4RegisterDualResultUsesRnRm_UMULL_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000) == 0x00A00000 /* op(23:20)=101x */) {
    return Binary4RegisterDualResultLtV6RdHiLoNotRn_UMLAL_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000) == 0x00C00000 /* op(23:20)=110x */) {
    return Binary4RegisterDualResultUsesRnRm_SMULL_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000) == 0x00E00000 /* op(23:20)=111x */) {
    return Binary4RegisterDualResultLtV6RdHiLoNotRn_SMLAL_A1_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table other_floating_point_data_processing_instructions.
 * Specified by: ('A7.5 Table A7 - 17',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_other_floating_point_data_processing_instructions(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x000F0000) == 0x00000000 /* opc2(19:16)=0000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* opc3(7:6)=01 */) {
    return CondVfpOp_Vmov_Rule_327_A2_P642_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00000000 /* opc2(19:16)=0000 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6)=11 */) {
    return CondVfpOp_Vabs_Rule_269_A2_P532_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16)=0001 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* opc3(7:6)=01 */) {
    return CondVfpOp_Vneg_Rule_342_A2_P672_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16)=0001 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6)=11 */) {
    return CondVfpOp_Vsqrt_Rule_388_A1_P762_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00040000 /* opc2(19:16)=0100 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_Vcmp_Vcmpe_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00050000 /* opc2(19:16)=0101 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x0000002F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000 */) {
    return CondVfpOp_Vcmp_Vcmpe_Rule_A2_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00070000 /* opc2(19:16)=0111 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6)=11 */) {
    return CondVfpOp_Vcvt_Rule_298_A1_P584_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00080000 /* opc2(19:16)=1000 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_;
  }

  if ((inst.Bits() & 0x000E0000) == 0x00020000 /* opc2(19:16)=001x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx */) {
    return CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588_instance_;
  }

  if ((inst.Bits() & 0x000E0000) == 0x000C0000 /* opc2(19:16)=110x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_;
  }

  if ((inst.Bits() & 0x000A0000) == 0x000A0000 /* opc2(19:16)=1x1x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_Vcvt_Rule_297_A1_P582_instance_;
  }

  if ((inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */ &&
      (inst.Bits() & 0x000000A0) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx */) {
    return CondVfpOp_Vmov_Rule_326_A2_P640_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table packing_unpacking_saturation_and_reversal.
 * Specified by: ('See Section A5.4.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_packing_unpacking_saturation_and_reversal(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:5)=xx0 */) {
    return Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op1(22:20)=01x */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:5)=xx0 */) {
    return Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00600000 /* op1(22:20)=11x */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:5)=xx0 */) {
    return Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table parallel_addition_and_subtraction_signed.
 * Specified by: ('See Section A5.4.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_addition_and_subtraction_signed(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table parallel_addition_and_subtraction_unsigned.
 * Specified by: ('See Section A5.4.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_addition_and_subtraction_unsigned(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table saturating_addition_and_subtraction.
 * Specified by: ('See Section A5.2.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_saturating_addition_and_subtraction(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00600000) == 0x00000000 /* op(22:21)=00 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QADD_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QSUB_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00400000 /* op(22:21)=10 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QDADD_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QDSUB_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table signed_multiply_signed_and_unsigned_divide.
 * Specified by: ('See Section A5.4.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_signed_multiply_signed_and_unsigned_divide(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) {
    return Binary4RegisterDualOpNoCondsUpdate_SMLAD_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */) {
    return Binary3RegisterOpAltANoCondsUpdate_SMUAD_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5)=01x */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) {
    return Binary4RegisterDualOpNoCondsUpdate_SMLSD_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5)=01x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */) {
    return Binary3RegisterOpAltANoCondsUpdate_SMUSD_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00100000 /* op1(22:20)=001 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltANoCondsUpdate_SDIV_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltANoCondsUpdate_UDIV_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */) {
    return Binary4RegisterDualResultNoCondsUpdate_SMLALD_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5)=01x */) {
    return Binary4RegisterDualResultNoCondsUpdate_SMLSLD_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) {
    return Binary4RegisterDualOpNoCondsUpdate_SMMLA_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */) {
    return Binary3RegisterOpAltANoCondsUpdate_SMMUL_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* op2(7:5)=11x */) {
    return Binary4RegisterDualOpNoCondsUpdate_SMMLS_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table simd_dp_1imm.
 * Specified by: ('See Section A7.4.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_1imm(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000020) == 0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000D00) == 0x00000800 /* cmode(11:8)=10x0 */) {
    return Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000D00) == 0x00000900 /* cmode(11:8)=10x1 */) {
    return Vector1RegisterImmediate_BIT_VORR_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000900) == 0x00000000 /* cmode(11:8)=0xx0 */) {
    return Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000900) == 0x00000100 /* cmode(11:8)=0xx1 */) {
    return Vector1RegisterImmediate_BIT_VORR_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000C00) == 0x00000C00 /* cmode(11:8)=11xx */) {
    return Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000E00 /* cmode(11:8)=1110 */) {
    return Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* cmode(11:8)=1111 */) {
    return Undefined_None_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000D00) == 0x00000800 /* cmode(11:8)=10x0 */) {
    return Vector1RegisterImmediate_MVN_VMVN_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000D00) == 0x00000900 /* cmode(11:8)=10x1 */) {
    return Vector1RegisterImmediate_BIT_VBIC_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000E00) == 0x00000C00 /* cmode(11:8)=110x */) {
    return Vector1RegisterImmediate_MVN_VMVN_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000900) == 0x00000000 /* cmode(11:8)=0xx0 */) {
    return Vector1RegisterImmediate_MVN_VMVN_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000900) == 0x00000100 /* cmode(11:8)=0xx1 */) {
    return Vector1RegisterImmediate_BIT_VBIC_immediate_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table simd_dp_2misc.
 * Specified by: ('See Section A7.4.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2misc(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000000 /* B(10:6)=0000x */) {
    return Vector2RegisterMiscellaneous_RG_VREV64_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000080 /* B(10:6)=0001x */) {
    return Vector2RegisterMiscellaneous_RG_VREV32_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000100 /* B(10:6)=0010x */) {
    return Vector2RegisterMiscellaneous_RG_VREV16_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000400 /* B(10:6)=1000x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCLS_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000480 /* B(10:6)=1001x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCLZ_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000500 /* B(10:6)=1010x */) {
    return Vector2RegisterMiscellaneous_V8_VCNT_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000580 /* B(10:6)=1011x */) {
    return Vector2RegisterMiscellaneous_V8_VMVN_register_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000700 /* B(10:6)=1110x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VQABS_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780) == 0x00000780 /* B(10:6)=1111x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VQNEG_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000700) == 0x00000200 /* B(10:6)=010xx */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VPADDL_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000700) == 0x00000600 /* B(10:6)=110xx */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VPADAL_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000000 /* B(10:6)=0000x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000080 /* B(10:6)=0001x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000100 /* B(10:6)=0010x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000180 /* B(10:6)=0011x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000200 /* B(10:6)=0100x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000300 /* B(10:6)=0110x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VABS_A1_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000380 /* B(10:6)=0111x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VNEG_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000400 /* B(10:6)=1000x */) {
    return Vector2RegisterMiscellaneous_F32_VCGT_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000480 /* B(10:6)=1001x */) {
    return Vector2RegisterMiscellaneous_F32_VCGE_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000500 /* B(10:6)=1010x */) {
    return Vector2RegisterMiscellaneous_F32_VCEQ_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000580 /* B(10:6)=1011x */) {
    return Vector2RegisterMiscellaneous_F32_VCLE_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000600 /* B(10:6)=1100x */) {
    return Vector2RegisterMiscellaneous_F32_VCLT_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000700 /* B(10:6)=1110x */) {
    return Vector2RegisterMiscellaneous_F32_VABS_A1_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780) == 0x00000780 /* B(10:6)=1111x */) {
    return Vector2RegisterMiscellaneous_F32_VNEG_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0) == 0x00000200 /* B(10:6)=01000 */) {
    return Vector2RegisterMiscellaneous_V16_32_64N_VMOVN_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0) == 0x00000240 /* B(10:6)=01001 */) {
    return Vector2RegisterMiscellaneous_I16_32_64N_VQMOVUN_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0) == 0x00000300 /* B(10:6)=01100 */) {
    return Vector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000006C0) == 0x00000600 /* B(10:6)=11x00 */) {
    return Vector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780) == 0x00000000 /* B(10:6)=0000x */) {
    return Vector2RegisterMiscellaneous_V8S_VSWP_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780) == 0x00000080 /* B(10:6)=0001x */) {
    return Vector2RegisterMiscellaneous_V8_16_32T_VTRN_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780) == 0x00000100 /* B(10:6)=0010x */) {
    return Vector2RegisterMiscellaneous_V8_16_32I_VUZP_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780) == 0x00000180 /* B(10:6)=0011x */) {
    return Vector2RegisterMiscellaneous_V8_16_32I_VZIP_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780) == 0x00000280 /* B(10:6)=0101x */) {
    return Vector2RegisterMiscellaneous_I16_32_64N_VQMOVN_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00030000 /* A(17:16)=11 */ &&
      (inst.Bits() & 0x00000680) == 0x00000400 /* B(10:6)=10x0x */) {
    return Vector2RegisterMiscellaneous_F32_VRECPE_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00030000 /* A(17:16)=11 */ &&
      (inst.Bits() & 0x00000680) == 0x00000480 /* B(10:6)=10x1x */) {
    return Vector2RegisterMiscellaneous_F32_VRSQRTE_instance_;
  }

  if ((inst.Bits() & 0x00030000) == 0x00030000 /* A(17:16)=11 */ &&
      (inst.Bits() & 0x00000600) == 0x00000600 /* B(10:6)=11xxx */) {
    return Vector2RegisterMiscellaneous_CVT_F2I_VCVT_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table simd_dp_2scalar.
 * Specified by: ('See Section A7.4.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2scalar(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00) == 0x00000000 /* A(11:8)=0000 */) {
    return VectorBinary2RegisterScalar_I16_32_VMLA_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */) {
    return VectorBinary2RegisterScalar_F32_VMLA_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000200 /* A(11:8)=0010 */) {
    return VectorBinary2RegisterScalar_I16_32L_VMLAL_by_scalar_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000300 /* A(11:8)=0011 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterScalar_I16_32L_VQDMLAL_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000400 /* A(11:8)=0100 */) {
    return VectorBinary2RegisterScalar_I16_32_VMLS_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000500 /* A(11:8)=0101 */) {
    return VectorBinary2RegisterScalar_F32_VMLS_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000600 /* A(11:8)=0110 */) {
    return VectorBinary2RegisterScalar_I16_32L_VMLSL_by_scalar_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000700 /* A(11:8)=0111 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterScalar_I16_32L_VQDMLSL_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000800 /* A(11:8)=1000 */) {
    return VectorBinary2RegisterScalar_I16_32_VMUL_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000900 /* A(11:8)=1001 */) {
    return VectorBinary2RegisterScalar_F32_VMUL_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8)=1010 */) {
    return VectorBinary2RegisterScalar_I16_32L_VMULL_by_scalar_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterScalar_I16_32L_VQDMULL_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000C00 /* A(11:8)=1100 */) {
    return VectorBinary2RegisterScalar_I16_32_VQDMULH_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */) {
    return VectorBinary2RegisterScalar_I16_32_VQRDMULH_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table simd_dp_2shift.
 * Specified by: ('See Section A7.4.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_2shift(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00) == 0x00000000 /* A(11:8)=0000 */) {
    return VectorBinary2RegisterShiftAmount_I_VSHR_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */) {
    return VectorBinary2RegisterShiftAmount_I_VSRA_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000200 /* A(11:8)=0010 */) {
    return VectorBinary2RegisterShiftAmount_I_VRSHR_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000300 /* A(11:8)=0011 */) {
    return VectorBinary2RegisterShiftAmount_I_VRSRA_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary2RegisterShiftAmount_I_VSRI_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterShiftAmount_I_VSHL_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary2RegisterShiftAmount_I_VSLI_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64R_VSHRN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* B(6)=1 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64R_VRSHRN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* B(6)=1 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRUN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8)=1010 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_E8_16_32L_VSHLL_A1_or_VMOVL_instance_;
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000600 /* A(11:8)=011x */) {
    return VectorBinary2RegisterShiftAmount_ILS_VQSHL_VQSHLU_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000E00 /* A(11:8)=111x */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_CVT_VCVT_between_floating_point_and_fixed_point_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table simd_dp_3diff.
 * Specified by: ('See Section A7.4.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_3diff(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00) == 0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_VADDHN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_VRADDHN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000500 /* A(11:8)=0101 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_VABAL_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000600 /* A(11:8)=0110 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_VSUBHN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000600 /* A(11:8)=0110 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_VRSUBHN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000700 /* A(11:8)=0111 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_VABDL_integer_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000C00 /* A(11:8)=1100 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_VMULL_integer_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32L_VQDMULL_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8)=1110 */) {
    return VectorBinary3RegisterDifferentLength_P8_VMULL_polynomial_A2_instance_;
  }

  if ((inst.Bits() & 0x00000D00) == 0x00000800 /* A(11:8)=10x0 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_VMLAL_VMLSL_integer_A2_instance_;
  }

  if ((inst.Bits() & 0x00000D00) == 0x00000900 /* A(11:8)=10x1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32L_VQDMLAL_VQDMLSL_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000000 /* A(11:8)=000x */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32_VADDL_VADDW_instance_;
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000200 /* A(11:8)=001x */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32_VSUBL_VSUBW_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table simd_dp_3same.
 * Specified by: ('See Section A7.4.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_simd_dp_3same(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00) == 0x00000000 /* A(11:8)=0000 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VHADD_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000000 /* A(11:8)=0000 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VQADD_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VRHADD_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00300000) == 0x00000000 /* C(21:20)=00 */) {
    return VectorBinary3RegisterSameLengthDQ_VAND_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00300000) == 0x00100000 /* C(21:20)=01 */) {
    return VectorBinary3RegisterSameLengthDQ_VBIC_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00300000) == 0x00200000 /* C(21:20)=10 */) {
    return VectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00300000) == 0x00300000 /* C(21:20)=11 */) {
    return VectorBinary3RegisterSameLengthDQ_VORN_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00300000) == 0x00000000 /* C(21:20)=00 */) {
    return VectorBinary3RegisterSameLengthDQ_VEOR_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00300000) == 0x00100000 /* C(21:20)=01 */) {
    return VectorBinary3RegisterSameLengthDQ_VBSL_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00300000) == 0x00200000 /* C(21:20)=10 */) {
    return VectorBinary3RegisterSameLengthDQ_VBIT_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00300000) == 0x00300000 /* C(21:20)=11 */) {
    return VectorBinary3RegisterSameLengthDQ_VBIF_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000200 /* A(11:8)=0010 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VHSUB_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000200 /* A(11:8)=0010 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VQSUB_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000300 /* A(11:8)=0011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000300 /* A(11:8)=0011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQ_VSHL_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VQSHL_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQ_VRSHL_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VQRSHL_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000600 /* A(11:8)=0110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMAX_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000600 /* A(11:8)=0110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMIN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000700 /* A(11:8)=0111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VABD_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000700 /* A(11:8)=0111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VABA_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQ_VADD_integer_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VSUB_integer_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VTST_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8)=1010 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx */) {
    return VectorBinary3RegisterSameLengthDI_VPMAX_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8)=1010 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx */) {
    return VectorBinary3RegisterSameLengthDI_VPMIN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx */) {
    return VectorBinary3RegisterSameLengthDI_VPADD_integer_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000C00 /* A(11:8)=1100 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */ &&
      (inst.Bits() & 0x00100000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx */) {
    return VectorBinary3RegisterSameLength32_DQ_VFMA_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000C00 /* A(11:8)=1100 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */ &&
      (inst.Bits() & 0x00100000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx */) {
    return VectorBinary3RegisterSameLength32_DQ_VFMS_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32P_VPADD_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VABD_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VCGE_register_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VCGT_register_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VACGE_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VACGT_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMAX_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMIN_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32P_VPMAX_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32P_VPMIN_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VRECPS_instance_;
  }

  if ((inst.Bits() & 0x00000F00) == 0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VRSQRTS_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table synchronization_primitives.
 * Specified by: ('See Section A5.2.10',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_synchronization_primitives(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00F00000) == 0x00800000 /* op(23:20)=1000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterOp_STREX_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00900000 /* op(23:20)=1001 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterOp_LDREX_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00A00000 /* op(23:20)=1010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterDoubleOp_STREXD_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00B00000 /* op(23:20)=1011 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterDoubleOp_LDREXD_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00C00000 /* op(23:20)=1100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterOp_STREXB_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00D00000 /* op(23:20)=1101 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterOp_LDREXB_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00E00000 /* op(23:20)=1110 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterOp_STREXH_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00F00000 /* op(23:20)=1111 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterOp_STREXH_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00000000 /* op(23:20)=0x00 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Deprecated_SWP_SWPB_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table transfer_between_arm_core_and_extension_register_8_16_and_32_bit.
 * Specified by: ('A7.8',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00000000 /* A(23:21)=000 */ &&
      (inst.Bits() & 0x0000006F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000 */) {
    return MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF) == 0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */) {
    return VfpUsesRegOp_Vmsr_Rule_336_A1_P660_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23:21)=0xx */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23:21)=1xx */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* B(6:5)=0x */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return DuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF) == 0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */) {
    return VfpMrsOp_Vmrs_Rule_335_A1_P658_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table transfer_between_arm_core_and_extension_registers_64_bit.
 * Specified by: ('A7.9',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_transfer_between_arm_core_and_extension_registers_64_bit(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x000000D0) == 0x00000010 /* op(7:4)=00x1 */) {
    return MoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1_instance_;
  }

  if ((inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x000000D0) == 0x00000010 /* op(7:4)=00x1 */) {
    return MoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1_instance_;
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table unconditional_instructions.
 * Specified by: ('See Section A5.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_unconditional_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x0FF00000) == 0x0C400000 /* op1(27:20)=11000100 */) {
    return ForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188_instance_;
  }

  if ((inst.Bits() & 0x0FF00000) == 0x0C500000 /* op1(27:20)=11000101 */) {
    return ForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204_instance_;
  }

  if ((inst.Bits() & 0x0E500000) == 0x08100000 /* op1(27:20)=100xx0x1 */ &&
      (inst.Bits() & 0x0000FFFF) == 0x00000A00 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000 */) {
    return ForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16_instance_;
  }

  if ((inst.Bits() & 0x0E500000) == 0x08400000 /* op1(27:20)=100xx1x0 */ &&
      (inst.Bits() & 0x000FFFE0) == 0x000D0500 /* $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx */) {
    return ForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20_instance_;
  }

  if ((inst.Bits() & 0x0F100000) == 0x0E000000 /* op1(27:20)=1110xxx0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */) {
    return ForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186_instance_;
  }

  if ((inst.Bits() & 0x0F100000) == 0x0E100000 /* op1(27:20)=1110xxx1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */) {
    return ForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202_instance_;
  }

  if ((inst.Bits() & 0x0E100000) == 0x0C000000 /* op1(27:20)=110xxxx0 */ &&
      (inst.Bits() & 0x0FB00000) != 0x0C000000 /* op1_repeated(27:20)=~11000x00 */) {
    return ForbiddenUncondDecoder_Stc2_Rule_188_A2_P372_instance_;
  }

  if ((inst.Bits() & 0x0E100000) == 0x0C100000 /* op1(27:20)=110xxxx1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0FB00000) != 0x0C100000 /* op1_repeated(27:20)=~11000x01 */) {
    return ForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106_instance_;
  }

  if ((inst.Bits() & 0x0E100000) == 0x0C100000 /* op1(27:20)=110xxxx1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x0FB00000) != 0x0C100000 /* op1_repeated(27:20)=~11000x01 */) {
    return ForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108_instance_;
  }

  if ((inst.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20)=1110xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */) {
    return ForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68_instance_;
  }

  if ((inst.Bits() & 0x0E000000) == 0x0A000000 /* op1(27:20)=101xxxxx */) {
    return ForbiddenUncondDecoder_Blx_Rule_23_A2_P58_instance_;
  }

  if ((inst.Bits() & 0x08000000) == 0x00000000 /* op1(27:20)=0xxxxxxx */) {
    return decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(inst);
  }

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Undefined_None_instance_;
  }

  // Catch any attempt to fall through...
  return not_implemented_;
}


const NamedClassDecoder& NamedArm32DecoderState::
decode_named(const nacl_arm_dec::Instruction inst) const {
  return decode_ARMv7(inst);
}

const nacl_arm_dec::ClassDecoder& NamedArm32DecoderState::
decode(const nacl_arm_dec::Instruction inst) const {
  return decode_named(inst).named_decoder();
}

}  // namespace nacl_arm_test
