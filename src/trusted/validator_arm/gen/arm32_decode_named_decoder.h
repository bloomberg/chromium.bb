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
  const NamedBinary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76 Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_;
  const NamedBinary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82 Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_;
  const NamedBinary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450 Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_;
  const NamedBinary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456 Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_;
  const NamedBinary2RegisterImmediateOp_Adc_Rule_6_A1_P14 Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14_instance_;
  const NamedBinary2RegisterImmediateOp_Add_Rule_5_A1_P22 Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_;
  const NamedBinary2RegisterImmediateOp_And_Rule_11_A1_P34 Binary2RegisterImmediateOp_And_Rule_11_A1_P34_instance_;
  const NamedBinary2RegisterImmediateOp_Eor_Rule_44_A1_P94 Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94_instance_;
  const NamedBinary2RegisterImmediateOp_Orr_Rule_113_A1_P228 Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228_instance_;
  const NamedBinary2RegisterImmediateOp_Rsb_Rule_142_A1_P284 Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284_instance_;
  const NamedBinary2RegisterImmediateOp_Rsc_Rule_145_A1_P290 Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290_instance_;
  const NamedBinary2RegisterImmediateOp_Sbc_Rule_151_A1_P302 Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302_instance_;
  const NamedBinary2RegisterImmediateOp_Sub_Rule_212_A1_P420 Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_;
  const NamedBinary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16 Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_;
  const NamedBinary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24 Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_;
  const NamedBinary3RegisterImmedShiftedOp_And_Rule_7_A1_P36 Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_;
  const NamedBinary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52 Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_;
  const NamedBinary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96 Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_;
  const NamedBinary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230 Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_;
  const NamedBinary3RegisterImmedShiftedOp_Rsb_Rule_143_P286 Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_;
  const NamedBinary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292 Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_;
  const NamedBinary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304 Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_;
  const NamedBinary3RegisterImmedShiftedOp_SubRule_213_A1_P422 Binary3RegisterImmedShiftedOp_SubRule_213_A1_P422_instance_;
  const NamedBinary3RegisterOp_Asr_Rule_15_A1_P42 Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_;
  const NamedBinary3RegisterOp_Lsl_Rule_89_A1_P180 Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_;
  const NamedBinary3RegisterOp_Lsr_Rule_91_A1_P184 Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_;
  const NamedBinary3RegisterOp_Ror_Rule_140_A1_P280 Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_;
  const NamedBinary3RegisterShiftedTest_Cmn_Rule_34_A1_P78 Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_;
  const NamedBinary3RegisterShiftedTest_Cmp_Rule_37_A1_P84 Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_;
  const NamedBinary3RegisterShiftedTest_Teq_Rule_229_A1_P452 Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_;
  const NamedBinary3RegisterShiftedTest_Tst_Rule_232_A1_P458 Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_;
  const NamedBinary4RegisterShiftedOp_Adc_Rule_3_A1_P18 Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_;
  const NamedBinary4RegisterShiftedOp_Add_Rule_7_A1_P26 Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_;
  const NamedBinary4RegisterShiftedOp_And_Rule_13_A1_P38 Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_;
  const NamedBinary4RegisterShiftedOp_Bic_Rule_21_A1_P54 Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_;
  const NamedBinary4RegisterShiftedOp_Eor_Rule_46_A1_P98 Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_;
  const NamedBinary4RegisterShiftedOp_Orr_Rule_115_A1_P212 Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_;
  const NamedBinary4RegisterShiftedOp_Rsb_Rule_144_A1_P288 Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_;
  const NamedBinary4RegisterShiftedOp_Rsc_Rule_147_A1_P294 Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_;
  const NamedBinary4RegisterShiftedOp_Sbc_Rule_153_A1_P306 Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_;
  const NamedBinary4RegisterShiftedOp_Sub_Rule_214_A1_P424 Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_;
  const NamedBinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74 BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74_instance_;
  const NamedBinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80 BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80_instance_;
  const NamedBinaryRegisterImmediateTest_Teq_Rule_227_A1_P448 BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448_instance_;
  const NamedBranch_None Branch_None_instance_;
  const NamedBreakpoint_None Breakpoint_None_instance_;
  const NamedBxBlx_None BxBlx_None_instance_;
  const NamedCoprocessorOp_None CoprocessorOp_None_instance_;
  const NamedDataProc_None DataProc_None_instance_;
  const NamedDeprecated_None Deprecated_None_instance_;
  const NamedEffectiveNoOp_None EffectiveNoOp_None_instance_;
  const NamedForbidden_None Forbidden_None_instance_;
  const NamedLoadCoprocessor_None LoadCoprocessor_None_instance_;
  const NamedLoadDoubleExclusive_None LoadDoubleExclusive_None_instance_;
  const NamedLoadDoubleI_None LoadDoubleI_None_instance_;
  const NamedLoadDoubleR_None LoadDoubleR_None_instance_;
  const NamedLoadExclusive_None LoadExclusive_None_instance_;
  const NamedLoadImmediate_None LoadImmediate_None_instance_;
  const NamedLoadMultiple_None LoadMultiple_None_instance_;
  const NamedLoadRegister_None LoadRegister_None_instance_;
  const NamedLongMultiply_None LongMultiply_None_instance_;
  const NamedMaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50 MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_;
  const NamedMaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454 MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_;
  const NamedMoveDoubleFromCoprocessor_None MoveDoubleFromCoprocessor_None_instance_;
  const NamedMoveFromCoprocessor_None MoveFromCoprocessor_None_instance_;
  const NamedMoveToStatusRegister_None MoveToStatusRegister_None_instance_;
  const NamedMultiply_None Multiply_None_instance_;
  const NamedPackSatRev_None PackSatRev_None_instance_;
  const NamedRoadblock_None Roadblock_None_instance_;
  const NamedSatAddSub_None SatAddSub_None_instance_;
  const NamedStoreCoprocessor_None StoreCoprocessor_None_instance_;
  const NamedStoreExclusive_None StoreExclusive_None_instance_;
  const NamedStoreImmediate_None StoreImmediate_None_instance_;
  const NamedStoreRegister_None StoreRegister_None_instance_;
  const NamedUnary1RegisterBitRange_Bfc_17_A1_P46 Unary1RegisterBitRange_Bfc_17_A1_P46_instance_;
  const NamedUnary1RegisterImmediateOp_Adr_Rule_10_A1_P32 Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32_instance_;
  const NamedUnary1RegisterImmediateOp_Adr_Rule_10_A2_P32 Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32_instance_;
  const NamedUnary1RegisterImmediateOp_Mov_Rule_96_A1_P194 Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194_instance_;
  const NamedUnary1RegisterImmediateOp_Mov_Rule_96_A2_P_194 Unary1RegisterImmediateOp_Mov_Rule_96_A2_P_194_instance_;
  const NamedUnary1RegisterImmediateOp_Mvn_Rule_106_A1_P214 Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214_instance_;
  const NamedUnary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40 Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_;
  const NamedUnary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178 Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_;
  const NamedUnary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182 Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_;
  const NamedUnary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216 Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_;
  const NamedUnary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278 Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_;
  const NamedUnary2RegisterOp_Mov_Rule_97_A1_P196 Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_;
  const NamedUnary2RegisterOp_Rrx_Rule_141_A1_P282 Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_;
  const NamedUnary3RegisterShiftedOp_Mvn_Rule_108_A1_P218 Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_;
  const NamedUndefined_None Undefined_None_instance_;
  const NamedUnpredictable_None Unpredictable_None_instance_;
  const NamedVectorLoad_None VectorLoad_None_instance_;
  const NamedVectorStore_None VectorStore_None_instance_;
  const NamedDefs12To15_Adc_Rule_2_A1_P16 Defs12To15_Adc_Rule_2_A1_P16_instance_;
  const NamedDefs12To15_Add_Rule_6_A1_P24 Defs12To15_Add_Rule_6_A1_P24_instance_;
  const NamedDefs12To15_And_Rule_7_A1_P36 Defs12To15_And_Rule_7_A1_P36_instance_;
  const NamedDefs12To15_Bic_Rule_20_A1_P52 Defs12To15_Bic_Rule_20_A1_P52_instance_;
  const NamedDefs12To15_Eor_Rule_45_A1_P96 Defs12To15_Eor_Rule_45_A1_P96_instance_;
  const NamedDefs12To15_Mov_Rule_96_A2_P_194 Defs12To15_Mov_Rule_96_A2_P_194_instance_;
  const NamedDefs12To15_Orr_Rule_114_A1_P230 Defs12To15_Orr_Rule_114_A1_P230_instance_;
  const NamedDefs12To15_Rsb_Rule_143_P286 Defs12To15_Rsb_Rule_143_P286_instance_;
  const NamedDefs12To15_Rsc_Rule_146_A1_P292 Defs12To15_Rsc_Rule_146_A1_P292_instance_;
  const NamedDefs12To15_Sbc_Rule_152_A1_P304 Defs12To15_Sbc_Rule_152_A1_P304_instance_;
  const NamedDefs12To15_SubRule_213_A1_P422 Defs12To15_SubRule_213_A1_P422_instance_;
  const NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26 Defs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26_instance_;
  const NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288 Defs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288_instance_;
  const NamedMaskAddress_Bic_Rule_19_A1_P50 MaskAddress_Bic_Rule_19_A1_P50_instance_;
  const NamedMaybeSetsConds_Cmn_Rule_33_A1_P76 MaybeSetsConds_Cmn_Rule_33_A1_P76_instance_;
  const NamedMaybeSetsConds_Cmp_Rule_36_A1_P82 MaybeSetsConds_Cmp_Rule_36_A1_P82_instance_;
  const NamedMaybeSetsConds_Teq_Rule_228_A1_P450 MaybeSetsConds_Teq_Rule_228_A1_P450_instance_;
  const NamedMaybeSetsConds_Tst_Rule_231_A1_P456 MaybeSetsConds_Tst_Rule_231_A1_P456_instance_;
  const NamedTestIfAddressMasked_Tst_Rule_230_A1_P454 TestIfAddressMasked_Tst_Rule_230_A1_P454_instance_;
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
