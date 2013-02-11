/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
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

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table advanced_simd_data_processing_instructions.
 * Specified by: ('See Section A7.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_advanced_simd_data_processing_instructions(
     const nacl_arm_dec::Instruction inst) const {

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
    return VectorBinary3RegisterImmOp_Vext_Rule_305_A1_P598_instance_;
  }

  if ((inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00B00000)  ==
          0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* B(11:8)=1100 */ &&
      (inst.Bits() & 0x00000090)  ==
          0x00000000 /* C(7:4)=0xx0 */) {
    return VectorUnary2RegisterDup_Vdup_Rule_302_A1_P592_instance_;
  }

  if ((inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00B00000)  ==
          0x00B00000 /* A(23:19)=1x11x */ &&
      (inst.Bits() & 0x00000C00)  ==
          0x00000800 /* B(11:8)=10xx */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* C(7:4)=xxx0 */) {
    return VectorBinary3RegisterLookupOp_Vtbl_Vtbx_Rule_406_A1_P798_instance_;
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
  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000300 /* B(11:8)=0011 */) {
    return VectorLoadStoreMultiple2_VST2_multiple_2_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000200 /* B(11:8)=x010 */) {
    return VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000000 /* B(11:8)=000x */) {
    return VectorLoadStoreMultiple4_VST4_multiple_4_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000400 /* B(11:8)=010x */) {
    return VectorLoadStoreMultiple3_VST3_multiple_3_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000600 /* B(11:8)=011x */) {
    return VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000800 /* B(11:8)=100x */) {
    return VectorLoadStoreMultiple2_VST2_multiple_2_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000800 /* B(11:8)=1000 */) {
    return VectorLoadStoreSingle1_VST1_single_element_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000900 /* B(11:8)=1001 */) {
    return VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* B(11:8)=1010 */) {
    return VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* B(11:8)=1011 */) {
    return VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000000 /* B(11:8)=0x00 */) {
    return VectorLoadStoreSingle1_VST1_single_element_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000100 /* B(11:8)=0x01 */) {
    return VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000200 /* B(11:8)=0x10 */) {
    return VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00000000 /* L(21)=0 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000300 /* B(11:8)=0x11 */) {
    return VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000300 /* B(11:8)=0011 */) {
    return VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000200 /* B(11:8)=x010 */) {
    return VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000000 /* B(11:8)=000x */) {
    return VectorLoadStoreMultiple4_VLD4_multiple_4_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000400 /* B(11:8)=010x */) {
    return VectorLoadStoreMultiple3_VLD3_multiple_3_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000600 /* B(11:8)=011x */) {
    return VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23)=0 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000800 /* B(11:8)=100x */) {
    return VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000800 /* B(11:8)=1000 */) {
    return VectorLoadStoreSingle1_VLD1_single_element_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000900 /* B(11:8)=1001 */) {
    return VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* B(11:8)=1010 */) {
    return VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* B(11:8)=1011 */) {
    return VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* B(11:8)=1100 */) {
    return VectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* B(11:8)=1101 */) {
    return VectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* B(11:8)=1110 */) {
    return VectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* B(11:8)=1111 */) {
    return VectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000000 /* B(11:8)=0x00 */) {
    return VectorLoadStoreSingle1_VLD1_single_element_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000100 /* B(11:8)=0x01 */) {
    return VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000200 /* B(11:8)=0x10 */) {
    return VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane_instance_;
  }

  if ((inst.Bits() & 0x00200000)  ==
          0x00200000 /* L(21)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00800000 /* A(23)=1 */ &&
      (inst.Bits() & 0x00000B00)  ==
          0x00000300 /* B(11:8)=0x11 */) {
    return VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x03D00000)  ==
          0x00000000 /* op(25:20)=0000x0 */) {
    return StoreRegisterList_STMDA_STMED_instance_;
  }

  if ((inst.Bits() & 0x03D00000)  ==
          0x00100000 /* op(25:20)=0000x1 */) {
    return LoadRegisterList_LDMDA_LDMFA_instance_;
  }

  if ((inst.Bits() & 0x03D00000)  ==
          0x00800000 /* op(25:20)=0010x0 */) {
    return StoreRegisterList_STM_STMIA_STMEA_instance_;
  }

  if ((inst.Bits() & 0x03D00000)  ==
          0x00900000 /* op(25:20)=0010x1 */) {
    return LoadRegisterList_LDM_LDMIA_LDMFD_instance_;
  }

  if ((inst.Bits() & 0x03D00000)  ==
          0x01000000 /* op(25:20)=0100x0 */) {
    return StoreRegisterList_STMDB_STMFD_instance_;
  }

  if ((inst.Bits() & 0x03D00000)  ==
          0x01100000 /* op(25:20)=0100x1 */) {
    return LoadRegisterList_LDMDB_LDMEA_instance_;
  }

  if ((inst.Bits() & 0x03D00000)  ==
          0x01800000 /* op(25:20)=0110x0 */) {
    return StoreRegisterList_STMIB_STMFA_instance_;
  }

  if ((inst.Bits() & 0x03D00000)  ==
          0x01900000 /* op(25:20)=0110x1 */) {
    return LoadRegisterList_LDMIB_LDMED_instance_;
  }

  if ((inst.Bits() & 0x02500000)  ==
          0x00400000 /* op(25:20)=0xx1x0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_STM_User_registers_instance_;
  }

  if ((inst.Bits() & 0x02500000)  ==
          0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000)  ==
          0x00000000 /* R(15)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) {
    return ForbiddenCondDecoder_LDM_User_registers_instance_;
  }

  if ((inst.Bits() & 0x02500000)  ==
          0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000)  ==
          0x00008000 /* R(15)=1 */) {
    return ForbiddenCondDecoder_LDM_exception_return_instance_;
  }

  if ((inst.Bits() & 0x03000000)  ==
          0x02000000 /* op(25:20)=10xxxx */) {
    return BranchImmediate24_B_instance_;
  }

  if ((inst.Bits() & 0x03000000)  ==
          0x03000000 /* op(25:20)=11xxxx */) {
    return BranchImmediate24_BL_BLX_immediate_instance_;
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

  if ((inst.Bits() & 0x03E00000)  ==
          0x00000000 /* op1(25:20)=00000x */) {
    return Undefined_None_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03F00000)  ==
          0x00400000 /* op1(25:20)=000100 */) {
    return Forbidden_MCRR_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03F00000)  ==
          0x00500000 /* op1(25:20)=000101 */) {
    return Forbidden_MRRC_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03100000)  ==
          0x02000000 /* op1(25:20)=10xxx0 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* op(4)=1 */) {
    return Forbidden_MCR_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03100000)  ==
          0x02100000 /* op1(25:20)=10xxx1 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* op(4)=1 */) {
    return Forbidden_MRC_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000)  ==
          0x00000000 /* op1(25:20)=0xxxx0 */ &&
      (inst.Bits() & 0x03B00000)  !=
          0x00000000 /* op1_repeated(25:20)=~000x00 */) {
    return Forbidden_STC_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000)  ==
          0x00100000 /* op1(25:20)=0xxxx1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x03B00000)  !=
          0x00100000 /* op1_repeated(25:20)=~000x01 */) {
    return Forbidden_LDC_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000)  ==
          0x00100000 /* op1(25:20)=0xxxx1 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x03B00000)  !=
          0x00100000 /* op1_repeated(25:20)=~000x01 */) {
    return Forbidden_LDC_literal_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  !=
          0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03000000)  ==
          0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op(4)=0 */) {
    return Forbidden_CDP_instance_;
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
    return Forbidden_SVC_instance_;
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
    return ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000D0)  ==
          0x000000D0 /* op2(7:4)=11x1 */) {
    return ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_;
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
      (inst.Bits() & 0x01F00000)  ==
          0x01000000 /* op1(24:20)=10000 */) {
    return MOVW_cccc00110000iiiiddddiiiiiiiiiiii_case_0_MOVW_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01F00000)  ==
          0x01400000 /* op1(24:20)=10100 */) {
    return MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_0_MOVT_instance_;
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
  if ((inst.Bits() & 0x01F00000)  ==
          0x01100000 /* op(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return MaskedBinaryRegisterImmediateTest_TST_immediate_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01300000 /* op(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return BinaryRegisterImmediateTest_TEQ_immediate_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01500000 /* op(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return BinaryRegisterImmediateTest_CMP_immediate_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01700000 /* op(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return BinaryRegisterImmediateTest_CMN_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00000000 /* op(24:20)=0000x */) {
    return Binary2RegisterImmediateOp_AND_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00200000 /* op(24:20)=0001x */) {
    return Binary2RegisterImmediateOp_EOR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00400000 /* op(24:20)=0010x */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Binary2RegisterImmediateOpAddSub_SUB_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00400000 /* op(24:20)=0010x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */) {
    return Unary1RegisterImmediateOpPc_ADR_A2_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00600000 /* op(24:20)=0011x */) {
    return Binary2RegisterImmediateOp_RSB_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00800000 /* op(24:20)=0100x */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Binary2RegisterImmediateOpAddSub_ADD_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00800000 /* op(24:20)=0100x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */) {
    return Unary1RegisterImmediateOpPc_ADR_A1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00A00000 /* op(24:20)=0101x */) {
    return Binary2RegisterImmediateOp_ADC_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00C00000 /* op(24:20)=0110x */) {
    return Binary2RegisterImmediateOp_SBC_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00E00000 /* op(24:20)=0111x */) {
    return Binary2RegisterImmediateOp_RSC_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01800000 /* op(24:20)=1100x */) {
    return Binary2RegisterImmediateOpDynCodeReplace_ORR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op(24:20)=1101x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01C00000 /* op(24:20)=1110x */) {
    return MaskedBinary2RegisterImmediateOp_BIC_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01E00000 /* op(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
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
  if ((inst.Bits() & 0x01F00000)  ==
          0x01100000 /* op1(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary2RegisterImmedShiftedTest_TST_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01300000 /* op1(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary2RegisterImmedShiftedTest_TEQ_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01500000 /* op1(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary2RegisterImmedShiftedTest_CMP_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01700000 /* op1(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary2RegisterImmedShiftedTest_CMN_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00000000 /* op1(24:20)=0000x */) {
    return Binary3RegisterShiftedOp_AND_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00200000 /* op1(24:20)=0001x */) {
    return Binary3RegisterShiftedOp_EOR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00400000 /* op1(24:20)=0010x */) {
    return Binary3RegisterShiftedOp_SUB_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00600000 /* op1(24:20)=0011x */) {
    return Binary3RegisterShiftedOp_RSB_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00800000 /* op1(24:20)=0100x */) {
    return Binary3RegisterShiftedOp_ADD_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00A00000 /* op1(24:20)=0101x */) {
    return Binary3RegisterShiftedOp_ADC_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00C00000 /* op1(24:20)=0110x */) {
    return Binary3RegisterShiftedOp_SBC_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00E00000 /* op1(24:20)=0111x */) {
    return Binary3RegisterShiftedOp_RSC_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01800000 /* op1(24:20)=1100x */) {
    return Binary3RegisterShiftedOp_ORR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80)  !=
          0x00000000 /* op2(11:7)=~00000 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op3(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOpImmNotZero_LSL_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80)  !=
          0x00000000 /* op2(11:7)=~00000 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000060 /* op3(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOpImmNotZero_ROR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80)  ==
          0x00000000 /* op2(11:7)=00000 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op3(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterOp_MOV_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80)  ==
          0x00000000 /* op2(11:7)=00000 */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000060 /* op3(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterOp_RRX_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000020 /* op3(6:5)=01 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOp_LSR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000040 /* op3(6:5)=10 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Unary2RegisterShiftedOp_ASR_immediate_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01C00000 /* op1(24:20)=1110x */) {
    return Binary3RegisterShiftedOp_BIC_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
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
  if ((inst.Bits() & 0x01F00000)  ==
          0x01100000 /* op1(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterShiftedTest_TST_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01300000 /* op1(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterShiftedTest_TEQ_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01500000 /* op1(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterShiftedTest_CMP_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01700000 /* op1(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterShiftedTest_CMN_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00000000 /* op1(24:20)=0000x */) {
    return Binary4RegisterShiftedOp_AND_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00200000 /* op1(24:20)=0001x */) {
    return Binary4RegisterShiftedOp_EOR_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00400000 /* op1(24:20)=0010x */) {
    return Binary4RegisterShiftedOp_SUB_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00600000 /* op1(24:20)=0011x */) {
    return Binary4RegisterShiftedOp_RSB_register_shfited_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00800000 /* op1(24:20)=0100x */) {
    return Binary4RegisterShiftedOp_ADD_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00A00000 /* op1(24:20)=0101x */) {
    return Binary4RegisterShiftedOp_ADC_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00C00000 /* op1(24:20)=0110x */) {
    return Binary4RegisterShiftedOp_SBC_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00E00000 /* op1(24:20)=0111x */) {
    return Binary4RegisterShiftedOp_RSC_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01800000 /* op1(24:20)=1100x */) {
    return Binary4RegisterShiftedOp_ORR_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op2(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Binary3RegisterOp_LSL_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Binary3RegisterOp_LSR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Binary3RegisterOp_ASR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Binary3RegisterOp_ROR_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01C00000 /* op1(24:20)=1110x */) {
    return Binary4RegisterShiftedOp_BIC_register_shifted_register_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
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

  if ((inst.Bits() & 0x01B00000)  ==
          0x00900000 /* opcode(24:20)=01x01 */) {
    return LoadVectorRegisterList_VLDM_instance_;
  }

  if ((inst.Bits() & 0x01B00000)  ==
          0x00B00000 /* opcode(24:20)=01x11 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000D0000 /* Rn(19:16)=~1101 */) {
    return LoadVectorRegisterList_VLDM_instance_;
  }

  if ((inst.Bits() & 0x01B00000)  ==
          0x00B00000 /* opcode(24:20)=01x11 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000D0000 /* Rn(19:16)=1101 */) {
    return LoadVectorRegisterList_VPOP_instance_;
  }

  if ((inst.Bits() & 0x01B00000)  ==
          0x01200000 /* opcode(24:20)=10x10 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000D0000 /* Rn(19:16)=~1101 */) {
    return StoreVectorRegisterList_VSTM_instance_;
  }

  if ((inst.Bits() & 0x01B00000)  ==
          0x01200000 /* opcode(24:20)=10x10 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000D0000 /* Rn(19:16)=1101 */) {
    return StoreVectorRegisterList_VPUSH_instance_;
  }

  if ((inst.Bits() & 0x01B00000)  ==
          0x01300000 /* opcode(24:20)=10x11 */) {
    return LoadVectorRegisterList_VLDM_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x00400000 /* opcode(24:20)=0010x */) {
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);
  }

  if ((inst.Bits() & 0x01300000)  ==
          0x01000000 /* opcode(24:20)=1xx00 */) {
    return StoreVectorRegister_VSTR_instance_;
  }

  if ((inst.Bits() & 0x01300000)  ==
          0x01100000 /* opcode(24:20)=1xx01 */) {
    return LoadVectorRegister_VLDR_instance_;
  }

  if ((inst.Bits() & 0x01900000)  ==
          0x00800000 /* opcode(24:20)=01xx0 */) {
    return StoreVectorRegisterList_VSTM_instance_;
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
  if ((inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Store3RegisterOp_STRH_register_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterOp_LDRH_register_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */) {
    return Store2RegisterImm8Op_STRH_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8Op_LDRH_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */) {
    return LoadRegisterImm8Op_LDRH_literal_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterDoubleOp_LDRD_register_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterOp_LDRSB_register_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8DoubleOp_LDRD_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return LoadRegisterImm8DoubleOp_LDRD_literal_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8Op_LDRSB_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Store3RegisterDoubleOp_STRD_register_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Load3RegisterOp_LDRSH_register_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */) {
    return Store2RegisterImm8DoubleOp_STRD_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000060)  ==
          0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */) {
    return Load2RegisterImm8Op_LDRSH_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000040)  ==
          0x00000040 /* op2(6:5)=1x */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000)  ==
          0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
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

  if ((inst.Bits() & 0x00B00000)  ==
          0x00000000 /* opc1(23:20)=0x00 */) {
    return CondVfpOp_VMLA_VMLS_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00100000 /* opc1(23:20)=0x01 */) {
    return CondVfpOp_VNMLA_VNMLS_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00200000 /* opc1(23:20)=0x10 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* opc3(7:6)=x0 */) {
    return CondVfpOp_VMUL_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00200000 /* opc1(23:20)=0x10 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_VNMUL_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00300000 /* opc1(23:20)=0x11 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* opc3(7:6)=x0 */) {
    return CondVfpOp_VADD_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00300000 /* opc1(23:20)=0x11 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_VSUB_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00800000 /* opc1(23:20)=1x00 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* opc3(7:6)=x0 */) {
    return CondVfpOp_VDIV_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00900000 /* opc1(23:20)=1x01 */) {
    return CondVfpOp_VFNMA_VFNMS_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00A00000 /* opc1(23:20)=1x10 */) {
    return CondVfpOp_VFMA_VFMS_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00B00000 /* opc1(23:20)=1x11 */) {
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
  if ((inst.Bits() & 0x00600000)  ==
          0x00000000 /* op1(22:21)=00 */) {
    return Binary4RegisterDualOp_SMLABB_SMLABT_SMLATB_SMLATT_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */) {
    return Binary4RegisterDualOp_SMLAWB_SMLAWT_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltA_SMULWB_SMULWT_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00400000 /* op1(22:21)=10 */) {
    return Binary4RegisterDualResult_SMLALBB_SMLALBT_SMLALTB_SMLALTT_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00600000 /* op1(22:21)=11 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
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
  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000)  ==
          0x00200000 /* op1(24:20)=0x010 */) {
    return ForbiddenCondDecoder_STRT_A1_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000)  ==
          0x00300000 /* op1(24:20)=0x011 */) {
    return ForbiddenCondDecoder_LDRT_A1_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000)  ==
          0x00600000 /* op1(24:20)=0x110 */) {
    return ForbiddenCondDecoder_STRBT_A1_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000)  ==
          0x00700000 /* op1(24:20)=0x111 */) {
    return ForbiddenCondDecoder_LDRBT_A1_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00200000 /* op1_repeated(24:20)=~0x010 */) {
    return Store2RegisterImm12Op_STR_immediate_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00300000 /* op1_repeated(24:20)=~0x011 */) {
    return LdrImmediateOp_LDR_immediate_instance_;
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
    return Load2RegisterImm12Op_LDR_literal_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00600000 /* op1_repeated(24:20)=~0x110 */) {
    return Store2RegisterImm12Op_STRB_immediate_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00700000 /* op1_repeated(24:20)=~0x111 */) {
    return Load2RegisterImm12Op_LDRB_immediate_instance_;
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
    return Load2RegisterImm12Op_LDRB_literal_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000)  ==
          0x00200000 /* op1(24:20)=0x010 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_STRT_A2_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000)  ==
          0x00300000 /* op1(24:20)=0x011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_LDRT_A2_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000)  ==
          0x00600000 /* op1(24:20)=0x110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_STRBT_A2_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000)  ==
          0x00700000 /* op1(24:20)=0x111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return ForbiddenCondDecoder_LDRBT_A2_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00200000 /* op1_repeated(24:20)=~0x010 */) {
    return Store3RegisterImm5Op_STR_register_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00300000 /* op1_repeated(24:20)=~0x011 */) {
    return Load3RegisterImm5Op_LDR_register_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00600000 /* op1_repeated(24:20)=~0x110 */) {
    return Store3RegisterImm5Op_STRB_register_instance_;
  }

  if ((inst.Bits() & 0x02000000)  ==
          0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000)  ==
          0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000)  !=
          0x00700000 /* op1_repeated(24:20)=~0x111 */) {
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

  if ((inst.Bits() & 0x01F00000)  ==
          0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000)  !=
          0x0000F000 /* Rd(15:12)=~1111 */) {
    return Binary4RegisterDualOp_USADA8_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* Rd(15:12)=1111 */) {
    return Binary3RegisterOpAltA_USAD8_instance_;
  }

  if ((inst.Bits() & 0x01F00000)  ==
          0x01F00000 /* op1(24:20)=11111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */) {
    return PermanentlyUndefined_UDF_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(7:5)=x10 */) {
    return Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_SBFX_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F)  !=
          0x0000000F /* Rn(3:0)=~1111 */) {
    return Binary2RegisterBitRangeMsbGeLsb_BFI_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F)  ==
          0x0000000F /* Rn(3:0)=1111 */) {
    return Unary1RegisterBitRangeMsbGeLsb_BFC_instance_;
  }

  if ((inst.Bits() & 0x01E00000)  ==
          0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x00000060)  ==
          0x00000040 /* op2(7:5)=x10 */) {
    return Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_UBFX_instance_;
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

  if ((inst.Bits() & 0x07F00000)  ==
          0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000000 /* op2(7:4)=0000 */ &&
      (inst.Bits() & 0x00010000)  ==
          0x00010000 /* Rn(19:16)=xxx1 */ &&
      (inst.Bits() & 0x000EFD0F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000 */) {
    return Forbidden_SETEND_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op2(7:4)=xx0x */ &&
      (inst.Bits() & 0x00010000)  ==
          0x00000000 /* Rn(19:16)=xxx0 */ &&
      (inst.Bits() & 0x0000FE00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx */) {
    return Forbidden_CPS_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05300000 /* op1(26:20)=1010011 */) {
    return Unpredictable_None_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000000 /* op2(7:4)=0000 */) {
    return Unpredictable_None_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000010 /* op2(7:4)=0001 */ &&
      (inst.Bits() & 0x000FFF0F)  ==
          0x000FF00F /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111 */) {
    return Forbidden_CLREX_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000040 /* op2(7:4)=0100 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return DataBarrier_DSB_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000050 /* op2(7:4)=0101 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return DataBarrier_DMB_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000060 /* op2(7:4)=0110 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return InstructionBarrier_ISB_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x00000070 /* op2(7:4)=0111 */) {
    return Unpredictable_None_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:4)=001x */) {
    return Unpredictable_None_instance_;
  }

  if ((inst.Bits() & 0x07F00000)  ==
          0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000080 /* op2(7:4)=1xxx */) {
    return Unpredictable_None_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x04100000 /* op1(26:20)=100x001 */) {
    return Forbidden_None_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x04500000 /* op1(26:20)=100x101 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_PLI_immediate_literal_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x05100000 /* op1(26:20)=101x001 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */) {
    return Unpredictable_None_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x05500000 /* op1(26:20)=101x101 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_PLD_literal_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x06100000 /* op1(26:20)=110x001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */) {
    return Forbidden_None_instance_;
  }

  if ((inst.Bits() & 0x07700000)  ==
          0x06500000 /* op1(26:20)=110x101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOp_PLI_register_instance_;
  }

  if ((inst.Bits() & 0x07B00000)  ==
          0x05B00000 /* op1(26:20)=1011x11 */) {
    return Unpredictable_None_instance_;
  }

  if ((inst.Bits() & 0x07300000)  ==
          0x04300000 /* op1(26:20)=100xx11 */) {
    return Unpredictable_None_instance_;
  }

  if ((inst.Bits() & 0x07300000)  ==
          0x05100000 /* op1(26:20)=101xx01 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterImm12Op_PLD_PLDW_immediate_instance_;
  }

  if ((inst.Bits() & 0x07300000)  ==
          0x07100000 /* op1(26:20)=111xx01 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOp_PLD_PLDW_register_instance_;
  }

  if ((inst.Bits() & 0x06300000)  ==
          0x06300000 /* op1(26:20)=11xxx11 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op2(7:4)=xxx0 */) {
    return Unpredictable_None_instance_;
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
    return Unary1RegisterUse_MSR_register_instance_;
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
    return ForbiddenCondDecoder_MSR_register_instance_;
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
    return ForbiddenCondDecoder_MSR_register_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x0000FD00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return ForbiddenCondDecoder_MSR_register_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x000F0D0F)  ==
          0x000F0000 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000 */) {
    return Unary1RegisterSet_MRS_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x00000C0F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000 */) {
    return ForbiddenCondDecoder_MRS_Banked_register_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200)  ==
          0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* op(22:21)=x1 */ &&
      (inst.Bits() & 0x0000FC00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx */) {
    return ForbiddenCondDecoder_MSR_Banked_register_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000010 /* op2(6:4)=001 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return BranchToRegister_Bx_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000010 /* op2(6:4)=001 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000F0F00)  ==
          0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterOpNotRmIsPc_CLZ_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000020 /* op2(6:4)=010 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return ForbiddenCondDecoder_BXJ_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000030 /* op2(6:4)=011 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return BranchToRegister_BLX_register_instance_;
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
    return ForbiddenCondDecoder_ERET_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */) {
    return BreakPointAndConstantPoolHead_BKPT_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00400000 /* op(22:21)=10 */) {
    return ForbiddenCondDecoder_HVC_instance_;
  }

  if ((inst.Bits() & 0x00000070)  ==
          0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000)  ==
          0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx */) {
    return ForbiddenCondDecoder_SMC_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF)  ==
          0x00000000 /* op2(7:0)=00000000 */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return CondDecoder_NOP_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF)  ==
          0x00000001 /* op2(7:0)=00000001 */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return CondDecoder_YIELD_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF)  ==
          0x00000002 /* op2(7:0)=00000010 */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_WFE_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF)  ==
          0x00000003 /* op2(7:0)=00000011 */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_WFI_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF)  ==
          0x00000004 /* op2(7:0)=00000100 */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_SEV_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000F0)  ==
          0x000000F0 /* op2(7:0)=1111xxxx */ &&
      (inst.Bits() & 0x0000FF00)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_DBG_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x00040000 /* op1(19:16)=0100 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return MoveImmediate12ToApsr_MSR_immediate_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000B0000)  ==
          0x00080000 /* op1(19:16)=1x00 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return MoveImmediate12ToApsr_MSR_immediate_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00030000)  ==
          0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_MSR_immediate_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00020000)  ==
          0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_MSR_immediate_instance_;
  }

  if ((inst.Bits() & 0x00400000)  ==
          0x00400000 /* op(22)=1 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_MSR_immediate_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00F00000)  ==
          0x00400000 /* op(23:20)=0100 */) {
    return Binary4RegisterDualResult_UMAAL_A1_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00600000 /* op(23:20)=0110 */) {
    return Binary4RegisterDualOp_MLS_A1_instance_;
  }

  if ((inst.Bits() & 0x00D00000)  ==
          0x00500000 /* op(23:20)=01x1 */) {
    return Undefined_None_instance_;
  }

  if ((inst.Bits() & 0x00E00000)  ==
          0x00000000 /* op(23:20)=000x */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltA_MUL_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000)  ==
          0x00200000 /* op(23:20)=001x */) {
    return Binary4RegisterDualOpLtV6RdNotRn_MLA_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000)  ==
          0x00800000 /* op(23:20)=100x */) {
    return Binary4RegisterDualResultUsesRnRm_UMULL_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000)  ==
          0x00A00000 /* op(23:20)=101x */) {
    return Binary4RegisterDualResultLtV6RdHiLoNotRn_UMLAL_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000)  ==
          0x00C00000 /* op(23:20)=110x */) {
    return Binary4RegisterDualResultUsesRnRm_SMULL_A1_instance_;
  }

  if ((inst.Bits() & 0x00E00000)  ==
          0x00E00000 /* op(23:20)=111x */) {
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
  if ((inst.Bits() & 0x000F0000)  ==
          0x00000000 /* opc2(19:16)=0000 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000040 /* opc3(7:6)=01 */) {
    return CondVfpOp_VMOV_register_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00000000 /* opc2(19:16)=0000 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x000000C0 /* opc3(7:6)=11 */) {
    return CondVfpOp_VABS_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00010000 /* opc2(19:16)=0001 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000040 /* opc3(7:6)=01 */) {
    return CondVfpOp_VNEG_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00010000 /* opc2(19:16)=0001 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x000000C0 /* opc3(7:6)=11 */) {
    return CondVfpOp_VSQRT_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00040000 /* opc2(19:16)=0100 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_VCMP_VCMPE_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00050000 /* opc2(19:16)=0101 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x0000002F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000 */) {
    return CondVfpOp_VCMP_VCMPE_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00070000 /* opc2(19:16)=0111 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x000000C0 /* opc3(7:6)=11 */) {
    return CondVfpOp_VCVT_between_double_precision_and_single_precision_instance_;
  }

  if ((inst.Bits() & 0x000F0000)  ==
          0x00080000 /* opc2(19:16)=1000 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_instance_;
  }

  if ((inst.Bits() & 0x000E0000)  ==
          0x00020000 /* opc2(19:16)=001x */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx */) {
    return CondVfpOp_VCVTB_VCVTT_instance_;
  }

  if ((inst.Bits() & 0x000E0000)  ==
          0x000C0000 /* opc2(19:16)=110x */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return CondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_instance_;
  }

  if ((inst.Bits() & 0x000A0000)  ==
          0x000A0000 /* opc2(19:16)=1x1x */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* opc3(7:6)=x1 */) {
    return VcvtPtAndFixedPoint_FloatingPoint_VCVT_between_floating_point_and_fixed_point_Floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000040)  ==
          0x00000000 /* opc3(7:6)=x0 */ &&
      (inst.Bits() & 0x000000A0)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx */) {
    return CondVfpOp_VMOV_immediate_instance_;
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
  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SXTAB16_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_SXTB16_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SEL_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op2(7:5)=xx0 */) {
    return Binary3RegisterOpAltBNoCondUpdates_PKH_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Unary2RegisterSatImmedShiftedOp_SSAT16_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SXTAB_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_SXTB_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x000F0F00)  ==
          0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_REV_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SXTAH_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_SXTH_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x000F0F00)  ==
          0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_REV16_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UXTAB16_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_UXTB16_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Unary2RegisterSatImmedShiftedOp_USAT16_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UXTAB_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_UXTB_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x000F0F00)  ==
          0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_RBIT_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UXTAH_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_UXTH_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x000F0F00)  ==
          0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Unary2RegisterImmedShiftedOp_REVSH_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00200000 /* op1(22:20)=01x */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op2(7:5)=xx0 */) {
    return Unary2RegisterSatImmedShiftedOp_SSAT_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00600000 /* op1(22:20)=11x */ &&
      (inst.Bits() & 0x00000020)  ==
          0x00000000 /* op2(7:5)=xx0 */) {
    return Unary2RegisterSatImmedShiftedOp_USAT_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SADD16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SASX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SSAX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SSSUB16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SADD8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SSUB8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QADD16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QASX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QSAX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QSUB16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QADD8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QSUB8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SHADD16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SHASX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SHSAX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SHSUB16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SHADD8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_SHSUB8_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UADD16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UASX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_USAX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_USUB16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UADD8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_USUB8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UQADD16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UQASX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UQSAX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UQSUB16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UQADD8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UQSUB8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UHADD16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UHASX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UHSAX_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UHSUB16_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UHADD8_instance_;
  }

  if ((inst.Bits() & 0x00300000)  ==
          0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_UHSUB8_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00600000)  ==
          0x00000000 /* op(22:21)=00 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QADD_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QSUB_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00400000 /* op(22:21)=10 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Binary3RegisterOpAltBNoCondUpdates_QDADD_instance_;
  }

  if ((inst.Bits() & 0x00600000)  ==
          0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
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
  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000)  !=
          0x0000F000 /* A(15:12)=~1111 */) {
    return Binary4RegisterDualOpNoCondsUpdate_SMLAD_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* A(15:12)=1111 */) {
    return Binary3RegisterOpAltANoCondsUpdate_SMUAD_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000040 /* op2(7:5)=01x */ &&
      (inst.Bits() & 0x0000F000)  !=
          0x0000F000 /* A(15:12)=~1111 */) {
    return Binary4RegisterDualOpNoCondsUpdate_SMLSD_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000040 /* op2(7:5)=01x */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* A(15:12)=1111 */) {
    return Binary3RegisterOpAltANoCondsUpdate_SMUSD_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00100000 /* op1(22:20)=001 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltANoCondsUpdate_SDIV_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0)  ==
          0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Binary3RegisterOpAltANoCondsUpdate_UDIV_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000000 /* op2(7:5)=00x */) {
    return Binary4RegisterDualResultNoCondsUpdate_SMLALD_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000040 /* op2(7:5)=01x */) {
    return Binary4RegisterDualResultNoCondsUpdate_SMLSLD_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000)  !=
          0x0000F000 /* A(15:12)=~1111 */) {
    return Binary4RegisterDualOpNoCondsUpdate_SMMLA_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000)  ==
          0x0000F000 /* A(15:12)=1111 */) {
    return Binary3RegisterOpAltANoCondsUpdate_SMMUL_instance_;
  }

  if ((inst.Bits() & 0x00700000)  ==
          0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0)  ==
          0x000000C0 /* op2(7:5)=11x */) {
    return Binary4RegisterDualOpNoCondsUpdate_SMMLS_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000D00)  ==
          0x00000800 /* cmode(11:8)=10x0 */) {
    return Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000D00)  ==
          0x00000900 /* cmode(11:8)=10x1 */) {
    return Vector1RegisterImmediate_BIT_VORR_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000900)  ==
          0x00000000 /* cmode(11:8)=0xx0 */) {
    return Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000900)  ==
          0x00000100 /* cmode(11:8)=0xx1 */) {
    return Vector1RegisterImmediate_BIT_VORR_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000000 /* op(5)=0 */ &&
      (inst.Bits() & 0x00000C00)  ==
          0x00000C00 /* cmode(11:8)=11xx */) {
    return Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* cmode(11:8)=1110 */) {
    return Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* cmode(11:8)=1111 */) {
    return Undefined_None_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000D00)  ==
          0x00000800 /* cmode(11:8)=10x0 */) {
    return Vector1RegisterImmediate_MVN_VMVN_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000D00)  ==
          0x00000900 /* cmode(11:8)=10x1 */) {
    return Vector1RegisterImmediate_BIT_VBIC_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000E00)  ==
          0x00000C00 /* cmode(11:8)=110x */) {
    return Vector1RegisterImmediate_MVN_VMVN_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000900)  ==
          0x00000000 /* cmode(11:8)=0xx0 */) {
    return Vector1RegisterImmediate_MVN_VMVN_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000020)  ==
          0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x00000900)  ==
          0x00000100 /* cmode(11:8)=0xx1 */) {
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
  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000000 /* B(10:6)=0000x */) {
    return Vector2RegisterMiscellaneous_RG_VREV64_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000080 /* B(10:6)=0001x */) {
    return Vector2RegisterMiscellaneous_RG_VREV32_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000100 /* B(10:6)=0010x */) {
    return Vector2RegisterMiscellaneous_RG_VREV16_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000400 /* B(10:6)=1000x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCLS_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000480 /* B(10:6)=1001x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCLZ_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000500 /* B(10:6)=1010x */) {
    return Vector2RegisterMiscellaneous_V8_VCNT_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000580 /* B(10:6)=1011x */) {
    return Vector2RegisterMiscellaneous_V8_VMVN_register_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000700 /* B(10:6)=1110x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VQABS_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000780 /* B(10:6)=1111x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VQNEG_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000200 /* B(10:6)=010xx */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VPADDL_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00000000 /* A(17:16)=00 */ &&
      (inst.Bits() & 0x00000700)  ==
          0x00000600 /* B(10:6)=110xx */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VPADAL_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000000 /* B(10:6)=0000x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000080 /* B(10:6)=0001x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000100 /* B(10:6)=0010x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000180 /* B(10:6)=0011x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000200 /* B(10:6)=0100x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000300 /* B(10:6)=0110x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VABS_A1_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000380 /* B(10:6)=0111x */) {
    return Vector2RegisterMiscellaneous_V8_16_32_VNEG_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000400 /* B(10:6)=1000x */) {
    return Vector2RegisterMiscellaneous_F32_VCGT_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000480 /* B(10:6)=1001x */) {
    return Vector2RegisterMiscellaneous_F32_VCGE_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000500 /* B(10:6)=1010x */) {
    return Vector2RegisterMiscellaneous_F32_VCEQ_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000580 /* B(10:6)=1011x */) {
    return Vector2RegisterMiscellaneous_F32_VCLE_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000600 /* B(10:6)=1100x */) {
    return Vector2RegisterMiscellaneous_F32_VCLT_immediate_0_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000700 /* B(10:6)=1110x */) {
    return Vector2RegisterMiscellaneous_F32_VABS_A1_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00010000 /* A(17:16)=01 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000780 /* B(10:6)=1111x */) {
    return Vector2RegisterMiscellaneous_F32_VNEG_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0)  ==
          0x00000200 /* B(10:6)=01000 */) {
    return Vector2RegisterMiscellaneous_V16_32_64N_VMOVN_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0)  ==
          0x00000240 /* B(10:6)=01001 */) {
    return Vector2RegisterMiscellaneous_I16_32_64N_VQMOVUN_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000007C0)  ==
          0x00000300 /* B(10:6)=01100 */) {
    return Vector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x000006C0)  ==
          0x00000600 /* B(10:6)=11x00 */) {
    return Vector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000000 /* B(10:6)=0000x */) {
    return Vector2RegisterMiscellaneous_V8S_VSWP_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000080 /* B(10:6)=0001x */) {
    return Vector2RegisterMiscellaneous_V8_16_32T_VTRN_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000100 /* B(10:6)=0010x */) {
    return Vector2RegisterMiscellaneous_V8_16_32I_VUZP_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000180 /* B(10:6)=0011x */) {
    return Vector2RegisterMiscellaneous_V8_16_32I_VZIP_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00020000 /* A(17:16)=10 */ &&
      (inst.Bits() & 0x00000780)  ==
          0x00000280 /* B(10:6)=0101x */) {
    return Vector2RegisterMiscellaneous_I16_32_64N_VQMOVN_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00030000 /* A(17:16)=11 */ &&
      (inst.Bits() & 0x00000680)  ==
          0x00000400 /* B(10:6)=10x0x */) {
    return Vector2RegisterMiscellaneous_F32_VRECPE_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00030000 /* A(17:16)=11 */ &&
      (inst.Bits() & 0x00000680)  ==
          0x00000480 /* B(10:6)=10x1x */) {
    return Vector2RegisterMiscellaneous_F32_VRSQRTE_instance_;
  }

  if ((inst.Bits() & 0x00030000)  ==
          0x00030000 /* A(17:16)=11 */ &&
      (inst.Bits() & 0x00000600)  ==
          0x00000600 /* B(10:6)=11xxx */) {
    return Vector2RegisterMiscellaneous_CVT_F2I_VCVT_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000000 /* A(11:8)=0000 */) {
    return VectorBinary2RegisterScalar_I16_32_VMLA_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */) {
    return VectorBinary2RegisterScalar_F32_VMLA_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000200 /* A(11:8)=0010 */) {
    return VectorBinary2RegisterScalar_I16_32L_VMLAL_by_scalar_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000300 /* A(11:8)=0011 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterScalar_I16_32L_VQDMLAL_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000400 /* A(11:8)=0100 */) {
    return VectorBinary2RegisterScalar_I16_32_VMLS_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000500 /* A(11:8)=0101 */) {
    return VectorBinary2RegisterScalar_F32_VMLS_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000600 /* A(11:8)=0110 */) {
    return VectorBinary2RegisterScalar_I16_32L_VMLSL_by_scalar_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000700 /* A(11:8)=0111 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterScalar_I16_32L_VQDMLSL_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */) {
    return VectorBinary2RegisterScalar_I16_32_VMUL_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */) {
    return VectorBinary2RegisterScalar_F32_VMUL_by_scalar_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* A(11:8)=1010 */) {
    return VectorBinary2RegisterScalar_I16_32L_VMULL_by_scalar_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterScalar_I16_32L_VQDMULL_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* A(11:8)=1100 */) {
    return VectorBinary2RegisterScalar_I16_32_VQDMULH_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */) {
    return VectorBinary2RegisterScalar_I16_32_VQRDMULH_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000000 /* A(11:8)=0000 */) {
    return VectorBinary2RegisterShiftAmount_I_VSHR_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */) {
    return VectorBinary2RegisterShiftAmount_I_VSRA_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000200 /* A(11:8)=0010 */) {
    return VectorBinary2RegisterShiftAmount_I_VRSHR_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000300 /* A(11:8)=0011 */) {
    return VectorBinary2RegisterShiftAmount_I_VRSRA_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary2RegisterShiftAmount_I_VSRI_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary2RegisterShiftAmount_I_VSHL_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary2RegisterShiftAmount_I_VSLI_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64R_VSHRN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* B(6)=1 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64R_VRSHRN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000040 /* B(6)=1 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRUN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* A(11:8)=1010 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* B(6)=0 */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_E8_16_32L_VSHLL_A1_or_VMOVL_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000600 /* A(11:8)=011x */) {
    return VectorBinary2RegisterShiftAmount_ILS_VQSHL_VQSHLU_immediate_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000E00 /* A(11:8)=111x */ &&
      (inst.Bits() & 0x00000080)  ==
          0x00000000 /* L(7)=0 */) {
    return VectorBinary2RegisterShiftAmount_CVT_VCVT_between_floating_point_and_fixed_point_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_VADDHN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_VRADDHN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000500 /* A(11:8)=0101 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_VABAL_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000600 /* A(11:8)=0110 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_VSUBHN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000600 /* A(11:8)=0110 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterDifferentLength_I16_32_64_VRSUBHN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000700 /* A(11:8)=0111 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_VABDL_integer_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* A(11:8)=1100 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_VMULL_integer_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32L_VQDMULL_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */) {
    return VectorBinary3RegisterDifferentLength_P8_VMULL_polynomial_A2_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000800 /* A(11:8)=10x0 */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32L_VMLAL_VMLSL_integer_A2_instance_;
  }

  if ((inst.Bits() & 0x00000D00)  ==
          0x00000900 /* A(11:8)=10x1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterDifferentLength_I16_32L_VQDMLAL_VQDMLSL_A1_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000000 /* A(11:8)=000x */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32_VADDL_VADDW_instance_;
  }

  if ((inst.Bits() & 0x00000E00)  ==
          0x00000200 /* A(11:8)=001x */) {
    return VectorBinary3RegisterDifferentLength_I8_16_32_VSUBL_VSUBW_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00000F00)  ==
          0x00000000 /* A(11:8)=0000 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VHADD_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000000 /* A(11:8)=0000 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VQADD_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VRHADD_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00300000)  ==
          0x00000000 /* C(21:20)=00 */) {
    return VectorBinary3RegisterSameLengthDQ_VAND_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00300000)  ==
          0x00100000 /* C(21:20)=01 */) {
    return VectorBinary3RegisterSameLengthDQ_VBIC_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00300000)  ==
          0x00200000 /* C(21:20)=10 */) {
    return VectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00300000)  ==
          0x00300000 /* C(21:20)=11 */) {
    return VectorBinary3RegisterSameLengthDQ_VORN_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00300000)  ==
          0x00000000 /* C(21:20)=00 */) {
    return VectorBinary3RegisterSameLengthDQ_VEOR_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00300000)  ==
          0x00100000 /* C(21:20)=01 */) {
    return VectorBinary3RegisterSameLengthDQ_VBSL_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00300000)  ==
          0x00200000 /* C(21:20)=10 */) {
    return VectorBinary3RegisterSameLengthDQ_VBIT_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000100 /* A(11:8)=0001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00300000)  ==
          0x00300000 /* C(21:20)=11 */) {
    return VectorBinary3RegisterSameLengthDQ_VBIF_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000200 /* A(11:8)=0010 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VHSUB_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000200 /* A(11:8)=0010 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VQSUB_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000300 /* A(11:8)=0011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000300 /* A(11:8)=0011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQ_VSHL_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000400 /* A(11:8)=0100 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VQSHL_register_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQ_VRSHL_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000500 /* A(11:8)=0101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VQRSHL_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000600 /* A(11:8)=0110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMAX_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000600 /* A(11:8)=0110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMIN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000700 /* A(11:8)=0111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VABD_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000700 /* A(11:8)=0111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VABA_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQ_VADD_integer_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQ_VSUB_integer_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VTST_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000800 /* A(11:8)=1000 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000900 /* A(11:8)=1001 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* A(11:8)=1010 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx */) {
    return VectorBinary3RegisterSameLengthDI_VPMAX_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000A00 /* A(11:8)=1010 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx */) {
    return VectorBinary3RegisterSameLengthDI_VPMIN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */) {
    return VectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */) {
    return VectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000B00 /* A(11:8)=1011 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00000040)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx */) {
    return VectorBinary3RegisterSameLengthDI_VPADD_integer_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* A(11:8)=1100 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */ &&
      (inst.Bits() & 0x00100000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx */) {
    return VectorBinary3RegisterSameLength32_DQ_VFMA_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000C00 /* A(11:8)=1100 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */ &&
      (inst.Bits() & 0x00100000)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx */) {
    return VectorBinary3RegisterSameLength32_DQ_VFMS_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32P_VPADD_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VABD_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000D00 /* A(11:8)=1101 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VCGE_register_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VCGT_register_A2_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VACGE_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000E00 /* A(11:8)=1110 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VACGT_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMAX_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VMIN_floating_point_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32P_VPMAX_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x01000000 /* U(24)=1 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32P_VPMIN_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00000000 /* C(21:20)=0x */) {
    return VectorBinary3RegisterSameLength32_DQ_VRECPS_instance_;
  }

  if ((inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* A(11:8)=1111 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* B(4)=1 */ &&
      (inst.Bits() & 0x01000000)  ==
          0x00000000 /* U(24)=0 */ &&
      (inst.Bits() & 0x00200000)  ==
          0x00200000 /* C(21:20)=1x */) {
    return VectorBinary3RegisterSameLength32_DQ_VRSQRTS_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00F00000)  ==
          0x00800000 /* op(23:20)=1000 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterOp_STREX_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00900000 /* op(23:20)=1001 */ &&
      (inst.Bits() & 0x00000F0F)  ==
          0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterOp_LDREX_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00A00000 /* op(23:20)=1010 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterDoubleOp_STREXD_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00B00000 /* op(23:20)=1011 */ &&
      (inst.Bits() & 0x00000F0F)  ==
          0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterDoubleOp_LDREXD_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00C00000 /* op(23:20)=1100 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterOp_STREXB_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00D00000 /* op(23:20)=1101 */ &&
      (inst.Bits() & 0x00000F0F)  ==
          0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterOp_LDREXB_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00E00000 /* op(23:20)=1110 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreExclusive3RegisterOp_STREXH_instance_;
  }

  if ((inst.Bits() & 0x00F00000)  ==
          0x00F00000 /* op(23:20)=1111 */ &&
      (inst.Bits() & 0x00000F0F)  ==
          0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadExclusive2RegisterOp_STREXH_instance_;
  }

  if ((inst.Bits() & 0x00B00000)  ==
          0x00000000 /* op(23:20)=0x00 */ &&
      (inst.Bits() & 0x00000F00)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Deprecated_SWP_SWPB_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00000100)  ==
          0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000)  ==
          0x00000000 /* A(23:21)=000 */ &&
      (inst.Bits() & 0x0000006F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000 */) {
    return MoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000)  ==
          0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF)  ==
          0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */) {
    return VfpUsesRegOp_VMSR_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000)  ==
          0x00000000 /* A(23:21)=0xx */ &&
      (inst.Bits() & 0x0000000F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return MoveVfpRegisterOpWithTypeSel_VMOV_ARM_core_register_to_scalar_instance_;
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
    return DuplicateToAdvSIMDRegisters_VDUP_arm_core_register_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000)  ==
          0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF)  ==
          0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */) {
    return VfpMrsOp_VMRS_instance_;
  }

  if ((inst.Bits() & 0x00100000)  ==
          0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100)  ==
          0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x0000000F)  ==
          0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return MoveVfpRegisterOpWithTypeSel_MOVE_scalar_to_ARM_core_register_instance_;
  }

  if (true) {
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
  if ((inst.Bits() & 0x00000100)  ==
          0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x000000D0)  ==
          0x00000010 /* op(7:4)=00x1 */) {
    return MoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_instance_;
  }

  if ((inst.Bits() & 0x00000100)  ==
          0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x000000D0)  ==
          0x00000010 /* op(7:4)=00x1 */) {
    return MoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_instance_;
  }

  if (true) {
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

  if ((inst.Bits() & 0x0FF00000)  ==
          0x0C400000 /* op1(27:20)=11000100 */) {
    return Forbidden_MCRR2_instance_;
  }

  if ((inst.Bits() & 0x0FF00000)  ==
          0x0C500000 /* op1(27:20)=11000101 */) {
    return Forbidden_MRRC2_instance_;
  }

  if ((inst.Bits() & 0x0E500000)  ==
          0x08100000 /* op1(27:20)=100xx0x1 */ &&
      (inst.Bits() & 0x0000FFFF)  ==
          0x00000A00 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000 */) {
    return Forbidden_RFE_instance_;
  }

  if ((inst.Bits() & 0x0E500000)  ==
          0x08400000 /* op1(27:20)=100xx1x0 */ &&
      (inst.Bits() & 0x000FFFE0)  ==
          0x000D0500 /* $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx */) {
    return Forbidden_SRS_instance_;
  }

  if ((inst.Bits() & 0x0F100000)  ==
          0x0E000000 /* op1(27:20)=1110xxx0 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* op(4)=1 */) {
    return Forbidden_MCR2_instance_;
  }

  if ((inst.Bits() & 0x0F100000)  ==
          0x0E100000 /* op1(27:20)=1110xxx1 */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000010 /* op(4)=1 */) {
    return Forbidden_MRC2_instance_;
  }

  if ((inst.Bits() & 0x0E100000)  ==
          0x0C000000 /* op1(27:20)=110xxxx0 */ &&
      (inst.Bits() & 0x0FB00000)  !=
          0x0C000000 /* op1_repeated(27:20)=~11000x00 */) {
    return Forbidden_STC2_instance_;
  }

  if ((inst.Bits() & 0x0E100000)  ==
          0x0C100000 /* op1(27:20)=110xxxx1 */ &&
      (inst.Bits() & 0x000F0000)  !=
          0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0FB00000)  !=
          0x0C100000 /* op1_repeated(27:20)=~11000x01 */) {
    return Forbidden_LDC2_immediate_instance_;
  }

  if ((inst.Bits() & 0x0E100000)  ==
          0x0C100000 /* op1(27:20)=110xxxx1 */ &&
      (inst.Bits() & 0x000F0000)  ==
          0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x0FB00000)  !=
          0x0C100000 /* op1_repeated(27:20)=~11000x01 */) {
    return Forbidden_LDC2_literal_instance_;
  }

  if ((inst.Bits() & 0x0F000000)  ==
          0x0E000000 /* op1(27:20)=1110xxxx */ &&
      (inst.Bits() & 0x00000010)  ==
          0x00000000 /* op(4)=0 */) {
    return Forbidden_CDP2_instance_;
  }

  if ((inst.Bits() & 0x0E000000)  ==
          0x0A000000 /* op1(27:20)=101xxxxx */) {
    return Forbidden_BLX_immediate_instance_;
  }

  if ((inst.Bits() & 0x08000000)  ==
          0x00000000 /* op1(27:20)=0xxxxxxx */) {
    return decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(inst);
  }

  if (true) {
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
