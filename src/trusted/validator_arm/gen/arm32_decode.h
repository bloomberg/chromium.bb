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

  inline const ClassDecoder& decode_pack_sat_rev(
      const Instruction insn) const;

  inline const ClassDecoder& decode_parallel_add_sub(
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

  // The following fields define the set of class decoders
  // that can be returned by the API function "decode". They
  // are created once as instance fields, and then returned
  // by the table methods above. This speeds up the code since
  // the class decoders need to only be built once (and reused
  // for each call to "decode").
  const Binary2RegisterImmediateOp Binary2RegisterImmediateOp_instance_;
  const Binary3RegisterOp Binary3RegisterOp_instance_;
  const Binary3RegisterShiftedTest Binary3RegisterShiftedTest_instance_;
  const Binary4RegisterShiftedOp Binary4RegisterShiftedOp_instance_;
  const BinaryRegisterImmediateTest BinaryRegisterImmediateTest_instance_;
  const Branch Branch_instance_;
  const Breakpoint Breakpoint_instance_;
  const BxBlx BxBlx_instance_;
  const CoprocessorOp CoprocessorOp_instance_;
  const DataProc DataProc_instance_;
  const Defs12To15 Defs12To15_instance_;
  const Defs12To15RdRnRsRmNotPc Defs12To15RdRnRsRmNotPc_instance_;
  const Deprecated Deprecated_instance_;
  const EffectiveNoOp EffectiveNoOp_instance_;
  const Forbidden Forbidden_instance_;
  const LdrRegister LdrRegister_instance_;
  const LoadCoprocessor LoadCoprocessor_instance_;
  const LoadDoubleExclusive LoadDoubleExclusive_instance_;
  const LoadDoubleI LoadDoubleI_instance_;
  const LoadDoubleR LoadDoubleR_instance_;
  const LoadExclusive LoadExclusive_instance_;
  const LoadImmediate LoadImmediate_instance_;
  const LoadMultiple LoadMultiple_instance_;
  const LoadRegister LoadRegister_instance_;
  const LongMultiply LongMultiply_instance_;
  const MaskAddress MaskAddress_instance_;
  const MaybeSetsConds MaybeSetsConds_instance_;
  const MoveDoubleFromCoprocessor MoveDoubleFromCoprocessor_instance_;
  const MoveFromCoprocessor MoveFromCoprocessor_instance_;
  const MoveToStatusRegister MoveToStatusRegister_instance_;
  const Multiply Multiply_instance_;
  const PackSatRev PackSatRev_instance_;
  const Roadblock Roadblock_instance_;
  const SatAddSub SatAddSub_instance_;
  const StoreCoprocessor StoreCoprocessor_instance_;
  const StoreDoubleR StoreDoubleR_instance_;
  const StoreExclusive StoreExclusive_instance_;
  const StoreImmediate StoreImmediate_instance_;
  const StoreRegister StoreRegister_instance_;
  const StrRegister StrRegister_instance_;
  const TestIfAddressMasked TestIfAddressMasked_instance_;
  const Unary1RegisterBitRange Unary1RegisterBitRange_instance_;
  const Unary1RegisterImmediateOp Unary1RegisterImmediateOp_instance_;
  const Unary2RegisterImmedShiftedOp Unary2RegisterImmedShiftedOp_instance_;
  const Unary2RegisterOp Unary2RegisterOp_instance_;
  const Unary3RegisterShiftedOp Unary3RegisterShiftedOp_instance_;
  const Undefined Undefined_instance_;
  const Unpredictable Unpredictable_instance_;
  const VectorLoad VectorLoad_instance_;
  const VectorStore VectorStore_instance_;
};

}  // namespace nacl_arm_dec
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_
