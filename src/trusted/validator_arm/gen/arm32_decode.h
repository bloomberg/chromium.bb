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
  const Binary2RegisterImmediateOpDynCodeReplace Binary2RegisterImmediateOpDynCodeReplace_instance_;
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
  const NotImplemented NotImplemented_instance_;
  const PreloadRegisterPairOp PreloadRegisterPairOp_instance_;
  const PreloadRegisterPairOpWAndRnNotPc PreloadRegisterPairOpWAndRnNotPc_instance_;
  const Roadblock Roadblock_instance_;
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
  const Undefined Undefined_instance_;
  const Unpredictable Unpredictable_instance_;
  const VfpMrsOp VfpMrsOp_instance_;
  const VfpOp VfpOp_instance_;
  const NotImplemented not_implemented_;
};

}  // namespace nacl_arm_dec
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_
