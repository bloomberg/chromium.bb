/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif


#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/actual_vs_baseline.h"
#include "native_client/src/trusted/validator_arm/actual_classes.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"
#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"

namespace nacl_arm_test {

// Generates a derived class decoder tester for each decoder action
// in the parse tables. This derived class introduces a default
// constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

class Binary2RegisterImmedShiftedTestTester_Cmn_Rule_33_A1_P76_
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTester_Cmn_Rule_33_A1_P76_()
    : Binary2RegisterImmedShiftedTestTester(
      state_.Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_)
  {}
};

class Binary2RegisterImmedShiftedTestTester_Cmp_Rule_36_A1_P82_
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTester_Cmp_Rule_36_A1_P82_()
    : Binary2RegisterImmedShiftedTestTester(
      state_.Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_)
  {}
};

class Binary2RegisterImmedShiftedTestTester_Teq_Rule_228_A1_P450_
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTester_Teq_Rule_228_A1_P450_()
    : Binary2RegisterImmedShiftedTestTester(
      state_.Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_)
  {}
};

class Binary2RegisterImmedShiftedTestTester_Tst_Rule_231_A1_P456_
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTester_Tst_Rule_231_A1_P456_()
    : Binary2RegisterImmedShiftedTestTester(
      state_.Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_Adc_Rule_6_A1_P14_NotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_Adc_Rule_6_A1_P14_NotRdIsPcAndS()
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS()
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(
      state_.Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_And_Rule_11_A1_P34_NotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_And_Rule_11_A1_P34_NotRdIsPcAndS()
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_And_Rule_11_A1_P34_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_Eor_Rule_44_A1_P94_NotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_Eor_Rule_44_A1_P94_NotRdIsPcAndS()
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_Orr_Rule_113_A1_P228_NotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_Orr_Rule_113_A1_P228_NotRdIsPcAndS()
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_Rsb_Rule_142_A1_P284_NotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_Rsb_Rule_142_A1_P284_NotRdIsPcAndS()
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_Rsc_Rule_145_A1_P290_NotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_Rsc_Rule_145_A1_P290_NotRdIsPcAndS()
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_Sbc_Rule_151_A1_P302_NotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_Sbc_Rule_151_A1_P302_NotRdIsPcAndS()
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS()
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(
      state_.Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_Adc_Rule_2_A1_P16_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_Adc_Rule_2_A1_P16_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_Add_Rule_6_A1_P24_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_Add_Rule_6_A1_P24_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_And_Rule_7_A1_P36_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_And_Rule_7_A1_P36_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_Bic_Rule_20_A1_P52_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_Bic_Rule_20_A1_P52_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_Eor_Rule_45_A1_P96_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_Eor_Rule_45_A1_P96_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_Orr_Rule_114_A1_P230_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_Orr_Rule_114_A1_P230_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_Rsb_Rule_143_P286_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_Rsb_Rule_143_P286_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_Rsc_Rule_146_A1_P292_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_Rsc_Rule_146_A1_P292_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_Sbc_Rule_152_A1_P304_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_Sbc_Rule_152_A1_P304_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_SubRule_213_A1_P422_NotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_SubRule_213_A1_P422_NotRdIsPcAndS()
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_SubRule_213_A1_P422_instance_)
  {}
};

class Binary3RegisterOpTester_Asr_Rule_15_A1_P42_RegsNotPc
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTester_Asr_Rule_15_A1_P42_RegsNotPc()
    : Binary3RegisterOpTesterRegsNotPc(
      state_.Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_)
  {}
};

class Binary3RegisterOpTester_Lsl_Rule_89_A1_P180_RegsNotPc
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTester_Lsl_Rule_89_A1_P180_RegsNotPc()
    : Binary3RegisterOpTesterRegsNotPc(
      state_.Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_)
  {}
};

class Binary3RegisterOpTester_Lsr_Rule_91_A1_P184_RegsNotPc
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTester_Lsr_Rule_91_A1_P184_RegsNotPc()
    : Binary3RegisterOpTesterRegsNotPc(
      state_.Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_)
  {}
};

class Binary3RegisterOpTester_Ror_Rule_140_A1_P280_RegsNotPc
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTester_Ror_Rule_140_A1_P280_RegsNotPc()
    : Binary3RegisterOpTesterRegsNotPc(
      state_.Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_)
  {}
};

class Binary3RegisterOpAltATester_Mul_Rule_105_A1_P212_RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATester_Mul_Rule_105_A1_P212_RegsNotPc()
    : Binary3RegisterOpAltATesterRegsNotPc(
      state_.Binary3RegisterOpAltA_Mul_Rule_105_A1_P212_instance_)
  {}
};

class Binary3RegisterShiftedTestTester_Cmn_Rule_34_A1_P78_RegsNotPc
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_Cmn_Rule_34_A1_P78_RegsNotPc()
    : Binary3RegisterShiftedTestTesterRegsNotPc(
      state_.Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_)
  {}
};

class Binary3RegisterShiftedTestTester_Cmp_Rule_37_A1_P84_RegsNotPc
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_Cmp_Rule_37_A1_P84_RegsNotPc()
    : Binary3RegisterShiftedTestTesterRegsNotPc(
      state_.Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_)
  {}
};

class Binary3RegisterShiftedTestTester_Teq_Rule_229_A1_P452_RegsNotPc
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_Teq_Rule_229_A1_P452_RegsNotPc()
    : Binary3RegisterShiftedTestTesterRegsNotPc(
      state_.Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_)
  {}
};

class Binary3RegisterShiftedTestTester_Tst_Rule_232_A1_P458_RegsNotPc
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_Tst_Rule_232_A1_P458_RegsNotPc()
    : Binary3RegisterShiftedTestTesterRegsNotPc(
      state_.Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_)
  {}
};

class Binary4RegisterDualOpTester_Mla_Rule_94_A1_P190_RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTester_Mla_Rule_94_A1_P190_RegsNotPc()
    : Binary4RegisterDualOpTesterRegsNotPc(
      state_.Binary4RegisterDualOp_Mla_Rule_94_A1_P190_instance_)
  {}
};

class Binary4RegisterDualOpTester_Mls_Rule_95_A1_P192_RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTester_Mls_Rule_95_A1_P192_RegsNotPc()
    : Binary4RegisterDualOpTesterRegsNotPc(
      state_.Binary4RegisterDualOp_Mls_Rule_95_A1_P192_instance_)
  {}
};

