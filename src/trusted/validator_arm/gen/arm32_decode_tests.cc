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

class Binary3RegisterOpAltATester_Smulwx_Rule_180_A1_P358_RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATester_Smulwx_Rule_180_A1_P358_RegsNotPc()
    : Binary3RegisterOpAltATesterRegsNotPc(
      state_.Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_)
  {}
};

class Binary3RegisterOpAltATester_Smulxx_Rule_178_P354_RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATester_Smulxx_Rule_178_P354_RegsNotPc()
    : Binary3RegisterOpAltATesterRegsNotPc(
      state_.Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Qadd16_Rule_125_A1_P252_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Qadd16_Rule_125_A1_P252_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Qadd16_Rule_125_A1_P252_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Qadd8_Rule_126_A1_P254_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Qadd8_Rule_126_A1_P254_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Qadd8_Rule_126_A1_P254_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Qasx_Rule_127_A1_P256_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Qasx_Rule_127_A1_P256_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Qasx_Rule_127_A1_P256_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Qsax_Rule_130_A1_P262_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Qsax_Rule_130_A1_P262_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Qsax_Rule_130_A1_P262_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Qsub16_Rule_132_A1_P266_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Qsub16_Rule_132_A1_P266_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Qsub16_Rule_132_A1_P266_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Qsub8_Rule_133_A1_P268_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Qsub8_Rule_133_A1_P268_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Qsub8_Rule_133_A1_P268_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Sadd16_Rule_148_A1_P296_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Sadd16_Rule_148_A1_P296_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Sadd16_Rule_148_A1_P296_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Sasx_Rule_150_A1_P300_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Sasx_Rule_150_A1_P300_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Sasx_Rule_150_A1_P300_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Ssad8_Rule_149_A1_P298_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Ssad8_Rule_149_A1_P298_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Ssad8_Rule_149_A1_P298_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Ssax_Rule_185_A1_P366_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Ssax_Rule_185_A1_P366_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Ssax_Rule_185_A1_P366_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Ssub16_Rule_186_A1_P368_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Ssub16_Rule_186_A1_P368_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Ssub16_Rule_186_A1_P368_instance_)
  {}
};

class Binary3RegisterOpAltBTester_Ssub8_Rule_187_A1_P370_RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_Ssub8_Rule_187_A1_P370_RegsNotPc()
    : Binary3RegisterOpAltBTesterRegsNotPc(
      state_.Binary3RegisterOpAltB_Ssub8_Rule_187_A1_P370_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_Shadd16_Rule_159_A1_P318_RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Shadd16_Rule_159_A1_P318_RegsNotPc()
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_Shadd8_Rule_160_A1_P320_RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Shadd8_Rule_160_A1_P320_RegsNotPc()
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_Shasx_Rule_161_A1_P322_RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Shasx_Rule_161_A1_P322_RegsNotPc()
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_Shsax_Rule_162_A1_P324_RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Shsax_Rule_162_A1_P324_RegsNotPc()
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_Shsub16_Rule_163_A1_P326_RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Shsub16_Rule_163_A1_P326_RegsNotPc()
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_Shsub8_Rule_164_A1_P328_RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Shsub8_Rule_164_A1_P328_RegsNotPc()
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_)
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

class Binary4RegisterDualOpTester_Smlawx_Rule_171_A1_340_RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTester_Smlawx_Rule_171_A1_340_RegsNotPc()
    : Binary4RegisterDualOpTesterRegsNotPc(
      state_.Binary4RegisterDualOp_Smlawx_Rule_171_A1_340_instance_)
  {}
};

class Binary4RegisterDualOpTester_Smlaxx_Rule_166_A1_P330_RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTester_Smlaxx_Rule_166_A1_P330_RegsNotPc()
    : Binary4RegisterDualOpTesterRegsNotPc(
      state_.Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_)
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

class Binary4RegisterDualResultTester_Smlalxx_Rule_169_A1_P336_RegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTester_Smlalxx_Rule_169_A1_P336_RegsNotPc()
    : Binary4RegisterDualResultTesterRegsNotPc(
      state_.Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_)
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
       Binary2RegisterImmediateOpTester_Adc_Rule_6_A1_P14_NotRdIsPcAndS_cccc0010101unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Adc_Rule_6_A1_P14_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Adc_Rule_6_A1_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010101unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS_cccc001010011111ddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS baseline_tester;
  NamedDefs12To15_Add_Rule_5_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001010011111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS_cccc0010100unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Add_Rule_5_A1_P22_NeitherRdIsPcAndSNorRnIsPcAndNotS baseline_tester;
  NamedDefs12To15_Add_Rule_5_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010100unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_And_Rule_11_A1_P34_NotRdIsPcAndS_cccc0010000unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_And_Rule_11_A1_P34_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_And_Rule_11_A1_P34 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010000unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Eor_Rule_44_A1_P94_NotRdIsPcAndS_cccc0010001unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Eor_Rule_44_A1_P94_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Eor_Rule_44_A1_P94 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010001unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Orr_Rule_113_A1_P228_NotRdIsPcAndS_cccc0011100unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Orr_Rule_113_A1_P228_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Orr_Rule_113_A1_P228 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011100unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Rsb_Rule_142_A1_P284_NotRdIsPcAndS_cccc0010011unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Rsb_Rule_142_A1_P284_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Rsb_Rule_142_A1_P284 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010011unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Rsc_Rule_145_A1_P290_NotRdIsPcAndS_cccc0010111unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Rsc_Rule_145_A1_P290_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Rsc_Rule_145_A1_P290 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010111unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Sbc_Rule_151_A1_P302_NotRdIsPcAndS_cccc0010110unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Sbc_Rule_151_A1_P302_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Sbc_Rule_151_A1_P302 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010110unnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS_cccc001001011111ddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS baseline_tester;
  NamedDefs12To15_Sub_Rule_212_A1_P420 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001001011111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS_cccc0010010unnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_Sub_Rule_212_A1_P420_NeitherRdIsPcAndSNorRnIsPcAndNotS baseline_tester;
  NamedDefs12To15_Sub_Rule_212_A1_P420 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010010unnnnddddiiiiiiiiiiii");
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
       Unary1RegisterImmediateOpTester_Adr_Rule_10_A1_P32__cccc001010001111ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_Adr_Rule_10_A1_P32_ baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A1_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001010001111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Adr_Rule_10_A2_P32__cccc001001001111ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_Adr_Rule_10_A2_P32_ baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A2_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001001001111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Mov_Rule_96_A1_P194_NotRdIsPcAndS_cccc0011101u0000ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_Mov_Rule_96_A1_P194_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A1_P194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011101u0000ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Mvn_Rule_106_A1_P214_NotRdIsPcAndS_cccc0011111u0000ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_Mvn_Rule_106_A1_P214_NotRdIsPcAndS baseline_tester;
  NamedDefs12To15_Mvn_Rule_106_A1_P214 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011111u0000ddddiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
