/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE


#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_

#include "native_client/src/trusted/validator_arm/decode.h"
#include "native_client/src/trusted/validator_arm/actual_classes.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"

namespace nacl_arm_dec {

// Defines a decoder class selector for instructions.
class Arm32DecoderState : DecoderState {
 public:
   explicit Arm32DecoderState();

   // Parses the given instruction, returning the decoder to use.
   virtual const ClassDecoder& decode(const Instruction) const;

 private:

  // The following list of methods correspond to each decoder table,
  // and implements the pattern matching of the corresponding bit
  // patterns. After matching the corresponding bit patterns, they
  // either call other methods in this list (corresponding to another
  // decoder table), or they return the instance field that implements
  // the class decoder that should be used to decode the particular
  // instruction.

  inline const ClassDecoder& decode_ARMv7(
      const Instruction inst) const;

  inline const ClassDecoder& decode_advanced_simd_data_processing_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_advanced_simd_element_or_structure_load_store_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_branch_branch_with_link_and_block_data_transfer(
      const Instruction inst) const;

  inline const ClassDecoder& decode_coprocessor_instructions_and_supervisor_call(
      const Instruction inst) const;

  inline const ClassDecoder& decode_data_processing_and_miscellaneous_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_data_processing_immediate(
      const Instruction inst) const;

  inline const ClassDecoder& decode_data_processing_register(
      const Instruction inst) const;

  inline const ClassDecoder& decode_data_processing_register_shifted_register(
      const Instruction inst) const;

  inline const ClassDecoder& decode_extension_register_load_store_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_extra_load_store_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_floating_point_data_processing_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_halfword_multiply_and_multiply_accumulate(
      const Instruction inst) const;

  inline const ClassDecoder& decode_load_store_word_and_unsigned_byte(
      const Instruction inst) const;

  inline const ClassDecoder& decode_load_store_word_and_unsigned_byte_str_or_push(
      const Instruction inst) const;

  inline const ClassDecoder& decode_media_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_miscellaneous_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_msr_immediate_and_hints(
      const Instruction inst) const;

  inline const ClassDecoder& decode_multiply_and_multiply_accumulate(
      const Instruction inst) const;

  inline const ClassDecoder& decode_other_floating_point_data_processing_instructions(
      const Instruction inst) const;

  inline const ClassDecoder& decode_packing_unpacking_saturation_and_reversal(
      const Instruction inst) const;

  inline const ClassDecoder& decode_parallel_addition_and_subtraction_signed(
      const Instruction inst) const;

  inline const ClassDecoder& decode_parallel_addition_and_subtraction_unsigned(
      const Instruction inst) const;

  inline const ClassDecoder& decode_saturating_addition_and_subtraction(
      const Instruction inst) const;

  inline const ClassDecoder& decode_signed_multiply_signed_and_unsigned_divide(
      const Instruction inst) const;

  inline const ClassDecoder& decode_simd_dp_1imm(
      const Instruction inst) const;

  inline const ClassDecoder& decode_simd_dp_2misc(
      const Instruction inst) const;

  inline const ClassDecoder& decode_simd_dp_2scalar(
      const Instruction inst) const;

  inline const ClassDecoder& decode_simd_dp_2shift(
      const Instruction inst) const;

  inline const ClassDecoder& decode_simd_dp_3diff(
      const Instruction inst) const;

  inline const ClassDecoder& decode_simd_dp_3same(
      const Instruction inst) const;

  inline const ClassDecoder& decode_synchronization_primitives(
      const Instruction inst) const;

  inline const ClassDecoder& decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(
      const Instruction inst) const;

  inline const ClassDecoder& decode_transfer_between_arm_core_and_extension_registers_64_bit(
      const Instruction inst) const;

  inline const ClassDecoder& decode_unconditional_instructions(
      const Instruction inst) const;