class Binary4RegisterDualResultTester_Smlal_Rule_168_A1_P334_RegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTester_Smlal_Rule_168_A1_P334_RegsNotPc()
    : Binary4RegisterDualResultTesterRegsNotPc(
      state_.Binary4RegisterDualResult_Smlal_Rule_168_A1_P334_instance_)
  {}
};

class Binary4RegisterDualResultTester_Smull_Rule_179_A1_P356_RegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTester_Smull_Rule_179_A1_P356_RegsNotPc()
    : Binary4RegisterDualResultTesterRegsNotPc(
      state_.Binary4RegisterDualResult_Smull_Rule_179_A1_P356_instance_)
  {}
};

class Binary4RegisterDualResultTester_Umaal_Rule_244_A1_P482_RegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTester_Umaal_Rule_244_A1_P482_RegsNotPc()
    : Binary4RegisterDualResultTesterRegsNotPc(
      state_.Binary4RegisterDualResult_Umaal_Rule_244_A1_P482_instance_)
  {}
};

class Binary4RegisterDualResultTester_Umlal_Rule_245_A1_P484_RegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTester_Umlal_Rule_245_A1_P484_RegsNotPc()
    : Binary4RegisterDualResultTesterRegsNotPc(
      state_.Binary4RegisterDualResult_Umlal_Rule_245_A1_P484_instance_)
  {}
};

class Binary4RegisterDualResultTester_Umull_Rule_246_A1_P486_RegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTester_Umull_Rule_246_A1_P486_RegsNotPc()
    : Binary4RegisterDualResultTesterRegsNotPc(
      state_.Binary4RegisterDualResult_Umull_Rule_246_A1_P486_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Adc_Rule_3_A1_P18_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_Adc_Rule_3_A1_P18_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Add_Rule_7_A1_P26_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_Add_Rule_7_A1_P26_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_And_Rule_13_A1_P38_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_And_Rule_13_A1_P38_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Bic_Rule_21_A1_P54_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_Bic_Rule_21_A1_P54_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Eor_Rule_46_A1_P98_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_Eor_Rule_46_A1_P98_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Orr_Rule_115_A1_P212_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_Orr_Rule_115_A1_P212_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Rsb_Rule_144_A1_P288_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_Rsb_Rule_144_A1_P288_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Rsc_Rule_147_A1_P294_
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTester_Rsc_Rule_147_A1_P294_()
    : Binary4RegisterShiftedOpTester(
      state_.Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Sbc_Rule_153_A1_P306_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_Sbc_Rule_153_A1_P306_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_Sub_Rule_214_A1_P424_RegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_Sub_Rule_214_A1_P424_RegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_)
  {}
};

class BinaryRegisterImmediateTestTester_Cmn_Rule_32_A1_P74_
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTester_Cmn_Rule_32_A1_P74_()
    : BinaryRegisterImmediateTestTester(
      state_.BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74_instance_)
  {}
};

class BinaryRegisterImmediateTestTester_Cmp_Rule_35_A1_P80_
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTester_Cmp_Rule_35_A1_P80_()
    : BinaryRegisterImmediateTestTester(
      state_.BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80_instance_)
  {}
};

class BinaryRegisterImmediateTestTester_Teq_Rule_227_A1_P448_
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTester_Teq_Rule_227_A1_P448_()
    : BinaryRegisterImmediateTestTester(
      state_.BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448_instance_)
  {}
};

class CondNopTester_Dbg_Rule_40_A1_P88_
    : public CondNopTester {
 public:
  CondNopTester_Dbg_Rule_40_A1_P88_()
    : CondNopTester(
      state_.CondNop_Dbg_Rule_40_A1_P88_instance_)
  {}
};

class CondNopTester_Nop_Rule_110_A1_P222_
    : public CondNopTester {
 public:
  CondNopTester_Nop_Rule_110_A1_P222_()
    : CondNopTester(
      state_.CondNop_Nop_Rule_110_A1_P222_instance_)
  {}
};

class CondNopTester_Yield_Rule_413_A1_P812_
    : public CondNopTester {
 public:
  CondNopTester_Yield_Rule_413_A1_P812_()
    : CondNopTester(
      state_.CondNop_Yield_Rule_413_A1_P812_instance_)
  {}
};

class ForbiddenCondNopTester_Msr_Rule_B6_1_6_A1_PB6_12_
    : public UnsafeCondNopTester {
 public:
  ForbiddenCondNopTester_Msr_Rule_B6_1_6_A1_PB6_12_()
    : UnsafeCondNopTester(
      state_.ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

class ForbiddenCondNopTester_Sev_Rule_158_A1_P316_
    : public UnsafeCondNopTester {
 public:
  ForbiddenCondNopTester_Sev_Rule_158_A1_P316_()
    : UnsafeCondNopTester(
      state_.ForbiddenCondNop_Sev_Rule_158_A1_P316_instance_)
  {}
};

class ForbiddenCondNopTester_Wfe_Rule_411_A1_P808_
    : public UnsafeCondNopTester {
 public:
  ForbiddenCondNopTester_Wfe_Rule_411_A1_P808_()
    : UnsafeCondNopTester(
      state_.ForbiddenCondNop_Wfe_Rule_411_A1_P808_instance_)
  {}
};

class ForbiddenCondNopTester_Wfi_Rule_412_A1_P810_
    : public UnsafeCondNopTester {
 public:
  ForbiddenCondNopTester_Wfi_Rule_412_A1_P810_()
    : UnsafeCondNopTester(
      state_.ForbiddenCondNop_Wfi_Rule_412_A1_P810_instance_)
  {}
};

class Load2RegisterImm12OpTester_Ldr_Rule_58_A1_P120_NotRnIsPc
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  Load2RegisterImm12OpTester_Ldr_Rule_58_A1_P120_NotRnIsPc()
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(
      state_.Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_)
  {}
};

class Load2RegisterImm12OpTester_Ldr_Rule_59_A1_P122_
    : public LoadStore2RegisterImm12OpTester {
 public:
  Load2RegisterImm12OpTester_Ldr_Rule_59_A1_P122_()
    : LoadStore2RegisterImm12OpTester(
      state_.Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_)
  {}
};

class Load2RegisterImm12OpTester_Ldrb_Rule_62_A1_P128_NotRnIsPc
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  Load2RegisterImm12OpTester_Ldrb_Rule_62_A1_P128_NotRnIsPc()
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(
      state_.Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_)
  {}
};

