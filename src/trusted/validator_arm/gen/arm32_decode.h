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
   virtual ~Arm32DecoderState();

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
      const Instruction insn) const;

  inline const ClassDecoder& decode_branch_block_xfer(
      const Instruction insn) const;

  inline const ClassDecoder& decode_dp_immed(
      const Instruction insn) const;

  inline const ClassDecoder& decode_dp_misc(
      const Instruction insn) const;

  inline const ClassDecoder& decode_dp_reg(
      const Instruction insn) const;

  inline const ClassDecoder& decode_dp_reg_shifted(
      const Instruction insn) const;

  inline const ClassDecoder& decode_ext_reg_load_store(
      const Instruction insn) const;

  inline const ClassDecoder& decode_ext_reg_move(
      const Instruction insn) const;

  inline const ClassDecoder& decode_ext_reg_transfers(
      const Instruction insn) const;

  inline const ClassDecoder& decode_extra_load_store(
      const Instruction insn) const;

  inline const ClassDecoder& decode_half_mult(
      const Instruction insn) const;

  inline const ClassDecoder& decode_load_store_word_byte(
      const Instruction insn) const;

  inline const ClassDecoder& decode_media(
      const Instruction insn) const;

  inline const ClassDecoder& decode_misc(
      const Instruction insn) const;

  inline const ClassDecoder& decode_misc_hints_simd(
      const Instruction insn) const;

  inline const ClassDecoder& decode_msr_and_hints(
      const Instruction insn) const;

  inline const ClassDecoder& decode_mult(
      const Instruction insn) const;

  inline const ClassDecoder& decode_other_vfp_data_proc(
      const Instruction insn) const;

  inline const ClassDecoder& decode_pack_sat_rev(
      const Instruction insn) const;

  inline const ClassDecoder& decode_parallel_add_sub_signed(
      const Instruction insn) const;

  inline const ClassDecoder& decode_parallel_add_sub_unsigned(
      const Instruction insn) const;

  inline const ClassDecoder& decode_sat_add_sub(
      const Instruction insn) const;

  inline const ClassDecoder& decode_signed_mult(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_dp(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_dp_1imm(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_dp_2misc(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_dp_2scalar(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_dp_2shift(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_dp_3diff(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_dp_3same(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_load_store(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_load_store_l0(
      const Instruction insn) const;

  inline const ClassDecoder& decode_simd_load_store_l1(
      const Instruction insn) const;

  inline const ClassDecoder& decode_super_cop(
      const Instruction insn) const;

  inline const ClassDecoder& decode_sync(
      const Instruction insn) const;

  inline const ClassDecoder& decode_unconditional(
      const Instruction insn) const;

  inline const ClassDecoder& decode_vfp_data_proc(
      const Instruction insn) const;

  // The following fields define the set of class decoders
  // that can be returned by the API function "decode". They
  // are created once as instance fields, and then returned
  // by the table methods above. This speeds up the code since
  // the class decoders need to only be built once (and reused
  // for each call to "decode").
  const Branch Branch_instance_;
  const Breakpoint Breakpoint_instance_;
  const BxBlx BxBlx_instance_;
  const CoprocessorOp CoprocessorOp_instance_;
  const DataProc DataProc_instance_;
  const Defs12To15 Defs12To15_instance_;
  const Defs12To15CondsDontCare Defs12To15CondsDontCare_instance_;
  const Defs12To15CondsDontCareRdRnRsRmNotPc Defs12To15CondsDontCareRdRnRsRmNotPc_instance_;
  const Defs12To15CondsDontCareRnRdRmNotPc Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  const Defs12To15RdRmRnNotPc Defs12To15RdRmRnNotPc_instance_;
  const Defs12To15RdRnNotPc Defs12To15RdRnNotPc_instance_;
  const Defs12To15RdRnRsRmNotPc Defs12To15RdRnRsRmNotPc_instance_;
  const Defs12To19CondsDontCareRdRmRnNotPc Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  const Defs16To19CondsDontCareRdRaRmRnNotPc Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  const Defs16To19CondsDontCareRdRmRnNotPc Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  const Deprecated Deprecated_instance_;
  const DontCareInst DontCareInst_instance_;
  const DontCareInstRnRsRmNotPc DontCareInstRnRsRmNotPc_instance_;
  const EffectiveNoOp EffectiveNoOp_instance_;
  const Forbidden Forbidden_instance_;
  const LoadBasedImmedMemory LoadBasedImmedMemory_instance_;
  const LoadBasedImmedMemoryDouble LoadBasedImmedMemoryDouble_instance_;
  const LoadBasedMemory LoadBasedMemory_instance_;
  const LoadBasedMemoryDouble LoadBasedMemoryDouble_instance_;
  const LoadBasedOffsetMemory LoadBasedOffsetMemory_instance_;
  const LoadBasedOffsetMemoryDouble LoadBasedOffsetMemoryDouble_instance_;
  const LoadCoprocessor LoadCoprocessor_instance_;
  const LoadMultiple LoadMultiple_instance_;
  const MaskAddress MaskAddress_instance_;
  const MoveDoubleFromCoprocessor MoveDoubleFromCoprocessor_instance_;
  const MoveFromCoprocessor MoveFromCoprocessor_instance_;
  const PackSatRev PackSatRev_instance_;
  const Roadblock Roadblock_instance_;
  const StoreBasedImmedMemory StoreBasedImmedMemory_instance_;
  const StoreBasedImmedMemoryDouble StoreBasedImmedMemoryDouble_instance_;
  const StoreBasedMemoryDoubleRtBits0To3 StoreBasedMemoryDoubleRtBits0To3_instance_;
  const StoreBasedMemoryRtBits0To3 StoreBasedMemoryRtBits0To3_instance_;
  const StoreBasedOffsetMemory StoreBasedOffsetMemory_instance_;
  const StoreBasedOffsetMemoryDouble StoreBasedOffsetMemoryDouble_instance_;
  const StoreCoprocessor StoreCoprocessor_instance_;
  const StoreRegisterList StoreRegisterList_instance_;
  const TestIfAddressMasked TestIfAddressMasked_instance_;
  const Unary1RegisterBitRange Unary1RegisterBitRange_instance_;
  const Unary1RegisterSet Unary1RegisterSet_instance_;
  const Unary1RegisterUse Unary1RegisterUse_instance_;
  const Undefined Undefined_instance_;
  const Unpredictable Unpredictable_instance_;
  const VectorLoad VectorLoad_instance_;
  const VectorStore VectorStore_instance_;
  const VfpOp VfpOp_instance_;
  const NotImplemented not_implemented_;
};

}  // namespace nacl_arm_dec
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_
