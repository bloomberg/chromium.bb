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
  const NamedBinary2RegisterImmedShiftedTest Binary2RegisterImmedShiftedTest_instance_;
  const NamedBinary3RegisterImmedShiftedOp Binary3RegisterImmedShiftedOp_instance_;
  const NamedBinary3RegisterOp Binary3RegisterOp_instance_;
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
  const NamedUnary1RegisterImmediateOp Unary1RegisterImmediateOp_instance_;
  const NamedUnary2RegisterImmedShiftedOp Unary2RegisterImmedShiftedOp_instance_;
  const NamedUnary2RegisterOp Unary2RegisterOp_instance_;
  const NamedUnary3RegisterShiftedOp Unary3RegisterShiftedOp_instance_;
  const NamedUndefined Undefined_instance_;
  const NamedUnpredictable Unpredictable_instance_;
  const NamedVectorLoad VectorLoad_instance_;
  const NamedVectorStore VectorStore_instance_;
  const NamedCmn_Rule_33_A1_P76Binary2RegisterImmedShiftedTest Cmn_Rule_33_A1_P76Binary2RegisterImmedShiftedTest_instance_;
  const NamedCmp_Rule_36_A1_P82Binary2RegisterImmedShiftedTest Cmp_Rule_36_A1_P82Binary2RegisterImmedShiftedTest_instance_;
  const NamedTeq_Rule_228_A1_P450Binary2RegisterImmedShiftedTest Teq_Rule_228_A1_P450Binary2RegisterImmedShiftedTest_instance_;
  const NamedTst_Rule_231_A1_P456Binary2RegisterImmedShiftedTest Tst_Rule_231_A1_P456Binary2RegisterImmedShiftedTest_instance_;
  const NamedAdc_Rule_2_A1_P16Binary3RegisterImmedShiftedOp Adc_Rule_2_A1_P16Binary3RegisterImmedShiftedOp_instance_;
  const NamedAdd_Rule_6_A1_P24Binary3RegisterImmedShiftedOp Add_Rule_6_A1_P24Binary3RegisterImmedShiftedOp_instance_;
  const NamedAnd_Rule_7_A1_P36Binary3RegisterImmedShiftedOp And_Rule_7_A1_P36Binary3RegisterImmedShiftedOp_instance_;
  const NamedBic_Rule_20_A1_P52Binary3RegisterImmedShiftedOp Bic_Rule_20_A1_P52Binary3RegisterImmedShiftedOp_instance_;
  const NamedEor_Rule_45_A1_P96Binary3RegisterImmedShiftedOp Eor_Rule_45_A1_P96Binary3RegisterImmedShiftedOp_instance_;
  const NamedOrr_Rule_114_A1_P230Binary3RegisterImmedShiftedOp Orr_Rule_114_A1_P230Binary3RegisterImmedShiftedOp_instance_;
  const NamedRsb_Rule_143_P286Binary3RegisterImmedShiftedOp Rsb_Rule_143_P286Binary3RegisterImmedShiftedOp_instance_;
  const NamedRsc_Rule_146_A1_P292Binary3RegisterImmedShiftedOp Rsc_Rule_146_A1_P292Binary3RegisterImmedShiftedOp_instance_;
  const NamedSbc_Rule_152_A1_P304Binary3RegisterImmedShiftedOp Sbc_Rule_152_A1_P304Binary3RegisterImmedShiftedOp_instance_;
  const NamedSubRule_213_A1_P422Binary3RegisterImmedShiftedOp SubRule_213_A1_P422Binary3RegisterImmedShiftedOp_instance_;
  const NamedAsr_Rule_15_A1_P42Binary3RegisterOp Asr_Rule_15_A1_P42Binary3RegisterOp_instance_;
  const NamedLsl_Rule_89_A1_P180Binary3RegisterOp Lsl_Rule_89_A1_P180Binary3RegisterOp_instance_;
  const NamedLsr_Rule_91_A1_P184Binary3RegisterOp Lsr_Rule_91_A1_P184Binary3RegisterOp_instance_;
  const NamedRor_Rule_140_A1_P280Binary3RegisterOp Ror_Rule_140_A1_P280Binary3RegisterOp_instance_;
  const NamedCmn_Rule_34_A1_P78Binary3RegisterShiftedTest Cmn_Rule_34_A1_P78Binary3RegisterShiftedTest_instance_;
  const NamedCmp_Rule_37_A1_P84Binary3RegisterShiftedTest Cmp_Rule_37_A1_P84Binary3RegisterShiftedTest_instance_;
  const NamedTeq_Rule_229_A1_P452Binary3RegisterShiftedTest Teq_Rule_229_A1_P452Binary3RegisterShiftedTest_instance_;
  const NamedTst_Rule_232_A1_P458Binary3RegisterShiftedTest Tst_Rule_232_A1_P458Binary3RegisterShiftedTest_instance_;
  const NamedAdc_Rule_3_A1_P18Binary4RegisterShiftedOp Adc_Rule_3_A1_P18Binary4RegisterShiftedOp_instance_;
  const NamedAdd_Rule_7_A1_P26Binary4RegisterShiftedOp Add_Rule_7_A1_P26Binary4RegisterShiftedOp_instance_;
  const NamedAnd_Rule_13_A1_P38Binary4RegisterShiftedOp And_Rule_13_A1_P38Binary4RegisterShiftedOp_instance_;
  const NamedBic_Rule_21_A1_P54Binary4RegisterShiftedOp Bic_Rule_21_A1_P54Binary4RegisterShiftedOp_instance_;
  const NamedEor_Rule_46_A1_P98Binary4RegisterShiftedOp Eor_Rule_46_A1_P98Binary4RegisterShiftedOp_instance_;
  const NamedOrr_Rule_115_A1_P212Binary4RegisterShiftedOp Orr_Rule_115_A1_P212Binary4RegisterShiftedOp_instance_;
  const NamedRsb_Rule_144_A1_P288Binary4RegisterShiftedOp Rsb_Rule_144_A1_P288Binary4RegisterShiftedOp_instance_;
  const NamedRsc_Rule_147_A1_P294Binary4RegisterShiftedOp Rsc_Rule_147_A1_P294Binary4RegisterShiftedOp_instance_;
  const NamedSbc_Rule_153_A1_P306Binary4RegisterShiftedOp Sbc_Rule_153_A1_P306Binary4RegisterShiftedOp_instance_;
  const NamedSub_Rule_214_A1_P424Binary4RegisterShiftedOp Sub_Rule_214_A1_P424Binary4RegisterShiftedOp_instance_;
  const NamedMov_Rule_96_A2_P_194Unary1RegisterImmediateOp Mov_Rule_96_A2_P_194Unary1RegisterImmediateOp_instance_;
  const NamedAsr_Rule_14_A1_P40Unary2RegisterImmedShiftedOp Asr_Rule_14_A1_P40Unary2RegisterImmedShiftedOp_instance_;
  const NamedLsl_Rule_88_A1_P178Unary2RegisterImmedShiftedOp Lsl_Rule_88_A1_P178Unary2RegisterImmedShiftedOp_instance_;
  const NamedLsr_Rule_90_A1_P182Unary2RegisterImmedShiftedOp Lsr_Rule_90_A1_P182Unary2RegisterImmedShiftedOp_instance_;
  const NamedMvn_Rule_107_A1_P216Unary2RegisterImmedShiftedOp Mvn_Rule_107_A1_P216Unary2RegisterImmedShiftedOp_instance_;
  const NamedRor_Rule_139_A1_P278Unary2RegisterImmedShiftedOp Ror_Rule_139_A1_P278Unary2RegisterImmedShiftedOp_instance_;
  const NamedMov_Rule_97_A1_P196Unary2RegisterOp Mov_Rule_97_A1_P196Unary2RegisterOp_instance_;
  const NamedRrx_Rule_141_A1_P282Unary2RegisterOp Rrx_Rule_141_A1_P282Unary2RegisterOp_instance_;
  const NamedMvn_Rule_108_A1_P218Unary3RegisterShiftedOp Mvn_Rule_108_A1_P218Unary3RegisterShiftedOp_instance_;
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