  // The following fields define the set of class decoders
  // that can be returned by the API function "decode". They
  // are created once as instance fields, and then returned
  // by the table methods above. This speeds up the code since
  // the class decoders need to only be built once (and reused
  // for each call to "decode").
  const Binary2RegisterImmedShiftedTest Binary2RegisterImmedShiftedTest_instance_;
  const Binary2RegisterImmediateOpDynCodeReplace Binary2RegisterImmediateOpDynCodeReplace_instance_;
  const Binary3RegisterShiftedOp Binary3RegisterShiftedOp_instance_;
  const Branch Branch_instance_;
  const Breakpoint Breakpoint_instance_;
  const BxBlx BxBlx_instance_;
  const DataBarrier DataBarrier_instance_;
  const Defs12To15 Defs12To15_instance_;
  const Defs12To15CondsDontCareMsbGeLsb Defs12To15CondsDontCareMsbGeLsb_instance_;
  const Defs12To15CondsDontCareRdRnNotPc Defs12To15CondsDontCareRdRnNotPc_instance_;
  const Defs12To15CondsDontCareRdRnNotPcBitfieldExtract Defs12To15CondsDontCareRdRnNotPcBitfieldExtract_instance_;
  const Defs12To15CondsDontCareRnRdRmNotPc Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  const Defs12To15RdRmRnNotPc Defs12To15RdRmRnNotPc_instance_;
  const Defs12To15RdRnNotPc Defs12To15RdRnNotPc_instance_;
  const Defs12To15RdRnRsRmNotPc Defs12To15RdRnRsRmNotPc_instance_;
  const Defs12To19CondsDontCareRdRmRnNotPc Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  const Defs16To19CondsDontCareRdRaRmRnNotPc Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  const Defs16To19CondsDontCareRdRmRnNotPc Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  const Deprecated Deprecated_instance_;
  const DontCareInst DontCareInst_instance_;
  const DontCareInstRdNotPc DontCareInstRdNotPc_instance_;
  const DontCareInstRnRsRmNotPc DontCareInstRnRsRmNotPc_instance_;
  const DuplicateToAdvSIMDRegisters DuplicateToAdvSIMDRegisters_instance_;
  const Forbidden Forbidden_instance_;
  const InstructionBarrier InstructionBarrier_instance_;
  const LdrImmediateOp LdrImmediateOp_instance_;
  const LoadBasedImmedMemory LoadBasedImmedMemory_instance_;
  const LoadBasedImmedMemoryDouble LoadBasedImmedMemoryDouble_instance_;
  const LoadBasedMemory LoadBasedMemory_instance_;
  const LoadBasedMemoryDouble LoadBasedMemoryDouble_instance_;
  const LoadBasedOffsetMemory LoadBasedOffsetMemory_instance_;
  const LoadBasedOffsetMemoryDouble LoadBasedOffsetMemoryDouble_instance_;
  const LoadMultiple LoadMultiple_instance_;
  const LoadVectorRegister LoadVectorRegister_instance_;
  const LoadVectorRegisterList LoadVectorRegisterList_instance_;
  const MaskAddress MaskAddress_instance_;
  const MoveDoubleVfpRegisterOp MoveDoubleVfpRegisterOp_instance_;
  const MoveVfpRegisterOp MoveVfpRegisterOp_instance_;
  const MoveVfpRegisterOpWithTypeSel MoveVfpRegisterOpWithTypeSel_instance_;
  const PermanentlyUndefined PermanentlyUndefined_instance_;
  const PreloadRegisterImm12Op PreloadRegisterImm12Op_instance_;
  const PreloadRegisterPairOp PreloadRegisterPairOp_instance_;
  const Store2RegisterImm12OpRnNotRtOnWriteback Store2RegisterImm12OpRnNotRtOnWriteback_instance_;
  const StoreBasedImmedMemory StoreBasedImmedMemory_instance_;
  const StoreBasedImmedMemoryDouble StoreBasedImmedMemoryDouble_instance_;
  const StoreBasedMemoryDoubleRtBits0To3 StoreBasedMemoryDoubleRtBits0To3_instance_;
  const StoreBasedMemoryRtBits0To3 StoreBasedMemoryRtBits0To3_instance_;
  const StoreBasedOffsetMemory StoreBasedOffsetMemory_instance_;
  const StoreBasedOffsetMemoryDouble StoreBasedOffsetMemoryDouble_instance_;
  const StoreRegisterList StoreRegisterList_instance_;
  const StoreVectorRegister StoreVectorRegister_instance_;
  const StoreVectorRegisterList StoreVectorRegisterList_instance_;
  const TestIfAddressMasked TestIfAddressMasked_instance_;
  const Unary1RegisterBitRangeMsbGeLsb Unary1RegisterBitRangeMsbGeLsb_instance_;
  const Unary1RegisterImmediateOpDynCodeReplace Unary1RegisterImmediateOpDynCodeReplace_instance_;
  const Unary1RegisterSet Unary1RegisterSet_instance_;
  const Unary1RegisterUse Unary1RegisterUse_instance_;
  const Unary2RegisterOp Unary2RegisterOp_instance_;
  const Unary2RegisterShiftedOp Unary2RegisterShiftedOp_instance_;
  const Unary2RegisterShiftedOpImmNotZero Unary2RegisterShiftedOpImmNotZero_instance_;
  const Undefined Undefined_instance_;
  const Unpredictable Unpredictable_instance_;
  const Vector1RegisterImmediate_BIT Vector1RegisterImmediate_BIT_instance_;
  const Vector1RegisterImmediate_MOV Vector1RegisterImmediate_MOV_instance_;
  const Vector1RegisterImmediate_MVN Vector1RegisterImmediate_MVN_instance_;
  const Vector2RegisterMiscellaneous_CVT_F2I Vector2RegisterMiscellaneous_CVT_F2I_instance_;
  const Vector2RegisterMiscellaneous_CVT_H2S Vector2RegisterMiscellaneous_CVT_H2S_instance_;
  const Vector2RegisterMiscellaneous_F32 Vector2RegisterMiscellaneous_F32_instance_;
  const Vector2RegisterMiscellaneous_I16_32_64N Vector2RegisterMiscellaneous_I16_32_64N_instance_;
  const Vector2RegisterMiscellaneous_I8_16_32L Vector2RegisterMiscellaneous_I8_16_32L_instance_;
  const Vector2RegisterMiscellaneous_RG Vector2RegisterMiscellaneous_RG_instance_;
  const Vector2RegisterMiscellaneous_V16_32_64N Vector2RegisterMiscellaneous_V16_32_64N_instance_;
  const Vector2RegisterMiscellaneous_V8 Vector2RegisterMiscellaneous_V8_instance_;
  const Vector2RegisterMiscellaneous_V8S Vector2RegisterMiscellaneous_V8S_instance_;
  const Vector2RegisterMiscellaneous_V8_16_32 Vector2RegisterMiscellaneous_V8_16_32_instance_;
  const Vector2RegisterMiscellaneous_V8_16_32I Vector2RegisterMiscellaneous_V8_16_32I_instance_;
  const Vector2RegisterMiscellaneous_V8_16_32T Vector2RegisterMiscellaneous_V8_16_32T_instance_;
  const VectorBinary2RegisterScalar_F32 VectorBinary2RegisterScalar_F32_instance_;
  const VectorBinary2RegisterScalar_I16_32 VectorBinary2RegisterScalar_I16_32_instance_;
  const VectorBinary2RegisterScalar_I16_32L VectorBinary2RegisterScalar_I16_32L_instance_;
  const VectorBinary2RegisterShiftAmount_CVT VectorBinary2RegisterShiftAmount_CVT_instance_;
  const VectorBinary2RegisterShiftAmount_E8_16_32L VectorBinary2RegisterShiftAmount_E8_16_32L_instance_;
  const VectorBinary2RegisterShiftAmount_I VectorBinary2RegisterShiftAmount_I_instance_;
  const VectorBinary2RegisterShiftAmount_ILS VectorBinary2RegisterShiftAmount_ILS_instance_;
  const VectorBinary2RegisterShiftAmount_N16_32_64R VectorBinary2RegisterShiftAmount_N16_32_64R_instance_;
  const VectorBinary2RegisterShiftAmount_N16_32_64RS VectorBinary2RegisterShiftAmount_N16_32_64RS_instance_;
  const VectorBinary3RegisterDifferentLength_I16_32L VectorBinary3RegisterDifferentLength_I16_32L_instance_;
  const VectorBinary3RegisterDifferentLength_I16_32_64 VectorBinary3RegisterDifferentLength_I16_32_64_instance_;
  const VectorBinary3RegisterDifferentLength_I8_16_32 VectorBinary3RegisterDifferentLength_I8_16_32_instance_;
  const VectorBinary3RegisterDifferentLength_I8_16_32L VectorBinary3RegisterDifferentLength_I8_16_32L_instance_;
  const VectorBinary3RegisterDifferentLength_P8 VectorBinary3RegisterDifferentLength_P8_instance_;
  const VectorBinary3RegisterImmOp VectorBinary3RegisterImmOp_instance_;
  const VectorBinary3RegisterLookupOp VectorBinary3RegisterLookupOp_instance_;
  const VectorBinary3RegisterSameLength32P VectorBinary3RegisterSameLength32P_instance_;
  const VectorBinary3RegisterSameLength32_DQ VectorBinary3RegisterSameLength32_DQ_instance_;
  const VectorBinary3RegisterSameLengthDI VectorBinary3RegisterSameLengthDI_instance_;
  const VectorBinary3RegisterSameLengthDQ VectorBinary3RegisterSameLengthDQ_instance_;
  const VectorBinary3RegisterSameLengthDQI16_32 VectorBinary3RegisterSameLengthDQI16_32_instance_;
  const VectorBinary3RegisterSameLengthDQI8P VectorBinary3RegisterSameLengthDQI8P_instance_;
  const VectorBinary3RegisterSameLengthDQI8_16_32 VectorBinary3RegisterSameLengthDQI8_16_32_instance_;
  const VectorLoadSingle1AllLanes VectorLoadSingle1AllLanes_instance_;
  const VectorLoadSingle2AllLanes VectorLoadSingle2AllLanes_instance_;
  const VectorLoadSingle3AllLanes VectorLoadSingle3AllLanes_instance_;
  const VectorLoadSingle4AllLanes VectorLoadSingle4AllLanes_instance_;
  const VectorLoadStoreMultiple1 VectorLoadStoreMultiple1_instance_;
  const VectorLoadStoreMultiple2 VectorLoadStoreMultiple2_instance_;
  const VectorLoadStoreMultiple3 VectorLoadStoreMultiple3_instance_;
  const VectorLoadStoreMultiple4 VectorLoadStoreMultiple4_instance_;
  const VectorLoadStoreSingle1 VectorLoadStoreSingle1_instance_;
  const VectorLoadStoreSingle2 VectorLoadStoreSingle2_instance_;
  const VectorLoadStoreSingle3 VectorLoadStoreSingle3_instance_;
  const VectorLoadStoreSingle4 VectorLoadStoreSingle4_instance_;
  const VectorUnary2RegisterDup VectorUnary2RegisterDup_instance_;
  const VfpMrsOp VfpMrsOp_instance_;
  const VfpOp VfpOp_instance_;
  const NotImplemented not_implemented_;
};

}  // namespace nacl_arm_dec
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_