class Load2RegisterImm12OpTester_Ldrb_Rule_63_A1_P130_
    : public LoadStore2RegisterImm12OpTester {
 public:
  Load2RegisterImm12OpTester_Ldrb_Rule_63_A1_P130_()
    : LoadStore2RegisterImm12OpTester(
      state_.Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_)
  {}
};

class Load2RegisterImm8DoubleOpTester_Ldrd_Rule_66_A1_P136_NotRnIsPc
    : public LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc {
 public:
  Load2RegisterImm8DoubleOpTester_Ldrd_Rule_66_A1_P136_NotRnIsPc()
    : LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136_instance_)
  {}
};

class Load2RegisterImm8DoubleOpTester_Ldrd_Rule_67_A1_P138_
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  Load2RegisterImm8DoubleOpTester_Ldrd_Rule_67_A1_P138_()
    : LoadStore2RegisterImm8DoubleOpTester(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138_instance_)
  {}
};

class Load2RegisterImm8OpTester_Ldrh_Rule_74_A1_P152_NotRnIsPc
    : public LoadStore2RegisterImm8OpTesterNotRnIsPc {
 public:
  Load2RegisterImm8OpTester_Ldrh_Rule_74_A1_P152_NotRnIsPc()
    : LoadStore2RegisterImm8OpTesterNotRnIsPc(
      state_.Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152_instance_)
  {}
};

class Load2RegisterImm8OpTester_Ldrh_Rule_75_A1_P154_
    : public LoadStore2RegisterImm8OpTester {
 public:
  Load2RegisterImm8OpTester_Ldrh_Rule_75_A1_P154_()
    : LoadStore2RegisterImm8OpTester(
      state_.Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154_instance_)
  {}
};

class Load2RegisterImm8OpTester_Ldrsb_Rule_78_A1_P160_NotRnIsPc
    : public LoadStore2RegisterImm8OpTesterNotRnIsPc {
 public:
  Load2RegisterImm8OpTester_Ldrsb_Rule_78_A1_P160_NotRnIsPc()
    : LoadStore2RegisterImm8OpTesterNotRnIsPc(
      state_.Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160_instance_)
  {}
};

class Load2RegisterImm8OpTester_Ldrsh_Rule_82_A1_P168_NotRnIsPc
    : public LoadStore2RegisterImm8OpTesterNotRnIsPc {
 public:
  Load2RegisterImm8OpTester_Ldrsh_Rule_82_A1_P168_NotRnIsPc()
    : LoadStore2RegisterImm8OpTesterNotRnIsPc(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168_instance_)
  {}
};

class Load2RegisterImm8OpTester_Ldrsh_Rule_83_A1_P170_
    : public LoadStore2RegisterImm8OpTester {
 public:
  Load2RegisterImm8OpTester_Ldrsh_Rule_83_A1_P170_()
    : LoadStore2RegisterImm8OpTester(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170_instance_)
  {}
};

class Load2RegisterImm8OpTester_ldrsb_Rule_79_A1_162_
    : public LoadStore2RegisterImm8OpTester {
 public:
  Load2RegisterImm8OpTester_ldrsb_Rule_79_A1_162_()
    : LoadStore2RegisterImm8OpTester(
      state_.Load2RegisterImm8Op_ldrsb_Rule_79_A1_162_instance_)
  {}
};

class Load3RegisterDoubleOpTester_Ldrd_Rule_68_A1_P140_
    : public LoadStore3RegisterDoubleOpTester {
 public:
  Load3RegisterDoubleOpTester_Ldrd_Rule_68_A1_P140_()
    : LoadStore3RegisterDoubleOpTester(
      state_.Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140_instance_)
  {}
};

class Load3RegisterImm5OpTester_Ldr_Rule_60_A1_P124_
    : public LoadStore3RegisterImm5OpTester {
 public:
  Load3RegisterImm5OpTester_Ldr_Rule_60_A1_P124_()
    : LoadStore3RegisterImm5OpTester(
      state_.Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_)
  {}
};

class Load3RegisterImm5OpTester_Ldrb_Rule_64_A1_P132_
    : public LoadStore3RegisterImm5OpTester {
 public:
  Load3RegisterImm5OpTester_Ldrb_Rule_64_A1_P132_()
    : LoadStore3RegisterImm5OpTester(
      state_.Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_)
  {}
};

class Load3RegisterOpTester_Ldrh_Rule_76_A1_P156_
    : public LoadStore3RegisterOpTester {
 public:
  Load3RegisterOpTester_Ldrh_Rule_76_A1_P156_()
    : LoadStore3RegisterOpTester(
      state_.Load3RegisterOp_Ldrh_Rule_76_A1_P156_instance_)
  {}
};

class Load3RegisterOpTester_Ldrsb_Rule_80_A1_P164_
    : public LoadStore3RegisterOpTester {
 public:
  Load3RegisterOpTester_Ldrsb_Rule_80_A1_P164_()
    : LoadStore3RegisterOpTester(
      state_.Load3RegisterOp_Ldrsb_Rule_80_A1_P164_instance_)
  {}
};

class Load3RegisterOpTester_Ldrsh_Rule_84_A1_P172_
    : public LoadStore3RegisterOpTester {
 public:
  Load3RegisterOpTester_Ldrsh_Rule_84_A1_P172_()
    : LoadStore3RegisterOpTester(
      state_.Load3RegisterOp_Ldrsh_Rule_84_A1_P172_instance_)
  {}
};

class MaskedBinary2RegisterImmediateOpTester_Bic_Rule_19_A1_P50_NotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  MaskedBinary2RegisterImmediateOpTester_Bic_Rule_19_A1_P50_NotRdIsPcAndS()
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_)
  {}
};

class MaskedBinaryRegisterImmediateTestTester_Tst_Rule_230_A1_P454_
    : public BinaryRegisterImmediateTestTester {
 public:
  MaskedBinaryRegisterImmediateTestTester_Tst_Rule_230_A1_P454_()
    : BinaryRegisterImmediateTestTester(
      state_.MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_)
  {}
};

class Store2RegisterImm12OpTester_Str_Rule_194_A1_P384_
    : public LoadStore2RegisterImm12OpTester {
 public:
  Store2RegisterImm12OpTester_Str_Rule_194_A1_P384_()
    : LoadStore2RegisterImm12OpTester(
      state_.Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_)
  {}
};

