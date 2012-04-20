

/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE


#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif



#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_DECODER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_DECODER_H_

#include "native_client/src/trusted/validator_arm/decode.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_classes.h"
#include "native_client/src/trusted/validator_arm/named_class_decoder.h"

namespace nacl_arm_test {

// Defines a (named) decoder class selector for instructions
class NamedArm32DecoderState : nacl_arm_dec::DecoderState {
 public:
  explicit NamedArm32DecoderState();
  virtual ~NamedArm32DecoderState();

  // Parses the given instruction, returning the named class
  // decoder to use.
  const NamedClassDecoder& decode_named(
     const nacl_arm_dec::Instruction) const;

  // Parses the given instruction, returning the class decoder
  // to use.
  virtual const nacl_arm_dec::ClassDecoder& decode(
     const nacl_arm_dec::Instruction) const;

  // The following fields define the set of class decoders
  // that can be returned by the API function "decode_named". They
  // are created once as instance fields, and then returned
  // by the table methods above. This speeds up the code since
  // the class decoders need to only be bulit once (and reused
  // for each call to "decode_named").

  const NamedBinary3RegisterShiftedTest Binary3RegisterShiftedTest_instance_;

  const NamedBinary4RegisterShiftedOp Binary4RegisterShiftedOp_instance_;

  const NamedBranch Branch_instance_;

  const NamedBreakpoint Breakpoint_instance_;

  const NamedBxBlx BxBlx_instance_;

  const NamedCoprocessorOp CoprocessorOp_instance_;

  const NamedDataProc DataProc_instance_;

  const NamedDeprecated Deprecated_instance_;

  const NamedEffectiveNoOp EffectiveNoOp_instance_;

  const NamedForbidden Forbidden_instance_;

  const NamedImmediateBic ImmediateBic_instance_;

  const NamedLoadCoprocessor LoadCoprocessor_instance_;

  const NamedLoadDoubleExclusive LoadDoubleExclusive_instance_;

  const NamedLoadDoubleI LoadDoubleI_instance_;

  const NamedLoadDoubleR LoadDoubleR_instance_;

  const NamedLoadExclusive LoadExclusive_instance_;

  const NamedLoadImmediate LoadImmediate_instance_;

  const NamedLoadMultiple LoadMultiple_instance_;

  const NamedLoadRegister LoadRegister_instance_;

  const NamedLongMultiply LongMultiply_instance_;

  const NamedMoveDoubleFromCoprocessor MoveDoubleFromCoprocessor_instance_;

  const NamedMoveFromCoprocessor MoveFromCoprocessor_instance_;

  const NamedMoveToStatusRegister MoveToStatusRegister_instance_;

  const NamedMultiply Multiply_instance_;

  const NamedPackSatRev PackSatRev_instance_;

  const NamedRoadblock Roadblock_instance_;

  const NamedSatAddSub SatAddSub_instance_;

  const NamedStoreCoprocessor StoreCoprocessor_instance_;

  const NamedStoreExclusive StoreExclusive_instance_;

  const NamedStoreImmediate StoreImmediate_instance_;

  const NamedStoreRegister StoreRegister_instance_;

  const NamedTest Test_instance_;

  const NamedTestImmediate TestImmediate_instance_;

  const NamedUnary3RegisterShiftedOp Unary3RegisterShiftedOp_instance_;

  const NamedUndefined Undefined_instance_;

  const NamedUnpredictable Unpredictable_instance_;

  const NamedVectorLoad VectorLoad_instance_;

  const NamedVectorStore VectorStore_instance_;

  const NamedAdd_Rule_7_A1_P26Binary4RegisterShiftedOp Add_Rule_7_A1_P26Binary4RegisterShiftedOp_instance_;

  const NamedRsb_Rule_144_A1_P288Binary4RegisterShiftedOp Rsb_Rule_144_A1_P288Binary4RegisterShiftedOp_instance_;

 private:

  // The following list of methods correspond to each decoder table,
  // and implements the pattern matching of the corresponding bit
  // patterns. After matching the corresponding bit patterns, they
  // either call other methods in this list (corresponding to another
  // decoder table), or they return the instance field that implements
  // the class decoder that should be used to decode the particular
  // instruction.

  inline const NamedClassDecoder& decode_ARMv7(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_branch_block_xfer(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_dp_immed(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_dp_misc(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_dp_reg(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_dp_reg_shifted(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_extra_load_store(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_half_mult(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_load_store_word_byte(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_media(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_misc(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_misc_hints_simd(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_msr_and_hints(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_mult(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_pack_sat_rev(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_parallel_add_sub(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_sat_add_sub(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_signed_mult(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_dp(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_dp_1imm(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_dp_2misc(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_dp_2scalar(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_dp_2shift(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_dp_3diff(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_dp_3same(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_load_store(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_load_store_l0(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_simd_load_store_l1(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_super_cop(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_sync(
      const nacl_arm_dec::Instruction insn) const;

  inline const NamedClassDecoder& decode_unconditional(
      const nacl_arm_dec::Instruction insn) const;

};

} // namespace nacl_arm_test
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_DECODER_H_