class Store2RegisterImm12OpTester_Strb_Rule_197_A1_P390_
    : public LoadStore2RegisterImm12OpTester {
 public:
  Store2RegisterImm12OpTester_Strb_Rule_197_A1_P390_()
    : LoadStore2RegisterImm12OpTester(
      state_.Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_)
  {}
};

class Store2RegisterImm8DoubleOpTester_Strd_Rule_200_A1_P396_
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  Store2RegisterImm8DoubleOpTester_Strd_Rule_200_A1_P396_()
    : LoadStore2RegisterImm8DoubleOpTester(
      state_.Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396_instance_)
  {}
};

class Store2RegisterImm8OpTester_Strh_Rule_207_A1_P410_
    : public LoadStore2RegisterImm8OpTester {
 public:
  Store2RegisterImm8OpTester_Strh_Rule_207_A1_P410_()
    : LoadStore2RegisterImm8OpTester(
      state_.Store2RegisterImm8Op_Strh_Rule_207_A1_P410_instance_)
  {}
};

class Store3RegisterDoubleOpTester_Strd_Rule_201_A1_P398_
    : public LoadStore3RegisterDoubleOpTester {
 public:
  Store3RegisterDoubleOpTester_Strd_Rule_201_A1_P398_()
    : LoadStore3RegisterDoubleOpTester(
      state_.Store3RegisterDoubleOp_Strd_Rule_201_A1_P398_instance_)
  {}
};

class Store3RegisterImm5OpTester_Str_Rule_195_A1_P386_
    : public LoadStore3RegisterImm5OpTester {
 public:
  Store3RegisterImm5OpTester_Str_Rule_195_A1_P386_()
    : LoadStore3RegisterImm5OpTester(
      state_.Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_)
  {}
};

class Store3RegisterImm5OpTester_Strb_Rule_198_A1_P392_
    : public LoadStore3RegisterImm5OpTester {
 public:
  Store3RegisterImm5OpTester_Strb_Rule_198_A1_P392_()
    : LoadStore3RegisterImm5OpTester(
      state_.Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_)
  {}
};

class Store3RegisterOpTester_Strh_Rule_208_A1_P412_
    : public LoadStore3RegisterOpTester {
 public:
  Store3RegisterOpTester_Strh_Rule_208_A1_P412_()
    : LoadStore3RegisterOpTester(
      state_.Store3RegisterOp_Strh_Rule_208_A1_P412_instance_)
  {}
};

class Unary1RegisterBitRangeTester_Bfc_17_A1_P46_
    : public Unary1RegisterBitRangeTester {
 public:
  Unary1RegisterBitRangeTester_Bfc_17_A1_P46_()
    : Unary1RegisterBitRangeTester(
      state_.Unary1RegisterBitRange_Bfc_17_A1_P46_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_Adr_Rule_10_A1_P32_
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTester_Adr_Rule_10_A1_P32_()
    : Unary1RegisterImmediateOpTester(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_Adr_Rule_10_A2_P32_
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTester_Adr_Rule_10_A2_P32_()
    : Unary1RegisterImmediateOpTester(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_Mov_Rule_96_A1_P194_NotRdIsPcAndS
    : public Unary1RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTester_Mov_Rule_96_A1_P194_NotRdIsPcAndS()
    : Unary1RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_Mov_Rule_96_A2_P_194_RegsNotPc
    : public Unary1RegisterImmediateOpTesterRegsNotPc {
 public:
  Unary1RegisterImmediateOpTester_Mov_Rule_96_A2_P_194_RegsNotPc()
    : Unary1RegisterImmediateOpTesterRegsNotPc(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A2_P_194_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_Mvn_Rule_106_A1_P214_NotRdIsPcAndS
    : public Unary1RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTester_Mvn_Rule_106_A1_P214_NotRdIsPcAndS()
    : Unary1RegisterImmediateOpTesterNotRdIsPcAndS(
      state_.Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_Asr_Rule_14_A1_P40_
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTester_Asr_Rule_14_A1_P40_()
    : Unary2RegisterImmedShiftedOpTester(
      state_.Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_Lsl_Rule_88_A1_P178_Imm5NotZero
    : public Unary2RegisterImmedShiftedOpTesterImm5NotZero {
 public:
  Unary2RegisterImmedShiftedOpTester_Lsl_Rule_88_A1_P178_Imm5NotZero()
    : Unary2RegisterImmedShiftedOpTesterImm5NotZero(
      state_.Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_Lsr_Rule_90_A1_P182_
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTester_Lsr_Rule_90_A1_P182_()
    : Unary2RegisterImmedShiftedOpTester(
      state_.Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_Mvn_Rule_107_A1_P216_NotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_Mvn_Rule_107_A1_P216_NotRdIsPcAndS()
    : Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_Ror_Rule_139_A1_P278_Imm5NotZero
    : public Unary2RegisterImmedShiftedOpTesterImm5NotZero {
 public:
  Unary2RegisterImmedShiftedOpTester_Ror_Rule_139_A1_P278_Imm5NotZero()
    : Unary2RegisterImmedShiftedOpTesterImm5NotZero(
      state_.Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_)
  {}
};

class Unary2RegisterOpTester_Mov_Rule_97_A1_P196_NotRdIsPcAndS
    : public Unary2RegisterOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterOpTester_Mov_Rule_97_A1_P196_NotRdIsPcAndS()
    : Unary2RegisterOpTesterNotRdIsPcAndS(
      state_.Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_)
  {}
};

class Unary2RegisterOpTester_Rrx_Rule_141_A1_P282_
    : public Unary2RegisterOpTester {
 public:
  Unary2RegisterOpTester_Rrx_Rule_141_A1_P282_()
    : Unary2RegisterOpTester(
      state_.Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_)
  {}
};

class Unary3RegisterShiftedOpTester_Mvn_Rule_108_A1_P218_RegsNotPc
    : public Unary3RegisterShiftedOpTesterRegsNotPc {
 public:
  Unary3RegisterShiftedOpTester_Mvn_Rule_108_A1_P218_RegsNotPc()
    : Unary3RegisterShiftedOpTesterRegsNotPc(
      state_.Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following test each pattern specified in parse decoder tables.

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Cmn_Rule_33_A1_P76__cccc00010111nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_Cmn_Rule_33_A1_P76_ baseline_tester;
  NamedDontCareInst_Cmn_Rule_33_A1_P76 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Cmp_Rule_36_A1_P82__cccc00010101nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_Cmp_Rule_36_A1_P82_ baseline_tester;
  NamedDontCareInst_Cmp_Rule_36_A1_P82 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Teq_Rule_228_A1_P450__cccc00010011nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_Teq_Rule_228_A1_P450_ baseline_tester;
  NamedDontCareInst_Teq_Rule_228_A1_P450 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Tst_Rule_231_A1_P456__cccc00010001nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_Tst_Rule_231_A1_P456_ baseline_tester;
  NamedDontCareInst_Tst_Rule_231_A1_P456 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000iiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Adc_Rule_6_A1_P14_NotRdIsPcAndS_cccc0010101unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Adc_Rule_6_A1_P14_NotRdIsPcAndS tester;
  tester.Test("cccc0010101unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS_cccc001010011111ddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS tester;
  tester.Test("cccc001010011111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS_cccc0010100unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS tester;
  tester.Test("cccc0010100unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_And_Rule_11_A1_P34_NotRdIsPcAndS_cccc0010000unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_And_Rule_11_A1_P34_NotRdIsPcAndS tester;
  tester.Test("cccc0010000unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Eor_Rule_44_A1_P94_NotRdIsPcAndS_cccc0010001unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Eor_Rule_44_A1_P94_NotRdIsPcAndS tester;
  tester.Test("cccc0010001unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Orr_Rule_113_A1_P228_NotRdIsPcAndS_cccc0011100unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Orr_Rule_113_A1_P228_NotRdIsPcAndS tester;
  tester.Test("cccc0011100unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Rsb_Rule_142_A1_P284_NotRdIsPcAndS_cccc0010011unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Rsb_Rule_142_A1_P284_NotRdIsPcAndS tester;
  tester.Test("cccc0010011unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Rsc_Rule_145_A1_P290_NotRdIsPcAndS_cccc0010111unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Rsc_Rule_145_A1_P290_NotRdIsPcAndS tester;
  tester.Test("cccc0010111unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Sbc_Rule_151_A1_P302_NotRdIsPcAndS_cccc0010110unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Sbc_Rule_151_A1_P302_NotRdIsPcAndS tester;
  tester.Test("cccc0010110unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS_cccc001001011111ddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS tester;
  tester.Test("cccc001001011111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS_cccc0010010unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS tester;
  tester.Test("cccc0010010unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Adc_Rule_2_A1_P16_NotRdIsPcAndS_cccc0000101unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_Adc_Rule_2_A1_P16_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Adc_Rule_2_A1_P16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Add_Rule_6_A1_P24_NotRdIsPcAndS_cccc0000100unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_Add_Rule_6_A1_P24_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Add_Rule_6_A1_P24 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_And_Rule_7_A1_P36_NotRdIsPcAndS_cccc0000000unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_And_Rule_7_A1_P36_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_And_Rule_7_A1_P36 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Bic_Rule_20_A1_P52_NotRdIsPcAndS_cccc0001110unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_Bic_Rule_20_A1_P52_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Bic_Rule_20_A1_P52 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Orr_Rule_114_A1_P230_NotRdIsPcAndS_cccc0001100unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_Orr_Rule_114_A1_P230_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Orr_Rule_114_A1_P230 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Rsb_Rule_143_P286_NotRdIsPcAndS_cccc0000011unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_Rsb_Rule_143_P286_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Rsb_Rule_143_P286 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Rsc_Rule_146_A1_P292_NotRdIsPcAndS_cccc0000111unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_Rsc_Rule_146_A1_P292_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Rsc_Rule_146_A1_P292 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Sbc_Rule_152_A1_P304_NotRdIsPcAndS_cccc0000110unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_Sbc_Rule_152_A1_P304_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Sbc_Rule_152_A1_P304 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_SubRule_213_A1_P422_NotRdIsPcAndS_cccc0000010unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_SubRule_213_A1_P422_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_SubRule_213_A1_P422 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Eor_Rule_45_A1_P96_NotRdIsPcAndS_cccc0000001unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_Eor_Rule_45_A1_P96_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15CondsDontCare_Eor_Rule_45_A1_P96 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Asr_Rule_15_A1_P42_RegsNotPc_cccc0001101u0000ddddmmmm0101nnnn_Test) {
  Binary3RegisterOpTester_Asr_Rule_15_A1_P42_RegsNotPc tester;
  tester.Test("cccc0001101u0000ddddmmmm0101nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Lsl_Rule_89_A1_P180_RegsNotPc_cccc0001101u0000ddddmmmm0001nnnn_Test) {
  Binary3RegisterOpTester_Lsl_Rule_89_A1_P180_RegsNotPc tester;
  tester.Test("cccc0001101u0000ddddmmmm0001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Lsr_Rule_91_A1_P184_RegsNotPc_cccc0001101u0000ddddmmmm0011nnnn_Test) {
  Binary3RegisterOpTester_Lsr_Rule_91_A1_P184_RegsNotPc tester;
  tester.Test("cccc0001101u0000ddddmmmm0011nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Ror_Rule_140_A1_P280_RegsNotPc_cccc0001101u0000ddddmmmm0111nnnn_Test) {
  Binary3RegisterOpTester_Ror_Rule_140_A1_P280_RegsNotPc tester;
  tester.Test("cccc0001101u0000ddddmmmm0111nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Mul_Rule_105_A1_P212_RegsNotPc_cccc0000000udddd0000mmmm1001nnnn_Test) {
  Binary3RegisterOpAltATester_Mul_Rule_105_A1_P212_RegsNotPc tester;
  tester.Test("cccc0000000udddd0000mmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Cmn_Rule_34_A1_P78_RegsNotPc_cccc00010111nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_Cmn_Rule_34_A1_P78_RegsNotPc tester;
  tester.Test("cccc00010111nnnn0000ssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Cmp_Rule_37_A1_P84_RegsNotPc_cccc00010101nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_Cmp_Rule_37_A1_P84_RegsNotPc tester;
  tester.Test("cccc00010101nnnn0000ssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Teq_Rule_229_A1_P452_RegsNotPc_cccc00010011nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_Teq_Rule_229_A1_P452_RegsNotPc tester;
  tester.Test("cccc00010011nnnn0000ssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Tst_Rule_232_A1_P458_RegsNotPc_cccc00010001nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_Tst_Rule_232_A1_P458_RegsNotPc tester;
  tester.Test("cccc00010001nnnn0000ssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Mla_Rule_94_A1_P190_RegsNotPc_cccc0000001uddddaaaammmm1001nnnn_Test) {
  Binary4RegisterDualOpTester_Mla_Rule_94_A1_P190_RegsNotPc tester;
  tester.Test("cccc0000001uddddaaaammmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Mls_Rule_95_A1_P192_RegsNotPc_cccc00000110ddddaaaammmm1001nnnn_Test) {
  Binary4RegisterDualOpTester_Mls_Rule_95_A1_P192_RegsNotPc tester;
  tester.Test("cccc00000110ddddaaaammmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Smlal_Rule_168_A1_P334_RegsNotPc_cccc0000111uhhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_Smlal_Rule_168_A1_P334_RegsNotPc tester;
  tester.Test("cccc0000111uhhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Smull_Rule_179_A1_P356_RegsNotPc_cccc0000110uhhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_Smull_Rule_179_A1_P356_RegsNotPc tester;
  tester.Test("cccc0000110uhhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Umaal_Rule_244_A1_P482_RegsNotPc_cccc00000100hhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_Umaal_Rule_244_A1_P482_RegsNotPc tester;
  tester.Test("cccc00000100hhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Umlal_Rule_245_A1_P484_RegsNotPc_cccc0000101uhhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_Umlal_Rule_245_A1_P484_RegsNotPc tester;
  tester.Test("cccc0000101uhhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Umull_Rule_246_A1_P486_RegsNotPc_cccc0000100uhhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_Umull_Rule_246_A1_P486_RegsNotPc tester;
  tester.Test("cccc0000100uhhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Adc_Rule_3_A1_P18_RegsNotPc_cccc0000101unnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Adc_Rule_3_A1_P18_RegsNotPc tester;
  tester.Test("cccc0000101unnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_And_Rule_13_A1_P38_RegsNotPc_cccc0000000unnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_And_Rule_13_A1_P38_RegsNotPc tester;
  tester.Test("cccc0000000unnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Bic_Rule_21_A1_P54_RegsNotPc_cccc0001110unnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Bic_Rule_21_A1_P54_RegsNotPc tester;
  tester.Test("cccc0001110unnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Eor_Rule_46_A1_P98_RegsNotPc_cccc0000001unnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Eor_Rule_46_A1_P98_RegsNotPc tester;
  tester.Test("cccc0000001unnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Orr_Rule_115_A1_P212_RegsNotPc_cccc0001100unnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Orr_Rule_115_A1_P212_RegsNotPc tester;
  tester.Test("cccc0001100unnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Rsc_Rule_147_A1_P294__cccc0000111unnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Rsc_Rule_147_A1_P294_ tester;
  tester.Test("cccc0000111unnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Sbc_Rule_153_A1_P306_RegsNotPc_cccc0000110unnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Sbc_Rule_153_A1_P306_RegsNotPc tester;
  tester.Test("cccc0000110unnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Sub_Rule_214_A1_P424_RegsNotPc_cccc0000010unnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Sub_Rule_214_A1_P424_RegsNotPc tester;
  tester.Test("cccc0000010unnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Add_Rule_7_A1_P26_RegsNotPc_cccc0000100snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Add_Rule_7_A1_P26_RegsNotPc baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Rsb_Rule_144_A1_P288_RegsNotPc_cccc0000011snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_Rsb_Rule_144_A1_P288_RegsNotPc baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Cmn_Rule_32_A1_P74__cccc00110111nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_Cmn_Rule_32_A1_P74_ baseline_tester;
  NamedDontCareInst_Cmn_Rule_32_A1_P74 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110111nnnn0000iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Cmp_Rule_35_A1_P80__cccc00110101nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_Cmp_Rule_35_A1_P80_ baseline_tester;
  NamedDontCareInst_Cmp_Rule_35_A1_P80 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110101nnnn0000iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Teq_Rule_227_A1_P448__cccc00110011nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_Teq_Rule_227_A1_P448_ baseline_tester;
  NamedDontCareInst_Teq_Rule_227_A1_P448 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110011nnnn0000iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       CondNopTester_Dbg_Rule_40_A1_P88__cccc001100100000111100001111iiii_Test) {
  CondNopTester_Dbg_Rule_40_A1_P88_ baseline_tester;
  NamedEffectiveNoOp_Dbg_Rule_40_A1_P88 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100100000111100001111iiii");
}

TEST_F(Arm32DecoderStateTests,
       CondNopTester_Nop_Rule_110_A1_P222__cccc0011001000001111000000000000_Test) {
  CondNopTester_Nop_Rule_110_A1_P222_ baseline_tester;
  NamedEffectiveNoOp_Nop_Rule_110_A1_P222 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000000");
}

TEST_F(Arm32DecoderStateTests,
       CondNopTester_Yield_Rule_413_A1_P812__cccc0011001000001111000000000001_Test) {
  CondNopTester_Yield_Rule_413_A1_P812_ baseline_tester;
  NamedEffectiveNoOp_Yield_Rule_413_A1_P812 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000001");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_Msr_Rule_B6_1_6_A1_PB6_12__cccc00110010ii011111iiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_Msr_Rule_B6_1_6_A1_PB6_12_ baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii011111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_Msr_Rule_B6_1_6_A1_PB6_12__cccc00110010ii1i1111iiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_Msr_Rule_B6_1_6_A1_PB6_12_ baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii1i1111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_Msr_Rule_B6_1_6_A1_PB6_12__cccc00110110iiii1111iiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_Msr_Rule_B6_1_6_A1_PB6_12_ baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110110iiii1111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_Sev_Rule_158_A1_P316__cccc0011001000001111000000000100_Test) {
  ForbiddenCondNopTester_Sev_Rule_158_A1_P316_ baseline_tester;
  NamedForbidden_Sev_Rule_158_A1_P316 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000100");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_Wfe_Rule_411_A1_P808__cccc0011001000001111000000000010_Test) {
  ForbiddenCondNopTester_Wfe_Rule_411_A1_P808_ baseline_tester;
  NamedForbidden_Wfe_Rule_411_A1_P808 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000010");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_Wfi_Rule_412_A1_P810__cccc0011001000001111000000000011_Test) {
  ForbiddenCondNopTester_Wfi_Rule_412_A1_P810_ baseline_tester;
  NamedForbidden_Wfi_Rule_412_A1_P810 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000011");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Ldr_Rule_58_A1_P120_NotRnIsPc_cccc010pd0w1nnnnttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_Ldr_Rule_58_A1_P120_NotRnIsPc baseline_tester;
  NamedLdrImmediate_Ldr_Rule_58_A1_P120 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pd0w1nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Ldr_Rule_59_A1_P122__cccc0101d0011111ttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_Ldr_Rule_59_A1_P122_ baseline_tester;
  NamedLdrImmediate_Ldr_Rule_59_A1_P122 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101d0011111ttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Ldrb_Rule_62_A1_P128_NotRnIsPc_cccc010pd1w1nnnnttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_Ldrb_Rule_62_A1_P128_NotRnIsPc baseline_tester;
  NamedLdrImmediate_Ldrb_Rule_62_A1_P128 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pd1w1nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Ldrb_Rule_63_A1_P130__cccc0101d1011111ttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_Ldrb_Rule_63_A1_P130_ baseline_tester;
  NamedLdrImmediate_Ldrb_Rule_63_A1_P130 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101d1011111ttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_Ldrd_Rule_66_A1_P136_NotRnIsPc_cccc000pd1w0nnnnttttiiii1101iiii_Test) {
  Load2RegisterImm8DoubleOpTester_Ldrd_Rule_66_A1_P136_NotRnIsPc baseline_tester;
  NamedLdrImmediateDouble_Ldrd_Rule_66_A1_P136 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd1w0nnnnttttiiii1101iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_Ldrd_Rule_67_A1_P138__cccc0001d1001111ttttiiii1101iiii_Test) {
  Load2RegisterImm8DoubleOpTester_Ldrd_Rule_67_A1_P138_ baseline_tester;
  NamedLdrImmediateDouble_Ldrd_Rule_67_A1_P138 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001d1001111ttttiiii1101iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Ldrh_Rule_74_A1_P152_NotRnIsPc_cccc000pd1w1nnnnttttiiii1011iiii_Test) {
  Load2RegisterImm8OpTester_Ldrh_Rule_74_A1_P152_NotRnIsPc baseline_tester;
  NamedLdrImmediate_Ldrh_Rule_74_A1_P152 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd1w1nnnnttttiiii1011iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Ldrh_Rule_75_A1_P154__cccc0001d1011111ttttiiii1011iiii_Test) {
  Load2RegisterImm8OpTester_Ldrh_Rule_75_A1_P154_ baseline_tester;
  NamedLdrImmediate_Ldrh_Rule_75_A1_P154 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001d1011111ttttiiii1011iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Ldrsb_Rule_78_A1_P160_NotRnIsPc_cccc000pd1w1nnnnttttiiii1101iiii_Test) {
  Load2RegisterImm8OpTester_Ldrsb_Rule_78_A1_P160_NotRnIsPc baseline_tester;
  NamedLdrImmediate_Ldrsb_Rule_78_A1_P160 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd1w1nnnnttttiiii1101iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Ldrsh_Rule_82_A1_P168_NotRnIsPc_cccc000pd1w1nnnnttttiiii1111iiii_Test) {
  Load2RegisterImm8OpTester_Ldrsh_Rule_82_A1_P168_NotRnIsPc baseline_tester;
  NamedLdrImmediate_Ldrsh_Rule_82_A1_P168 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd1w1nnnnttttiiii1111iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Ldrsh_Rule_83_A1_P170__cccc0001d1011111ttttiiii1111iiii_Test) {
  Load2RegisterImm8OpTester_Ldrsh_Rule_83_A1_P170_ baseline_tester;
  NamedLdrImmediate_Ldrsh_Rule_83_A1_P170 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001d1011111ttttiiii1111iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_ldrsb_Rule_79_A1_162__cccc0001d1011111ttttiiii1101iiii_Test) {
  Load2RegisterImm8OpTester_ldrsb_Rule_79_A1_162_ baseline_tester;
  NamedLdrImmediate_ldrsb_Rule_79_A1_162 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001d1011111ttttiiii1101iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterDoubleOpTester_Ldrd_Rule_68_A1_P140__cccc000pd0w0nnnntttt00001101mmmm_Test) {
  Load3RegisterDoubleOpTester_Ldrd_Rule_68_A1_P140_ baseline_tester;
  NamedLdrRegisterDouble_Ldrd_Rule_68_A1_P140 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd0w0nnnntttt00001101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_Ldr_Rule_60_A1_P124__cccc011pd0w1nnnnttttiiiiitt0mmmm_Test) {
  Load3RegisterImm5OpTester_Ldr_Rule_60_A1_P124_ baseline_tester;
  NamedLdrRegister_Ldr_Rule_60_A1_P124 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pd0w1nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_Ldrb_Rule_64_A1_P132__cccc011pd1w1nnnnttttiiiiitt0mmmm_Test) {
  Load3RegisterImm5OpTester_Ldrb_Rule_64_A1_P132_ baseline_tester;
  NamedLdrRegister_Ldrb_Rule_64_A1_P132 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pd1w1nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Ldrh_Rule_76_A1_P156__cccc000pd0w1nnnntttt00001011mmmm_Test) {
  Load3RegisterOpTester_Ldrh_Rule_76_A1_P156_ baseline_tester;
  NamedLdrRegister_Ldrh_Rule_76_A1_P156 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd0w1nnnntttt00001011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Ldrsb_Rule_80_A1_P164__cccc000pd0w1nnnntttt00001101mmmm_Test) {
  Load3RegisterOpTester_Ldrsb_Rule_80_A1_P164_ baseline_tester;
  NamedLdrRegister_Ldrsb_Rule_80_A1_P164 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd0w1nnnntttt00001101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Ldrsh_Rule_84_A1_P172__cccc000pd0w1nnnntttt00001111mmmm_Test) {
  Load3RegisterOpTester_Ldrsh_Rule_84_A1_P172_ baseline_tester;
  NamedLdrRegister_Ldrsh_Rule_84_A1_P172 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd0w1nnnntttt00001111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       MaskedBinary2RegisterImmediateOpTester_Bic_Rule_19_A1_P50_NotRdIsPcAndS_cccc0011110unnnnddddiiiiiiiiiiii_Test) {
  MaskedBinary2RegisterImmediateOpTester_Bic_Rule_19_A1_P50_NotRdIsPcAndS baseline_tester;
  NamedMaskAddress_Bic_Rule_19_A1_P50 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011110unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       MaskedBinaryRegisterImmediateTestTester_Tst_Rule_230_A1_P454__cccc00110001nnnn0000iiiiiiiiiiii_Test) {
  MaskedBinaryRegisterImmediateTestTester_Tst_Rule_230_A1_P454_ baseline_tester;
  NamedTestIfAddressMasked_Tst_Rule_230_A1_P454 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110001nnnn0000iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Str_Rule_194_A1_P384__cccc010pd0w0nnnnttttiiiiiiiiiiii_Test) {
  Store2RegisterImm12OpTester_Str_Rule_194_A1_P384_ baseline_tester;
  NamedStrImmediate_Str_Rule_194_A1_P384 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pd0w0nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Strb_Rule_197_A1_P390__cccc010pd1w0nnnnttttiiiiiiiiiiii_Test) {
  Store2RegisterImm12OpTester_Strb_Rule_197_A1_P390_ baseline_tester;
  NamedStrImmediate_Strb_Rule_197_A1_P390 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pd1w0nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8DoubleOpTester_Strd_Rule_200_A1_P396__cccc000pd1w0nnnnttttiiii1111iiii_Test) {
  Store2RegisterImm8DoubleOpTester_Strd_Rule_200_A1_P396_ baseline_tester;
  NamedStrImmediateDouble_Strd_Rule_200_A1_P396 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd1w0nnnnttttiiii1111iiii");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8OpTester_Strh_Rule_207_A1_P410__cccc000pd1w0nnnnttttiiii1011iiii_Test) {
  Store2RegisterImm8OpTester_Strh_Rule_207_A1_P410_ baseline_tester;
  NamedStrImmediate_Strh_Rule_207_A1_P410 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd1w0nnnnttttiiii1011iiii");
}

TEST_F(Arm32DecoderStateTests,
       Store3RegisterDoubleOpTester_Strd_Rule_201_A1_P398__cccc000pd0w0nnnntttt00001111mmmm_Test) {
  Store3RegisterDoubleOpTester_Strd_Rule_201_A1_P398_ baseline_tester;
  NamedStrRegisterDouble_Strd_Rule_201_A1_P398 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd0w0nnnntttt00001111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_Str_Rule_195_A1_P386__cccc011pd0w0nnnnttttiiiiitt0mmmm_Test) {
  Store3RegisterImm5OpTester_Str_Rule_195_A1_P386_ baseline_tester;
  NamedStrRegister_Str_Rule_195_A1_P386 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pd0w0nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_Strb_Rule_198_A1_P392__cccc011pd1w0nnnnttttiiiiitt0mmmm_Test) {
  Store3RegisterImm5OpTester_Strb_Rule_198_A1_P392_ baseline_tester;
  NamedStrRegister_Strb_Rule_198_A1_P392 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pd1w0nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Store3RegisterOpTester_Strh_Rule_208_A1_P412__cccc000pd0w0nnnntttt00001011mmmm_Test) {
  Store3RegisterOpTester_Strh_Rule_208_A1_P412_ baseline_tester;
  NamedStrRegister_Strh_Rule_208_A1_P412 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pd0w0nnnntttt00001011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Mov_Rule_96_A2_P_194_RegsNotPc_cccc00110000iiiiddddIIIIIIIIIIII_Test) {
  Unary1RegisterImmediateOpTester_Mov_Rule_96_A2_P_194_RegsNotPc baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A2_P_194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110000iiiiddddIIIIIIIIIIII");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Adr_Rule_10_A1_P32__cccc001010001111ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_Adr_Rule_10_A1_P32_ tester;
  tester.Test("cccc001010001111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Adr_Rule_10_A2_P32__cccc001001001111ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_Adr_Rule_10_A2_P32_ tester;
  tester.Test("cccc001001001111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Mov_Rule_96_A1_P194_NotRdIsPcAndS_cccc0011101u0000ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_Mov_Rule_96_A1_P194_NotRdIsPcAndS tester;
  tester.Test("cccc0011101u0000ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Mvn_Rule_106_A1_P214_NotRdIsPcAndS_cccc0011111u0000ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_Mvn_Rule_106_A1_P214_NotRdIsPcAndS tester;
  tester.Test("cccc0011111u0000ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Asr_Rule_14_A1_P40__cccc0001101u0000ddddiiiii100mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_Asr_Rule_14_A1_P40_ baseline_tester;
  NamedDefs12To15_Asr_Rule_14_A1_P40 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii100mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Lsl_Rule_88_A1_P178_Imm5NotZero_cccc0001101u0000ddddiiiii000mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_Lsl_Rule_88_A1_P178_Imm5NotZero baseline_tester;
  NamedDefs12To15_Lsl_Rule_88_A1_P178 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii000mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Lsr_Rule_90_A1_P182__cccc0001101u0000ddddiiiii010mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_Lsr_Rule_90_A1_P182_ baseline_tester;
  NamedDefs12To15_Lsr_Rule_90_A1_P182 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii010mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Mvn_Rule_107_A1_P216_NotRdIsPcAndS_cccc0001111u0000ddddiiiiitt0mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_Mvn_Rule_107_A1_P216_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Mvn_Rule_107_A1_P216 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111u0000ddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Ror_Rule_139_A1_P278_Imm5NotZero_cccc0001101u0000ddddiiiii110mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_Ror_Rule_139_A1_P278_Imm5NotZero baseline_tester;
  NamedDefs12To15_Ror_Rule_139_A1_P278 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii110mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Mov_Rule_97_A1_P196_NotRdIsPcAndS_cccc0001101udddd000000000000mmmm_Test) {
  Unary2RegisterOpTester_Mov_Rule_97_A1_P196_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Mov_Rule_97_A1_P196 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101udddd000000000000mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Rrx_Rule_141_A1_P282__cccc0001101udddd000000000110mmmm_Test) {
  Unary2RegisterOpTester_Rrx_Rule_141_A1_P282_ baseline_tester;
  NamedDefs12To15_Rrx_Rule_141_A1_P282 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101udddd000000000110mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary3RegisterShiftedOpTester_Mvn_Rule_108_A1_P218_RegsNotPc_cccc0001111u0000ddddssss0tt1mmmm_Test) {
  Unary3RegisterShiftedOpTester_Mvn_Rule_108_A1_P218_RegsNotPc tester;
  tester.Test("cccc0001111u0000ddddssss0tt1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
