/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif


#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_CLASSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_CLASSES_H_

#include "native_client/src/trusted/validator_arm/actual_classes.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"
#include "native_client/src/trusted/validator_arm/named_class_decoder.h"

/*
 * Define rule decoder classes.
 */
namespace nacl_arm_dec {

class Binary2RegisterBitRange_Bfi_Rule_18_A1_P48
    : public Binary2RegisterBitRange {
 public:
  virtual ~Binary2RegisterBitRange_Bfi_Rule_18_A1_P48() {}
};

class Binary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308
    : public Binary2RegisterBitRangeNotRnIsPc {
 public:
  virtual ~Binary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308() {}
};

class Binary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466
    : public Binary2RegisterBitRangeNotRnIsPc {
 public:
  virtual ~Binary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466() {}
};

class Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76
    : public Binary2RegisterImmedShiftedTest {
 public:
  virtual ~Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76() {}
};

class Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82
    : public Binary2RegisterImmedShiftedTest {
 public:
  virtual ~Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82() {}
};

class Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450
    : public Binary2RegisterImmedShiftedTest {
 public:
  virtual ~Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450() {}
};

class Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456
    : public Binary2RegisterImmedShiftedTest {
 public:
  virtual ~Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456() {}
};

class Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14() {}
};

class Binary2RegisterImmediateOp_Add_Rule_5_A1_P22
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_Add_Rule_5_A1_P22() {}
};

class Binary2RegisterImmediateOp_And_Rule_11_A1_P34
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_And_Rule_11_A1_P34() {}
};

class Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94() {}
};

class Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228() {}
};

class Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284() {}
};

class Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290() {}
};

class Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302() {}
};

class Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420
    : public Binary2RegisterImmediateOp {
 public:
  virtual ~Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420() {}
};

class Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16() {}
};

class Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24() {}
};

class Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36() {}
};

class Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52() {}
};

class Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96() {}
};

class Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230() {}
};

class Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286() {}
};

class Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292() {}
};

class Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304() {}
};

class Binary3RegisterImmedShiftedOp_SubRule_213_A1_P422
    : public Binary3RegisterImmedShiftedOp {
 public:
  virtual ~Binary3RegisterImmedShiftedOp_SubRule_213_A1_P422() {}
};

class Binary3RegisterOp_Asr_Rule_15_A1_P42
    : public Binary3RegisterOp {
 public:
  virtual ~Binary3RegisterOp_Asr_Rule_15_A1_P42() {}
};

class Binary3RegisterOp_Lsl_Rule_89_A1_P180
    : public Binary3RegisterOp {
 public:
  virtual ~Binary3RegisterOp_Lsl_Rule_89_A1_P180() {}
};

class Binary3RegisterOp_Lsr_Rule_91_A1_P184
    : public Binary3RegisterOp {
 public:
  virtual ~Binary3RegisterOp_Lsr_Rule_91_A1_P184() {}
};

class Binary3RegisterOp_Ror_Rule_140_A1_P280
    : public Binary3RegisterOp {
 public:
  virtual ~Binary3RegisterOp_Ror_Rule_140_A1_P280() {}
};

class Binary3RegisterOpAltA_Mul_Rule_105_A1_P212
    : public Binary3RegisterOpAltA {
 public:
  virtual ~Binary3RegisterOpAltA_Mul_Rule_105_A1_P212() {}
};

class Binary3RegisterOpAltA_Smmul_Rule_176_P350
    : public Binary3RegisterOpAltA {
 public:
  virtual ~Binary3RegisterOpAltA_Smmul_Rule_176_P350() {}
};

class Binary3RegisterOpAltA_Smuad_Rule_177_P352
    : public Binary3RegisterOpAltA {
 public:
  virtual ~Binary3RegisterOpAltA_Smuad_Rule_177_P352() {}
};

class Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358
    : public Binary3RegisterOpAltA {
 public:
  virtual ~Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358() {}
};

class Binary3RegisterOpAltA_Smulxx_Rule_178_P354
    : public Binary3RegisterOpAltA {
 public:
  virtual ~Binary3RegisterOpAltA_Smulxx_Rule_178_P354() {}
};

class Binary3RegisterOpAltA_Smusd_Rule_181_P360
    : public Binary3RegisterOpAltA {
 public:
  virtual ~Binary3RegisterOpAltA_Smusd_Rule_181_P360() {}
};

class Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500
    : public Binary3RegisterOpAltA {
 public:
  virtual ~Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500() {}
};

class Binary3RegisterOpAltB_Qadd16_Rule_125_A1_P252
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qadd16_Rule_125_A1_P252() {}
};

class Binary3RegisterOpAltB_Qadd8_Rule_126_A1_P254
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qadd8_Rule_126_A1_P254() {}
};

class Binary3RegisterOpAltB_Qadd_Rule_124_A1_P250
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qadd_Rule_124_A1_P250() {}
};

class Binary3RegisterOpAltB_Qasx_Rule_127_A1_P256
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qasx_Rule_127_A1_P256() {}
};

class Binary3RegisterOpAltB_Qdadd_Rule_128_A1_P258
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qdadd_Rule_128_A1_P258() {}
};

class Binary3RegisterOpAltB_Qdsub_Rule_129_A1_P260
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qdsub_Rule_129_A1_P260() {}
};

class Binary3RegisterOpAltB_Qsax_Rule_130_A1_P262
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qsax_Rule_130_A1_P262() {}
};

class Binary3RegisterOpAltB_Qsub16_Rule_132_A1_P266
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qsub16_Rule_132_A1_P266() {}
};

class Binary3RegisterOpAltB_Qsub8_Rule_133_A1_P268
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qsub8_Rule_133_A1_P268() {}
};

class Binary3RegisterOpAltB_Qsub_Rule_131_A1_P264
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Qsub_Rule_131_A1_P264() {}
};

class Binary3RegisterOpAltB_Sadd16_Rule_148_A1_P296
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Sadd16_Rule_148_A1_P296() {}
};

class Binary3RegisterOpAltB_Sasx_Rule_150_A1_P300
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Sasx_Rule_150_A1_P300() {}
};

class Binary3RegisterOpAltB_Ssad8_Rule_149_A1_P298
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Ssad8_Rule_149_A1_P298() {}
};

class Binary3RegisterOpAltB_Ssax_Rule_185_A1_P366
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Ssax_Rule_185_A1_P366() {}
};

class Binary3RegisterOpAltB_Ssub16_Rule_186_A1_P368
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Ssub16_Rule_186_A1_P368() {}
};

class Binary3RegisterOpAltB_Ssub8_Rule_187_A1_P370
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Ssub8_Rule_187_A1_P370() {}
};

class Binary3RegisterOpAltB_Uadd16_Rule_233_A1_P460
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uadd16_Rule_233_A1_P460() {}
};

class Binary3RegisterOpAltB_Uadd8_Rule_234_A1_P462
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uadd8_Rule_234_A1_P462() {}
};

class Binary3RegisterOpAltB_Uasx_Rule_235_A1_P464
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uasx_Rule_235_A1_P464() {}
};

class Binary3RegisterOpAltB_Uqadd16_Rule_247_A1_P488
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uqadd16_Rule_247_A1_P488() {}
};

class Binary3RegisterOpAltB_Uqadd8_Rule_248_A1_P490
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uqadd8_Rule_248_A1_P490() {}
};

class Binary3RegisterOpAltB_Uqasx_Rule_249_A1_P492
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uqasx_Rule_249_A1_P492() {}
};

class Binary3RegisterOpAltB_Uqsax_Rule_250_A1_P494
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uqsax_Rule_250_A1_P494() {}
};

class Binary3RegisterOpAltB_Uqsub16_Rule_251_A1_P496
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uqsub16_Rule_251_A1_P496() {}
};

class Binary3RegisterOpAltB_Uqsub8_Rule_252_A1_P498
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Uqsub8_Rule_252_A1_P498() {}
};

class Binary3RegisterOpAltB_Usax_Rule_257_A1_P508
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Usax_Rule_257_A1_P508() {}
};

class Binary3RegisterOpAltB_Usub16_Rule_258_A1_P510
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Usub16_Rule_258_A1_P510() {}
};

class Binary3RegisterOpAltB_Usub8_Rule_259_A1_P512
    : public Binary3RegisterOpAltB {
 public:
  virtual ~Binary3RegisterOpAltB_Usub8_Rule_259_A1_P512() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478() {}
};

class Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480
    : public Binary3RegisterOpAltBNoCondUpdates {
 public:
  virtual ~Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480() {}
};

class Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78
    : public Binary3RegisterShiftedTest {
 public:
  virtual ~Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78() {}
};

class Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84
    : public Binary3RegisterShiftedTest {
 public:
  virtual ~Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84() {}
};

class Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452
    : public Binary3RegisterShiftedTest {
 public:
  virtual ~Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452() {}
};

class Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458
    : public Binary3RegisterShiftedTest {
 public:
  virtual ~Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458() {}
};

class Binary4RegisterDualOp_Mla_Rule_94_A1_P190
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Mla_Rule_94_A1_P190() {}
};

class Binary4RegisterDualOp_Mls_Rule_95_A1_P192
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Mls_Rule_95_A1_P192() {}
};

class Binary4RegisterDualOp_Smlad_Rule_167_P332
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Smlad_Rule_167_P332() {}
};

class Binary4RegisterDualOp_Smlawx_Rule_171_A1_340
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Smlawx_Rule_171_A1_340() {}
};

class Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330() {}
};

class Binary4RegisterDualOp_Smlsd_Rule_172_P342
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Smlsd_Rule_172_P342() {}
};

class Binary4RegisterDualOp_Smmla_Rule_174_P346
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Smmla_Rule_174_P346() {}
};

class Binary4RegisterDualOp_Smmls_Rule_175_P348
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Smmls_Rule_175_P348() {}
};

class Binary4RegisterDualOp_Usda8_Rule_254_A1_P502
    : public Binary4RegisterDualOp {
 public:
  virtual ~Binary4RegisterDualOp_Usda8_Rule_254_A1_P502() {}
};

class Binary4RegisterDualResult_Smlal_Rule_168_A1_P334
    : public Binary4RegisterDualResult {
 public:
  virtual ~Binary4RegisterDualResult_Smlal_Rule_168_A1_P334() {}
};

class Binary4RegisterDualResult_Smlald_Rule_170_P336
    : public Binary4RegisterDualResult {
 public:
  virtual ~Binary4RegisterDualResult_Smlald_Rule_170_P336() {}
};

class Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336
    : public Binary4RegisterDualResult {
 public:
  virtual ~Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336() {}
};

class Binary4RegisterDualResult_Smlsld_Rule_173_P344
    : public Binary4RegisterDualResult {
 public:
  virtual ~Binary4RegisterDualResult_Smlsld_Rule_173_P344() {}
};

class Binary4RegisterDualResult_Smull_Rule_179_A1_P356
    : public Binary4RegisterDualResult {
 public:
  virtual ~Binary4RegisterDualResult_Smull_Rule_179_A1_P356() {}
};

class Binary4RegisterDualResult_Umaal_Rule_244_A1_P482
    : public Binary4RegisterDualResult {
 public:
  virtual ~Binary4RegisterDualResult_Umaal_Rule_244_A1_P482() {}
};

class Binary4RegisterDualResult_Umlal_Rule_245_A1_P484
    : public Binary4RegisterDualResult {
 public:
  virtual ~Binary4RegisterDualResult_Umlal_Rule_245_A1_P484() {}
};

class Binary4RegisterDualResult_Umull_Rule_246_A1_P486
    : public Binary4RegisterDualResult {
 public:
  virtual ~Binary4RegisterDualResult_Umull_Rule_246_A1_P486() {}
};

class Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18() {}
};

class Binary4RegisterShiftedOp_Add_Rule_7_A1_P26
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Add_Rule_7_A1_P26() {}
};

class Binary4RegisterShiftedOp_And_Rule_13_A1_P38
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_And_Rule_13_A1_P38() {}
};

class Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54() {}
};

class Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98() {}
};

class Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212() {}
};

class Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288() {}
};

class Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294() {}
};

class Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306() {}
};

class Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424
    : public Binary4RegisterShiftedOp {
 public:
  virtual ~Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424() {}
};

class BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74
    : public BinaryRegisterImmediateTest {
 public:
  virtual ~BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74() {}
};

class BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80
    : public BinaryRegisterImmediateTest {
 public:
  virtual ~BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80() {}
};

class BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448
    : public BinaryRegisterImmediateTest {
 public:
  virtual ~BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448() {}
};

class Branch_None
    : public Branch {
 public:
  virtual ~Branch_None() {}
};

class BranchToRegister_Blx_Rule_24_A1_P60
    : public BranchToRegister {
 public:
  virtual ~BranchToRegister_Blx_Rule_24_A1_P60() {}
};

class BranchToRegister_Bx_Rule_25_A1_P62
    : public BranchToRegister {
 public:
  virtual ~BranchToRegister_Bx_Rule_25_A1_P62() {}
};

class BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56
    : public BreakPointAndConstantPoolHead {
 public:
  virtual ~BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56() {}
};

class CondNop_Dbg_Rule_40_A1_P88
    : public CondNop {
 public:
  virtual ~CondNop_Dbg_Rule_40_A1_P88() {}
};

class CondNop_Nop_Rule_110_A1_P222
    : public CondNop {
 public:
  virtual ~CondNop_Nop_Rule_110_A1_P222() {}
};

class CondNop_Yield_Rule_413_A1_P812
    : public CondNop {
 public:
  virtual ~CondNop_Yield_Rule_413_A1_P812() {}
};

class CondVfpOp_Vadd_Rule_271_A2_P536
    : public CondVfpOp {
 public:
  virtual ~CondVfpOp_Vadd_Rule_271_A2_P536() {}
};

class CondVfpOp_Vdiv_Rule_301_A1_P590
    : public CondVfpOp {
 public:
  virtual ~CondVfpOp_Vdiv_Rule_301_A1_P590() {}
};

class CondVfpOp_Vm_la_ls_Rule_423_A2_P636
    : public CondVfpOp {
 public:
  virtual ~CondVfpOp_Vm_la_ls_Rule_423_A2_P636() {}
};

class CondVfpOp_Vmul_Rule_338_A2_P664
    : public CondVfpOp {
 public:
  virtual ~CondVfpOp_Vmul_Rule_338_A2_P664() {}
};

class CondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674
    : public CondVfpOp {
 public:
  virtual ~CondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674() {}
};

class CondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674
    : public CondVfpOp {
 public:
  virtual ~CondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674() {}
};

class CondVfpOp_Vsub_Rule_402_A2_P790
    : public CondVfpOp {
 public:
  virtual ~CondVfpOp_Vsub_Rule_402_A2_P790() {}
};

class CoprocessorOp_None
    : public CoprocessorOp {
 public:
  virtual ~CoprocessorOp_None() {}
};

class DataProc_None
    : public DataProc {
 public:
  virtual ~DataProc_None() {}
};

class Deprecated_None
    : public Deprecated {
 public:
  virtual ~Deprecated_None() {}
};

class EffectiveNoOp_None
    : public EffectiveNoOp {
 public:
  virtual ~EffectiveNoOp_None() {}
};

class Forbidden_None
    : public Forbidden {
 public:
  virtual ~Forbidden_None() {}
};

class ForbiddenCondNop_Bxj_Rule_26_A1_P64
    : public ForbiddenCondNop {
 public:
  virtual ~ForbiddenCondNop_Bxj_Rule_26_A1_P64() {}
};

class ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12
    : public ForbiddenCondNop {
 public:
  virtual ~ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12() {}
};

class ForbiddenCondNop_Msr_Rule_B6_1_7_P14
    : public ForbiddenCondNop {
 public:
  virtual ~ForbiddenCondNop_Msr_Rule_B6_1_7_P14() {}
};

class ForbiddenCondNop_Sev_Rule_158_A1_P316
    : public ForbiddenCondNop {
 public:
  virtual ~ForbiddenCondNop_Sev_Rule_158_A1_P316() {}
};

class ForbiddenCondNop_Smc_Rule_B6_1_9_P18
    : public ForbiddenCondNop {
 public:
  virtual ~ForbiddenCondNop_Smc_Rule_B6_1_9_P18() {}
};

class ForbiddenCondNop_Wfe_Rule_411_A1_P808
    : public ForbiddenCondNop {
 public:
  virtual ~ForbiddenCondNop_Wfe_Rule_411_A1_P808() {}
};

class ForbiddenCondNop_Wfi_Rule_412_A1_P810
    : public ForbiddenCondNop {
 public:
  virtual ~ForbiddenCondNop_Wfi_Rule_412_A1_P810() {}
};

class Load2RegisterImm12Op_Ldr_Rule_58_A1_P120
    : public Load2RegisterImm12Op {
 public:
  virtual ~Load2RegisterImm12Op_Ldr_Rule_58_A1_P120() {}
};

class Load2RegisterImm12Op_Ldr_Rule_59_A1_P122
    : public Load2RegisterImm12Op {
 public:
  virtual ~Load2RegisterImm12Op_Ldr_Rule_59_A1_P122() {}
};

class Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128
    : public Load2RegisterImm12Op {
 public:
  virtual ~Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128() {}
};

class Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130
    : public Load2RegisterImm12Op {
 public:
  virtual ~Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130() {}
};

class Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136
    : public Load2RegisterImm8DoubleOp {
 public:
  virtual ~Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136() {}
};

class Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138
    : public Load2RegisterImm8DoubleOp {
 public:
  virtual ~Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138() {}
};

class Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152
    : public Load2RegisterImm8Op {
 public:
  virtual ~Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152() {}
};

class Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154
    : public Load2RegisterImm8Op {
 public:
  virtual ~Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154() {}
};

class Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160
    : public Load2RegisterImm8Op {
 public:
  virtual ~Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160() {}
};

class Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168
    : public Load2RegisterImm8Op {
 public:
  virtual ~Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168() {}
};

class Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170
    : public Load2RegisterImm8Op {
 public:
  virtual ~Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170() {}
};

class Load2RegisterImm8Op_ldrsb_Rule_79_A1_162
    : public Load2RegisterImm8Op {
 public:
  virtual ~Load2RegisterImm8Op_ldrsb_Rule_79_A1_162() {}
};

class Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140
    : public Load3RegisterDoubleOp {
 public:
  virtual ~Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140() {}
};

class Load3RegisterImm5Op_Ldr_Rule_60_A1_P124
    : public Load3RegisterImm5Op {
 public:
  virtual ~Load3RegisterImm5Op_Ldr_Rule_60_A1_P124() {}
};

class Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132
    : public Load3RegisterImm5Op {
 public:
  virtual ~Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132() {}
};

class Load3RegisterOp_Ldrh_Rule_76_A1_P156
    : public Load3RegisterOp {
 public:
  virtual ~Load3RegisterOp_Ldrh_Rule_76_A1_P156() {}
};

class Load3RegisterOp_Ldrsb_Rule_80_A1_P164
    : public Load3RegisterOp {
 public:
  virtual ~Load3RegisterOp_Ldrsb_Rule_80_A1_P164() {}
};

class Load3RegisterOp_Ldrsh_Rule_84_A1_P172
    : public Load3RegisterOp {
 public:
  virtual ~Load3RegisterOp_Ldrsh_Rule_84_A1_P172() {}
};

class LoadCoprocessor_None
    : public LoadCoprocessor {
 public:
  virtual ~LoadCoprocessor_None() {}
};

class LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146
    : public LoadExclusive2RegisterDoubleOp {
 public:
  virtual ~LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146() {}
};

class LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142
    : public LoadExclusive2RegisterOp {
 public:
  virtual ~LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142() {}
};

class LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144
    : public LoadExclusive2RegisterOp {
 public:
  virtual ~LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144() {}
};

class LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148
    : public LoadExclusive2RegisterOp {
 public:
  virtual ~LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148() {}
};

class LoadMultiple_None
    : public LoadMultiple {
 public:
  virtual ~LoadMultiple_None() {}
};

class MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50
    : public MaskedBinary2RegisterImmediateOp {
 public:
  virtual ~MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50() {}
};

class MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454
    : public MaskedBinaryRegisterImmediateTest {
 public:
  virtual ~MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454() {}
};

class MoveDoubleFromCoprocessor_None
    : public MoveDoubleFromCoprocessor {
 public:
  virtual ~MoveDoubleFromCoprocessor_None() {}
};

class MoveFromCoprocessor_None
    : public MoveFromCoprocessor {
 public:
  virtual ~MoveFromCoprocessor_None() {}
};

class MoveImmediate12ToApsr_Msr_Rule_103_A1_P208
    : public MoveImmediate12ToApsr {
 public:
  virtual ~MoveImmediate12ToApsr_Msr_Rule_103_A1_P208() {}
};

class PackSatRev_None
    : public PackSatRev {
 public:
  virtual ~PackSatRev_None() {}
};

class Roadblock_None
    : public Roadblock {
 public:
  virtual ~Roadblock_None() {}
};

class Store2RegisterImm12Op_Str_Rule_194_A1_P384
    : public Store2RegisterImm12Op {
 public:
  virtual ~Store2RegisterImm12Op_Str_Rule_194_A1_P384() {}
};

class Store2RegisterImm12Op_Strb_Rule_197_A1_P390
    : public Store2RegisterImm12Op {
 public:
  virtual ~Store2RegisterImm12Op_Strb_Rule_197_A1_P390() {}
};

class Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396
    : public Store2RegisterImm8DoubleOp {
 public:
  virtual ~Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396() {}
};

class Store2RegisterImm8Op_Strh_Rule_207_A1_P410
    : public Store2RegisterImm8Op {
 public:
  virtual ~Store2RegisterImm8Op_Strh_Rule_207_A1_P410() {}
};

class Store3RegisterDoubleOp_Strd_Rule_201_A1_P398
    : public Store3RegisterDoubleOp {
 public:
  virtual ~Store3RegisterDoubleOp_Strd_Rule_201_A1_P398() {}
};

class Store3RegisterImm5Op_Str_Rule_195_A1_P386
    : public Store3RegisterImm5Op {
 public:
  virtual ~Store3RegisterImm5Op_Str_Rule_195_A1_P386() {}
};

class Store3RegisterImm5Op_Strb_Rule_198_A1_P392
    : public Store3RegisterImm5Op {
 public:
  virtual ~Store3RegisterImm5Op_Strb_Rule_198_A1_P392() {}
};

class Store3RegisterOp_Strh_Rule_208_A1_P412
    : public Store3RegisterOp {
 public:
  virtual ~Store3RegisterOp_Strh_Rule_208_A1_P412() {}
};

class StoreCoprocessor_None
    : public StoreCoprocessor {
 public:
  virtual ~StoreCoprocessor_None() {}
};

class StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404
    : public StoreExclusive3RegisterDoubleOp {
 public:
  virtual ~StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404() {}
};

class StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400
    : public StoreExclusive3RegisterOp {
 public:
  virtual ~StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400() {}
};

class StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402
    : public StoreExclusive3RegisterOp {
 public:
  virtual ~StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402() {}
};

class StoreExclusive3RegisterOp_cccc00011110nnnndddd11111001tttt
    : public StoreExclusive3RegisterOp {
 public:
  virtual ~StoreExclusive3RegisterOp_cccc00011110nnnndddd11111001tttt() {}
};

class StoreImmediate_None
    : public StoreImmediate {
 public:
  virtual ~StoreImmediate_None() {}
};

class Unary1RegisterBitRange_Bfc_17_A1_P46
    : public Unary1RegisterBitRange {
 public:
  virtual ~Unary1RegisterBitRange_Bfc_17_A1_P46() {}
};

class Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32
    : public Unary1RegisterImmediateOp {
 public:
  virtual ~Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32() {}
};

class Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32
    : public Unary1RegisterImmediateOp {
 public:
  virtual ~Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32() {}
};

class Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194
    : public Unary1RegisterImmediateOp {
 public:
  virtual ~Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194() {}
};

class Unary1RegisterImmediateOp_Mov_Rule_96_A2_P_194
    : public Unary1RegisterImmediateOp {
 public:
  virtual ~Unary1RegisterImmediateOp_Mov_Rule_96_A2_P_194() {}
};

class Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214
    : public Unary1RegisterImmediateOp {
 public:
  virtual ~Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214() {}
};

class Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10
    : public Unary1RegisterSet {
 public:
  virtual ~Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10() {}
};

class Unary1RegisterUse_Msr_Rule_104_A1_P210
    : public Unary1RegisterUse {
 public:
  virtual ~Unary1RegisterUse_Msr_Rule_104_A1_P210() {}
};

class Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40
    : public Unary2RegisterImmedShiftedOp {
 public:
  virtual ~Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40() {}
};

class Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178
    : public Unary2RegisterImmedShiftedOp {
 public:
  virtual ~Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178() {}
};

class Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182
    : public Unary2RegisterImmedShiftedOp {
 public:
  virtual ~Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182() {}
};

class Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216
    : public Unary2RegisterImmedShiftedOp {
 public:
  virtual ~Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216() {}
};

class Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278
    : public Unary2RegisterImmedShiftedOp {
 public:
  virtual ~Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278() {}
};

class Unary2RegisterOp_Mov_Rule_97_A1_P196
    : public Unary2RegisterOp {
 public:
  virtual ~Unary2RegisterOp_Mov_Rule_97_A1_P196() {}
};

class Unary2RegisterOp_Rrx_Rule_141_A1_P282
    : public Unary2RegisterOp {
 public:
  virtual ~Unary2RegisterOp_Rrx_Rule_141_A1_P282() {}
};

class Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72
    : public Unary2RegisterOpNotRmIsPc {
 public:
  virtual ~Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72() {}
};

class Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218
    : public Unary3RegisterShiftedOp {
 public:
  virtual ~Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218() {}
};

class Undefined_None
    : public Undefined {
 public:
  virtual ~Undefined_None() {}
};

class Unpredictable_None
    : public Unpredictable {
 public:
  virtual ~Unpredictable_None() {}
};

class VectorLoad_None
    : public VectorLoad {
 public:
  virtual ~VectorLoad_None() {}
};

class VectorStore_None
    : public VectorStore {
 public:
  virtual ~VectorStore_None() {}
};

class Breakpoint_Bkpt_Rule_22_A1_P56
    : public Breakpoint {
 public:
  virtual ~Breakpoint_Bkpt_Rule_22_A1_P56() {}
};

class BxBlx_Blx_Rule_24_A1_P60
    : public BxBlx {
 public:
  virtual ~BxBlx_Blx_Rule_24_A1_P60() {}
};

class BxBlx_Bx_Rule_25_A1_P62
    : public BxBlx {
 public:
  virtual ~BxBlx_Bx_Rule_25_A1_P62() {}
};

class Defs12To15_Adc_Rule_2_A1_P16
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Adc_Rule_2_A1_P16() {}
};

class Defs12To15_Adc_Rule_6_A1_P14
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Adc_Rule_6_A1_P14() {}
};

class Defs12To15_Add_Rule_5_A1_P22
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Add_Rule_5_A1_P22() {}
};

class Defs12To15_Add_Rule_6_A1_P24
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Add_Rule_6_A1_P24() {}
};

class Defs12To15_Adr_Rule_10_A1_P32
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Adr_Rule_10_A1_P32() {}
};

class Defs12To15_Adr_Rule_10_A2_P32
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Adr_Rule_10_A2_P32() {}
};

class Defs12To15_And_Rule_11_A1_P34
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_And_Rule_11_A1_P34() {}
};

class Defs12To15_And_Rule_7_A1_P36
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_And_Rule_7_A1_P36() {}
};

class Defs12To15_Asr_Rule_14_A1_P40
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Asr_Rule_14_A1_P40() {}
};

class Defs12To15_Bfi_Rule_18_A1_P48
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Bfi_Rule_18_A1_P48() {}
};

class Defs12To15_Bic_Rule_20_A1_P52
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Bic_Rule_20_A1_P52() {}
};

class Defs12To15_Eor_Rule_44_A1_P94
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Eor_Rule_44_A1_P94() {}
};

class Defs12To15_Lsl_Rule_88_A1_P178
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Lsl_Rule_88_A1_P178() {}
};

class Defs12To15_Lsr_Rule_90_A1_P182
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Lsr_Rule_90_A1_P182() {}
};

class Defs12To15_Mov_Rule_96_A1_P194
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Mov_Rule_96_A1_P194() {}
};

class Defs12To15_Mov_Rule_96_A2_P_194
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Mov_Rule_96_A2_P_194() {}
};

class Defs12To15_Mov_Rule_97_A1_P196
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Mov_Rule_97_A1_P196() {}
};

class Defs12To15_Mvn_Rule_106_A1_P214
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Mvn_Rule_106_A1_P214() {}
};

class Defs12To15_Mvn_Rule_107_A1_P216
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Mvn_Rule_107_A1_P216() {}
};

class Defs12To15_Orr_Rule_113_A1_P228
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Orr_Rule_113_A1_P228() {}
};

class Defs12To15_Orr_Rule_114_A1_P230
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Orr_Rule_114_A1_P230() {}
};

class Defs12To15_Ror_Rule_139_A1_P278
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Ror_Rule_139_A1_P278() {}
};

class Defs12To15_Rrx_Rule_141_A1_P282
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Rrx_Rule_141_A1_P282() {}
};

class Defs12To15_Rsb_Rule_142_A1_P284
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Rsb_Rule_142_A1_P284() {}
};

class Defs12To15_Rsb_Rule_143_P286
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Rsb_Rule_143_P286() {}
};

class Defs12To15_Rsc_Rule_145_A1_P290
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Rsc_Rule_145_A1_P290() {}
};

class Defs12To15_Rsc_Rule_146_A1_P292
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Rsc_Rule_146_A1_P292() {}
};

class Defs12To15_Sbc_Rule_151_A1_P302
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Sbc_Rule_151_A1_P302() {}
};

class Defs12To15_Sbc_Rule_152_A1_P304
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Sbc_Rule_152_A1_P304() {}
};

class Defs12To15_SubRule_213_A1_P422
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_SubRule_213_A1_P422() {}
};

class Defs12To15_Sub_Rule_212_A1_P420
    : public Defs12To15 {
 public:
  virtual ~Defs12To15_Sub_Rule_212_A1_P420() {}
};

class Defs12To15CondsDontCare_Eor_Rule_45_A1_P96
    : public Defs12To15CondsDontCare {
 public:
  virtual ~Defs12To15CondsDontCare_Eor_Rule_45_A1_P96() {}
};

class Defs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98
    : public Defs12To15CondsDontCareRdRnRsRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Ssad8_Rule_149_A1_P298
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Ssad8_Rule_149_A1_P298() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510() {}
};

class Defs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512
    : public Defs12To15CondsDontCareRnRdRmNotPc {
 public:
  virtual ~Defs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512() {}
};

class Defs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42
    : public Defs12To15RdRmRnNotPc {
 public:
  virtual ~Defs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42() {}
};

class Defs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180
    : public Defs12To15RdRmRnNotPc {
 public:
  virtual ~Defs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180() {}
};

class Defs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184
    : public Defs12To15RdRmRnNotPc {
 public:
  virtual ~Defs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184() {}
};

class Defs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218
    : public Defs12To15RdRmRnNotPc {
 public:
  virtual ~Defs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218() {}
};

class Defs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280
    : public Defs12To15RdRmRnNotPc {
 public:
  virtual ~Defs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280() {}
};

class Defs12To15RdRnNotPc_Clz_Rule_31_A1_P72
    : public Defs12To15RdRnNotPc {
 public:
  virtual ~Defs12To15RdRnNotPc_Clz_Rule_31_A1_P72() {}
};

class Defs12To15RdRnNotPc_Sbfx_Rule_154_A1_P308
    : public Defs12To15RdRnNotPc {
 public:
  virtual ~Defs12To15RdRnNotPc_Sbfx_Rule_154_A1_P308() {}
};

class Defs12To15RdRnNotPc_Ubfx_Rule_236_A1_P466
    : public Defs12To15RdRnNotPc {
 public:
  virtual ~Defs12To15RdRnNotPc_Ubfx_Rule_236_A1_P466() {}
};

class Defs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18() {}
};

class Defs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26() {}
};

class Defs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38() {}
};

class Defs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54() {}
};

class Defs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212() {}
};

class Defs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288() {}
};

class Defs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294() {}
};

class Defs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306() {}
};

class Defs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424
    : public Defs12To15RdRnRsRmNotPc {
 public:
  virtual ~Defs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424() {}
};

class Defs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334
    : public Defs12To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334() {}
};

class Defs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336
    : public Defs12To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336() {}
};

class Defs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336
    : public Defs12To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336() {}
};

class Defs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344
    : public Defs12To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344() {}
};

class Defs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356
    : public Defs12To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356() {}
};

class Defs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482
    : public Defs12To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482() {}
};

class Defs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484
    : public Defs12To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484() {}
};

class Defs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486
    : public Defs12To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348() {}
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Usda8_Rule_254_A1_P502
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRaRmRnNotPc_Usda8_Rule_254_A1_P502() {}
};

class Defs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212
    : public Defs16To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212() {}
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350
    : public Defs16To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350() {}
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352
    : public Defs16To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352() {}
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358
    : public Defs16To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358() {}
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354
    : public Defs16To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354() {}
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360
    : public Defs16To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360() {}
};

class Defs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500
    : public Defs16To19CondsDontCareRdRmRnNotPc {
 public:
  virtual ~Defs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500() {}
};

class DontCareInst_Cmn_Rule_32_A1_P74
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Cmn_Rule_32_A1_P74() {}
};

class DontCareInst_Cmn_Rule_33_A1_P76
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Cmn_Rule_33_A1_P76() {}
};

class DontCareInst_Cmp_Rule_35_A1_P80
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Cmp_Rule_35_A1_P80() {}
};

class DontCareInst_Cmp_Rule_36_A1_P82
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Cmp_Rule_36_A1_P82() {}
};

class DontCareInst_Dbg_Rule_40_A1_P88
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Dbg_Rule_40_A1_P88() {}
};

class DontCareInst_Msr_Rule_103_A1_P208
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Msr_Rule_103_A1_P208() {}
};

class DontCareInst_Nop_Rule_110_A1_P222
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Nop_Rule_110_A1_P222() {}
};

class DontCareInst_Teq_Rule_227_A1_P448
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Teq_Rule_227_A1_P448() {}
};

class DontCareInst_Teq_Rule_228_A1_P450
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Teq_Rule_228_A1_P450() {}
};

class DontCareInst_Tst_Rule_231_A1_P456
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Tst_Rule_231_A1_P456() {}
};

class DontCareInst_Yield_Rule_413_A1_P812
    : public DontCareInst {
 public:
  virtual ~DontCareInst_Yield_Rule_413_A1_P812() {}
};

class DontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78
    : public DontCareInstRnRsRmNotPc {
 public:
  virtual ~DontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78() {}
};

class DontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84
    : public DontCareInstRnRsRmNotPc {
 public:
  virtual ~DontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84() {}
};

class DontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452
    : public DontCareInstRnRsRmNotPc {
 public:
  virtual ~DontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452() {}
};

class DontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458
    : public DontCareInstRnRsRmNotPc {
 public:
  virtual ~DontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458() {}
};

class Forbidden_Bxj_Rule_26_A1_P64
    : public Forbidden {
 public:
  virtual ~Forbidden_Bxj_Rule_26_A1_P64() {}
};

class Forbidden_Msr_Rule_B6_1_6_A1_PB6_12
    : public Forbidden {
 public:
  virtual ~Forbidden_Msr_Rule_B6_1_6_A1_PB6_12() {}
};

class Forbidden_Msr_Rule_B6_1_7_P14
    : public Forbidden {
 public:
  virtual ~Forbidden_Msr_Rule_B6_1_7_P14() {}
};

class Forbidden_Sev_Rule_158_A1_P316
    : public Forbidden {
 public:
  virtual ~Forbidden_Sev_Rule_158_A1_P316() {}
};

class Forbidden_Smc_Rule_B6_1_9_P18
    : public Forbidden {
 public:
  virtual ~Forbidden_Smc_Rule_B6_1_9_P18() {}
};

class Forbidden_Wfe_Rule_411_A1_P808
    : public Forbidden {
 public:
  virtual ~Forbidden_Wfe_Rule_411_A1_P808() {}
};

class Forbidden_Wfi_Rule_412_A1_P810
    : public Forbidden {
 public:
  virtual ~Forbidden_Wfi_Rule_412_A1_P810() {}
};

class LdrImmediate_Ldr_Rule_58_A1_P120
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldr_Rule_58_A1_P120() {}
};

class LdrImmediate_Ldr_Rule_59_A1_P122
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldr_Rule_59_A1_P122() {}
};

class LdrImmediate_Ldrb_Rule_62_A1_P128
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldrb_Rule_62_A1_P128() {}
};

class LdrImmediate_Ldrb_Rule_63_A1_P130
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldrb_Rule_63_A1_P130() {}
};

class LdrImmediate_Ldrh_Rule_74_A1_P152
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldrh_Rule_74_A1_P152() {}
};

class LdrImmediate_Ldrh_Rule_75_A1_P154
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldrh_Rule_75_A1_P154() {}
};

class LdrImmediate_Ldrsb_Rule_78_A1_P160
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldrsb_Rule_78_A1_P160() {}
};

class LdrImmediate_Ldrsh_Rule_82_A1_P168
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldrsh_Rule_82_A1_P168() {}
};

class LdrImmediate_Ldrsh_Rule_83_A1_P170
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_Ldrsh_Rule_83_A1_P170() {}
};

class LdrImmediate_ldrsb_Rule_79_A1_162
    : public LdrImmediate {
 public:
  virtual ~LdrImmediate_ldrsb_Rule_79_A1_162() {}
};

class LdrImmediateDouble_Ldrd_Rule_66_A1_P136
    : public LdrImmediateDouble {
 public:
  virtual ~LdrImmediateDouble_Ldrd_Rule_66_A1_P136() {}
};

class LdrImmediateDouble_Ldrd_Rule_67_A1_P138
    : public LdrImmediateDouble {
 public:
  virtual ~LdrImmediateDouble_Ldrd_Rule_67_A1_P138() {}
};

class LdrRegister_Ldr_Rule_60_A1_P124
    : public LdrRegister {
 public:
  virtual ~LdrRegister_Ldr_Rule_60_A1_P124() {}
};

class LdrRegister_Ldrb_Rule_64_A1_P132
    : public LdrRegister {
 public:
  virtual ~LdrRegister_Ldrb_Rule_64_A1_P132() {}
};

class LdrRegister_Ldrh_Rule_76_A1_P156
    : public LdrRegister {
 public:
  virtual ~LdrRegister_Ldrh_Rule_76_A1_P156() {}
};

class LdrRegister_Ldrsb_Rule_80_A1_P164
    : public LdrRegister {
 public:
  virtual ~LdrRegister_Ldrsb_Rule_80_A1_P164() {}
};

class LdrRegister_Ldrsh_Rule_84_A1_P172
    : public LdrRegister {
 public:
  virtual ~LdrRegister_Ldrsh_Rule_84_A1_P172() {}
};

class LdrRegisterDouble_Ldrd_Rule_68_A1_P140
    : public LdrRegisterDouble {
 public:
  virtual ~LdrRegisterDouble_Ldrd_Rule_68_A1_P140() {}
};

class LoadBasedMemory_Ldrex_Rule_69_A1_P142
    : public LoadBasedMemory {
 public:
  virtual ~LoadBasedMemory_Ldrex_Rule_69_A1_P142() {}
};

class LoadBasedMemory_Ldrexb_Rule_70_A1_P144
    : public LoadBasedMemory {
 public:
  virtual ~LoadBasedMemory_Ldrexb_Rule_70_A1_P144() {}
};

class LoadBasedMemory_Ldrexh_Rule_72_A1_P148
    : public LoadBasedMemory {
 public:
  virtual ~LoadBasedMemory_Ldrexh_Rule_72_A1_P148() {}
};

class LoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146
    : public LoadBasedMemoryDouble {
 public:
  virtual ~LoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146() {}
};

class MaskAddress_Bic_Rule_19_A1_P50
    : public MaskAddress {
 public:
  virtual ~MaskAddress_Bic_Rule_19_A1_P50() {}
};

class StoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404
    : public StoreBasedMemoryDoubleRtBits0To3 {
 public:
  virtual ~StoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404() {}
};

class StoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400
    : public StoreBasedMemoryRtBits0To3 {
 public:
  virtual ~StoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400() {}
};

class StoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402
    : public StoreBasedMemoryRtBits0To3 {
 public:
  virtual ~StoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402() {}
};

class StoreBasedMemoryRtBits0To3_cccc00011110nnnndddd11111001tttt
    : public StoreBasedMemoryRtBits0To3 {
 public:
  virtual ~StoreBasedMemoryRtBits0To3_cccc00011110nnnndddd11111001tttt() {}
};

class StrImmediate_Str_Rule_194_A1_P384
    : public StrImmediate {
 public:
  virtual ~StrImmediate_Str_Rule_194_A1_P384() {}
};

class StrImmediate_Strb_Rule_197_A1_P390
    : public StrImmediate {
 public:
  virtual ~StrImmediate_Strb_Rule_197_A1_P390() {}
};

class StrImmediate_Strh_Rule_207_A1_P410
    : public StrImmediate {
 public:
  virtual ~StrImmediate_Strh_Rule_207_A1_P410() {}
};

class StrImmediateDouble_Strd_Rule_200_A1_P396
    : public StrImmediateDouble {
 public:
  virtual ~StrImmediateDouble_Strd_Rule_200_A1_P396() {}
};

class StrRegister_Str_Rule_195_A1_P386
    : public StrRegister {
 public:
  virtual ~StrRegister_Str_Rule_195_A1_P386() {}
};

class StrRegister_Strb_Rule_198_A1_P392
    : public StrRegister {
 public:
  virtual ~StrRegister_Strb_Rule_198_A1_P392() {}
};

class StrRegister_Strh_Rule_208_A1_P412
    : public StrRegister {
 public:
  virtual ~StrRegister_Strh_Rule_208_A1_P412() {}
};

class StrRegisterDouble_Strd_Rule_201_A1_P398
    : public StrRegisterDouble {
 public:
  virtual ~StrRegisterDouble_Strd_Rule_201_A1_P398() {}
};

class TestIfAddressMasked_Tst_Rule_230_A1_P454
    : public TestIfAddressMasked {
 public:
  virtual ~TestIfAddressMasked_Tst_Rule_230_A1_P454() {}
};

class VfpOp_Vadd_Rule_271_A2_P536
    : public VfpOp {
 public:
  virtual ~VfpOp_Vadd_Rule_271_A2_P536() {}
};

class VfpOp_Vdiv_Rule_301_A1_P590
    : public VfpOp {
 public:
  virtual ~VfpOp_Vdiv_Rule_301_A1_P590() {}
};

class VfpOp_Vm_la_ls_Rule_423_A2_P636
    : public VfpOp {
 public:
  virtual ~VfpOp_Vm_la_ls_Rule_423_A2_P636() {}
};

class VfpOp_Vmul_Rule_338_A2_P664
    : public VfpOp {
 public:
  virtual ~VfpOp_Vmul_Rule_338_A2_P664() {}
};

class VfpOp_Vnm_la_ls_ul_Rule_343_A1_P674
    : public VfpOp {
 public:
  virtual ~VfpOp_Vnm_la_ls_ul_Rule_343_A1_P674() {}
};

class VfpOp_Vnm_la_ls_ul_Rule_343_A2_P674
    : public VfpOp {
 public:
  virtual ~VfpOp_Vnm_la_ls_ul_Rule_343_A2_P674() {}
};

class VfpOp_Vsub_Rule_402_A2_P790
    : public VfpOp {
 public:
  virtual ~VfpOp_Vsub_Rule_402_A2_P790() {}
};

}  // nacl_arm_dec

namespace nacl_arm_test {

/*
 * Define named class decoders for each class decoder.
 * The main purpose of these classes is to introduce
 * instances that are named specifically to the class decoder
 * and/or rule that was used to parse them. This makes testing
 * much easier in that error messages use these named classes
 * to clarify what row in the corresponding table was used
 * to select this decoder. Without these names, debugging the
 * output of the test code would be nearly impossible
 */

class NamedBinary2RegisterBitRange_Bfi_Rule_18_A1_P48
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterBitRange_Bfi_Rule_18_A1_P48()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRange Bfi_Rule_18_A1_P48")
  {}
  virtual ~NamedBinary2RegisterBitRange_Bfi_Rule_18_A1_P48() {}

 private:
  nacl_arm_dec::Binary2RegisterBitRange_Bfi_Rule_18_A1_P48 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRange_Bfi_Rule_18_A1_P48);
};

class NamedBinary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRangeNotRnIsPc Sbfx_Rule_154_A1_P308")
  {}
  virtual ~NamedBinary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308() {}

 private:
  nacl_arm_dec::Binary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308);
};

class NamedBinary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRangeNotRnIsPc Ubfx_Rule_236_A1_P466")
  {}
  virtual ~NamedBinary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466() {}

 private:
  nacl_arm_dec::Binary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466);
};

class NamedBinary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmedShiftedTest Cmn_Rule_33_A1_P76")
  {}
  virtual ~NamedBinary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76() {}

 private:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76);
};

class NamedBinary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmedShiftedTest Cmp_Rule_36_A1_P82")
  {}
  virtual ~NamedBinary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82() {}

 private:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82);
};

class NamedBinary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmedShiftedTest Teq_Rule_228_A1_P450")
  {}
  virtual ~NamedBinary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450() {}

 private:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450);
};

class NamedBinary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmedShiftedTest Tst_Rule_231_A1_P456")
  {}
  virtual ~NamedBinary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456() {}

 private:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456);
};

class NamedBinary2RegisterImmediateOp_Adc_Rule_6_A1_P14
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_Adc_Rule_6_A1_P14()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp Adc_Rule_6_A1_P14")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_Adc_Rule_6_A1_P14() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_Adc_Rule_6_A1_P14);
};

class NamedBinary2RegisterImmediateOp_Add_Rule_5_A1_P22
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_Add_Rule_5_A1_P22()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp Add_Rule_5_A1_P22")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_Add_Rule_5_A1_P22() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_Add_Rule_5_A1_P22 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_Add_Rule_5_A1_P22);
};

class NamedBinary2RegisterImmediateOp_And_Rule_11_A1_P34
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_And_Rule_11_A1_P34()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp And_Rule_11_A1_P34")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_And_Rule_11_A1_P34() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_And_Rule_11_A1_P34 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_And_Rule_11_A1_P34);
};

class NamedBinary2RegisterImmediateOp_Eor_Rule_44_A1_P94
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_Eor_Rule_44_A1_P94()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp Eor_Rule_44_A1_P94")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_Eor_Rule_44_A1_P94() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_Eor_Rule_44_A1_P94);
};

class NamedBinary2RegisterImmediateOp_Orr_Rule_113_A1_P228
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_Orr_Rule_113_A1_P228()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp Orr_Rule_113_A1_P228")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_Orr_Rule_113_A1_P228() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_Orr_Rule_113_A1_P228);
};

class NamedBinary2RegisterImmediateOp_Rsb_Rule_142_A1_P284
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_Rsb_Rule_142_A1_P284()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp Rsb_Rule_142_A1_P284")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_Rsb_Rule_142_A1_P284() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_Rsb_Rule_142_A1_P284);
};

class NamedBinary2RegisterImmediateOp_Rsc_Rule_145_A1_P290
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_Rsc_Rule_145_A1_P290()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp Rsc_Rule_145_A1_P290")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_Rsc_Rule_145_A1_P290() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_Rsc_Rule_145_A1_P290);
};

class NamedBinary2RegisterImmediateOp_Sbc_Rule_151_A1_P302
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_Sbc_Rule_151_A1_P302()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp Sbc_Rule_151_A1_P302")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_Sbc_Rule_151_A1_P302() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_Sbc_Rule_151_A1_P302);
};

class NamedBinary2RegisterImmediateOp_Sub_Rule_212_A1_P420
    : public NamedClassDecoder {
 public:
  inline NamedBinary2RegisterImmediateOp_Sub_Rule_212_A1_P420()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp Sub_Rule_212_A1_P420")
  {}
  virtual ~NamedBinary2RegisterImmediateOp_Sub_Rule_212_A1_P420() {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_Sub_Rule_212_A1_P420);
};

class NamedBinary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp Adc_Rule_2_A1_P16")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16);
};

class NamedBinary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp Add_Rule_6_A1_P24")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24);
};

class NamedBinary3RegisterImmedShiftedOp_And_Rule_7_A1_P36
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_And_Rule_7_A1_P36()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp And_Rule_7_A1_P36")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_And_Rule_7_A1_P36() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_And_Rule_7_A1_P36);
};

class NamedBinary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp Bic_Rule_20_A1_P52")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52);
};

class NamedBinary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp Eor_Rule_45_A1_P96")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96);
};

class NamedBinary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp Orr_Rule_114_A1_P230")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230);
};

class NamedBinary3RegisterImmedShiftedOp_Rsb_Rule_143_P286
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_Rsb_Rule_143_P286()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp Rsb_Rule_143_P286")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_Rsb_Rule_143_P286() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_Rsb_Rule_143_P286);
};

class NamedBinary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp Rsc_Rule_146_A1_P292")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292);
};

class NamedBinary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp Sbc_Rule_152_A1_P304")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304);
};

class NamedBinary3RegisterImmedShiftedOp_SubRule_213_A1_P422
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterImmedShiftedOp_SubRule_213_A1_P422()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOp SubRule_213_A1_P422")
  {}
  virtual ~NamedBinary3RegisterImmedShiftedOp_SubRule_213_A1_P422() {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOp_SubRule_213_A1_P422 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOp_SubRule_213_A1_P422);
};

class NamedBinary3RegisterOp_Asr_Rule_15_A1_P42
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOp_Asr_Rule_15_A1_P42()
    : NamedClassDecoder(decoder_, "Binary3RegisterOp Asr_Rule_15_A1_P42")
  {}
  virtual ~NamedBinary3RegisterOp_Asr_Rule_15_A1_P42() {}

 private:
  nacl_arm_dec::Binary3RegisterOp_Asr_Rule_15_A1_P42 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOp_Asr_Rule_15_A1_P42);
};

class NamedBinary3RegisterOp_Lsl_Rule_89_A1_P180
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOp_Lsl_Rule_89_A1_P180()
    : NamedClassDecoder(decoder_, "Binary3RegisterOp Lsl_Rule_89_A1_P180")
  {}
  virtual ~NamedBinary3RegisterOp_Lsl_Rule_89_A1_P180() {}

 private:
  nacl_arm_dec::Binary3RegisterOp_Lsl_Rule_89_A1_P180 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOp_Lsl_Rule_89_A1_P180);
};

class NamedBinary3RegisterOp_Lsr_Rule_91_A1_P184
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOp_Lsr_Rule_91_A1_P184()
    : NamedClassDecoder(decoder_, "Binary3RegisterOp Lsr_Rule_91_A1_P184")
  {}
  virtual ~NamedBinary3RegisterOp_Lsr_Rule_91_A1_P184() {}

 private:
  nacl_arm_dec::Binary3RegisterOp_Lsr_Rule_91_A1_P184 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOp_Lsr_Rule_91_A1_P184);
};

class NamedBinary3RegisterOp_Ror_Rule_140_A1_P280
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOp_Ror_Rule_140_A1_P280()
    : NamedClassDecoder(decoder_, "Binary3RegisterOp Ror_Rule_140_A1_P280")
  {}
  virtual ~NamedBinary3RegisterOp_Ror_Rule_140_A1_P280() {}

 private:
  nacl_arm_dec::Binary3RegisterOp_Ror_Rule_140_A1_P280 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOp_Ror_Rule_140_A1_P280);
};

class NamedBinary3RegisterOpAltA_Mul_Rule_105_A1_P212
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltA_Mul_Rule_105_A1_P212()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Mul_Rule_105_A1_P212")
  {}
  virtual ~NamedBinary3RegisterOpAltA_Mul_Rule_105_A1_P212() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Mul_Rule_105_A1_P212 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Mul_Rule_105_A1_P212);
};

class NamedBinary3RegisterOpAltA_Smmul_Rule_176_P350
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltA_Smmul_Rule_176_P350()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smmul_Rule_176_P350")
  {}
  virtual ~NamedBinary3RegisterOpAltA_Smmul_Rule_176_P350() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smmul_Rule_176_P350 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smmul_Rule_176_P350);
};

class NamedBinary3RegisterOpAltA_Smuad_Rule_177_P352
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltA_Smuad_Rule_177_P352()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smuad_Rule_177_P352")
  {}
  virtual ~NamedBinary3RegisterOpAltA_Smuad_Rule_177_P352() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smuad_Rule_177_P352 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smuad_Rule_177_P352);
};

class NamedBinary3RegisterOpAltA_Smulwx_Rule_180_A1_P358
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltA_Smulwx_Rule_180_A1_P358()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smulwx_Rule_180_A1_P358")
  {}
  virtual ~NamedBinary3RegisterOpAltA_Smulwx_Rule_180_A1_P358() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smulwx_Rule_180_A1_P358);
};

class NamedBinary3RegisterOpAltA_Smulxx_Rule_178_P354
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltA_Smulxx_Rule_178_P354()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smulxx_Rule_178_P354")
  {}
  virtual ~NamedBinary3RegisterOpAltA_Smulxx_Rule_178_P354() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smulxx_Rule_178_P354 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smulxx_Rule_178_P354);
};

class NamedBinary3RegisterOpAltA_Smusd_Rule_181_P360
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltA_Smusd_Rule_181_P360()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smusd_Rule_181_P360")
  {}
  virtual ~NamedBinary3RegisterOpAltA_Smusd_Rule_181_P360() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smusd_Rule_181_P360 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smusd_Rule_181_P360);
};

class NamedBinary3RegisterOpAltA_Usad8_Rule_253_A1_P500
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltA_Usad8_Rule_253_A1_P500()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Usad8_Rule_253_A1_P500")
  {}
  virtual ~NamedBinary3RegisterOpAltA_Usad8_Rule_253_A1_P500() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Usad8_Rule_253_A1_P500);
};

class NamedBinary3RegisterOpAltB_Qadd16_Rule_125_A1_P252
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qadd16_Rule_125_A1_P252()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qadd16_Rule_125_A1_P252")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qadd16_Rule_125_A1_P252() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qadd16_Rule_125_A1_P252 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qadd16_Rule_125_A1_P252);
};

class NamedBinary3RegisterOpAltB_Qadd8_Rule_126_A1_P254
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qadd8_Rule_126_A1_P254()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qadd8_Rule_126_A1_P254")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qadd8_Rule_126_A1_P254() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qadd8_Rule_126_A1_P254 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qadd8_Rule_126_A1_P254);
};

class NamedBinary3RegisterOpAltB_Qadd_Rule_124_A1_P250
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qadd_Rule_124_A1_P250()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qadd_Rule_124_A1_P250")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qadd_Rule_124_A1_P250() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qadd_Rule_124_A1_P250 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qadd_Rule_124_A1_P250);
};

class NamedBinary3RegisterOpAltB_Qasx_Rule_127_A1_P256
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qasx_Rule_127_A1_P256()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qasx_Rule_127_A1_P256")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qasx_Rule_127_A1_P256() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qasx_Rule_127_A1_P256 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qasx_Rule_127_A1_P256);
};

class NamedBinary3RegisterOpAltB_Qdadd_Rule_128_A1_P258
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qdadd_Rule_128_A1_P258()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qdadd_Rule_128_A1_P258")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qdadd_Rule_128_A1_P258() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qdadd_Rule_128_A1_P258 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qdadd_Rule_128_A1_P258);
};

class NamedBinary3RegisterOpAltB_Qdsub_Rule_129_A1_P260
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qdsub_Rule_129_A1_P260()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qdsub_Rule_129_A1_P260")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qdsub_Rule_129_A1_P260() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qdsub_Rule_129_A1_P260 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qdsub_Rule_129_A1_P260);
};

class NamedBinary3RegisterOpAltB_Qsax_Rule_130_A1_P262
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qsax_Rule_130_A1_P262()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qsax_Rule_130_A1_P262")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qsax_Rule_130_A1_P262() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qsax_Rule_130_A1_P262 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qsax_Rule_130_A1_P262);
};

class NamedBinary3RegisterOpAltB_Qsub16_Rule_132_A1_P266
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qsub16_Rule_132_A1_P266()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qsub16_Rule_132_A1_P266")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qsub16_Rule_132_A1_P266() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qsub16_Rule_132_A1_P266 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qsub16_Rule_132_A1_P266);
};

class NamedBinary3RegisterOpAltB_Qsub8_Rule_133_A1_P268
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qsub8_Rule_133_A1_P268()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qsub8_Rule_133_A1_P268")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qsub8_Rule_133_A1_P268() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qsub8_Rule_133_A1_P268 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qsub8_Rule_133_A1_P268);
};

class NamedBinary3RegisterOpAltB_Qsub_Rule_131_A1_P264
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Qsub_Rule_131_A1_P264()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Qsub_Rule_131_A1_P264")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Qsub_Rule_131_A1_P264() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Qsub_Rule_131_A1_P264 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Qsub_Rule_131_A1_P264);
};

class NamedBinary3RegisterOpAltB_Sadd16_Rule_148_A1_P296
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Sadd16_Rule_148_A1_P296()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Sadd16_Rule_148_A1_P296")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Sadd16_Rule_148_A1_P296() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Sadd16_Rule_148_A1_P296 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Sadd16_Rule_148_A1_P296);
};

class NamedBinary3RegisterOpAltB_Sasx_Rule_150_A1_P300
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Sasx_Rule_150_A1_P300()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Sasx_Rule_150_A1_P300")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Sasx_Rule_150_A1_P300() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Sasx_Rule_150_A1_P300 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Sasx_Rule_150_A1_P300);
};

class NamedBinary3RegisterOpAltB_Ssad8_Rule_149_A1_P298
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Ssad8_Rule_149_A1_P298()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Ssad8_Rule_149_A1_P298")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Ssad8_Rule_149_A1_P298() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Ssad8_Rule_149_A1_P298 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Ssad8_Rule_149_A1_P298);
};

class NamedBinary3RegisterOpAltB_Ssax_Rule_185_A1_P366
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Ssax_Rule_185_A1_P366()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Ssax_Rule_185_A1_P366")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Ssax_Rule_185_A1_P366() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Ssax_Rule_185_A1_P366 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Ssax_Rule_185_A1_P366);
};

class NamedBinary3RegisterOpAltB_Ssub16_Rule_186_A1_P368
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Ssub16_Rule_186_A1_P368()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Ssub16_Rule_186_A1_P368")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Ssub16_Rule_186_A1_P368() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Ssub16_Rule_186_A1_P368 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Ssub16_Rule_186_A1_P368);
};

class NamedBinary3RegisterOpAltB_Ssub8_Rule_187_A1_P370
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Ssub8_Rule_187_A1_P370()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Ssub8_Rule_187_A1_P370")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Ssub8_Rule_187_A1_P370() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Ssub8_Rule_187_A1_P370 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Ssub8_Rule_187_A1_P370);
};

class NamedBinary3RegisterOpAltB_Uadd16_Rule_233_A1_P460
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uadd16_Rule_233_A1_P460()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uadd16_Rule_233_A1_P460")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uadd16_Rule_233_A1_P460() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uadd16_Rule_233_A1_P460 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uadd16_Rule_233_A1_P460);
};

class NamedBinary3RegisterOpAltB_Uadd8_Rule_234_A1_P462
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uadd8_Rule_234_A1_P462()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uadd8_Rule_234_A1_P462")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uadd8_Rule_234_A1_P462() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uadd8_Rule_234_A1_P462 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uadd8_Rule_234_A1_P462);
};

class NamedBinary3RegisterOpAltB_Uasx_Rule_235_A1_P464
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uasx_Rule_235_A1_P464()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uasx_Rule_235_A1_P464")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uasx_Rule_235_A1_P464() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uasx_Rule_235_A1_P464 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uasx_Rule_235_A1_P464);
};

class NamedBinary3RegisterOpAltB_Uqadd16_Rule_247_A1_P488
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uqadd16_Rule_247_A1_P488()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uqadd16_Rule_247_A1_P488")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uqadd16_Rule_247_A1_P488() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uqadd16_Rule_247_A1_P488 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uqadd16_Rule_247_A1_P488);
};

class NamedBinary3RegisterOpAltB_Uqadd8_Rule_248_A1_P490
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uqadd8_Rule_248_A1_P490()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uqadd8_Rule_248_A1_P490")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uqadd8_Rule_248_A1_P490() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uqadd8_Rule_248_A1_P490 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uqadd8_Rule_248_A1_P490);
};

class NamedBinary3RegisterOpAltB_Uqasx_Rule_249_A1_P492
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uqasx_Rule_249_A1_P492()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uqasx_Rule_249_A1_P492")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uqasx_Rule_249_A1_P492() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uqasx_Rule_249_A1_P492 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uqasx_Rule_249_A1_P492);
};

class NamedBinary3RegisterOpAltB_Uqsax_Rule_250_A1_P494
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uqsax_Rule_250_A1_P494()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uqsax_Rule_250_A1_P494")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uqsax_Rule_250_A1_P494() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uqsax_Rule_250_A1_P494 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uqsax_Rule_250_A1_P494);
};

class NamedBinary3RegisterOpAltB_Uqsub16_Rule_251_A1_P496
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uqsub16_Rule_251_A1_P496()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uqsub16_Rule_251_A1_P496")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uqsub16_Rule_251_A1_P496() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uqsub16_Rule_251_A1_P496 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uqsub16_Rule_251_A1_P496);
};

class NamedBinary3RegisterOpAltB_Uqsub8_Rule_252_A1_P498
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Uqsub8_Rule_252_A1_P498()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Uqsub8_Rule_252_A1_P498")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Uqsub8_Rule_252_A1_P498() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Uqsub8_Rule_252_A1_P498 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Uqsub8_Rule_252_A1_P498);
};

class NamedBinary3RegisterOpAltB_Usax_Rule_257_A1_P508
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Usax_Rule_257_A1_P508()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Usax_Rule_257_A1_P508")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Usax_Rule_257_A1_P508() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Usax_Rule_257_A1_P508 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Usax_Rule_257_A1_P508);
};

class NamedBinary3RegisterOpAltB_Usub16_Rule_258_A1_P510
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Usub16_Rule_258_A1_P510()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Usub16_Rule_258_A1_P510")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Usub16_Rule_258_A1_P510() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Usub16_Rule_258_A1_P510 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Usub16_Rule_258_A1_P510);
};

class NamedBinary3RegisterOpAltB_Usub8_Rule_259_A1_P512
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltB_Usub8_Rule_259_A1_P512()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltB Usub8_Rule_259_A1_P512")
  {}
  virtual ~NamedBinary3RegisterOpAltB_Usub8_Rule_259_A1_P512() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltB_Usub8_Rule_259_A1_P512 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltB_Usub8_Rule_259_A1_P512);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shadd16_Rule_159_A1_P318")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shadd8_Rule_160_A1_P320")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shasx_Rule_161_A1_P322")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shsax_Rule_162_A1_P324")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shsub16_Rule_163_A1_P326")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shsub8_Rule_164_A1_P328")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhadd16_Rule_238_A1_P470")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhadd8_Rule_239_A1_P472")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhasx_Rule_240_A1_P474")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhsax_Rule_241_A1_P476")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhsub16_Rule_242_A1_P478")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhsub8_Rule_243_A1_P480")
  {}
  virtual ~NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480() {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480);
};

class NamedBinary3RegisterShiftedTest_Cmn_Rule_34_A1_P78
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterShiftedTest_Cmn_Rule_34_A1_P78()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest Cmn_Rule_34_A1_P78")
  {}
  virtual ~NamedBinary3RegisterShiftedTest_Cmn_Rule_34_A1_P78() {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedTest_Cmn_Rule_34_A1_P78);
};

class NamedBinary3RegisterShiftedTest_Cmp_Rule_37_A1_P84
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterShiftedTest_Cmp_Rule_37_A1_P84()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest Cmp_Rule_37_A1_P84")
  {}
  virtual ~NamedBinary3RegisterShiftedTest_Cmp_Rule_37_A1_P84() {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedTest_Cmp_Rule_37_A1_P84);
};

class NamedBinary3RegisterShiftedTest_Teq_Rule_229_A1_P452
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterShiftedTest_Teq_Rule_229_A1_P452()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest Teq_Rule_229_A1_P452")
  {}
  virtual ~NamedBinary3RegisterShiftedTest_Teq_Rule_229_A1_P452() {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedTest_Teq_Rule_229_A1_P452);
};

class NamedBinary3RegisterShiftedTest_Tst_Rule_232_A1_P458
    : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterShiftedTest_Tst_Rule_232_A1_P458()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest Tst_Rule_232_A1_P458")
  {}
  virtual ~NamedBinary3RegisterShiftedTest_Tst_Rule_232_A1_P458() {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedTest_Tst_Rule_232_A1_P458);
};

class NamedBinary4RegisterDualOp_Mla_Rule_94_A1_P190
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Mla_Rule_94_A1_P190()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Mla_Rule_94_A1_P190")
  {}
  virtual ~NamedBinary4RegisterDualOp_Mla_Rule_94_A1_P190() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Mla_Rule_94_A1_P190 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Mla_Rule_94_A1_P190);
};

class NamedBinary4RegisterDualOp_Mls_Rule_95_A1_P192
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Mls_Rule_95_A1_P192()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Mls_Rule_95_A1_P192")
  {}
  virtual ~NamedBinary4RegisterDualOp_Mls_Rule_95_A1_P192() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Mls_Rule_95_A1_P192 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Mls_Rule_95_A1_P192);
};

class NamedBinary4RegisterDualOp_Smlad_Rule_167_P332
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Smlad_Rule_167_P332()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smlad_Rule_167_P332")
  {}
  virtual ~NamedBinary4RegisterDualOp_Smlad_Rule_167_P332() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smlad_Rule_167_P332 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smlad_Rule_167_P332);
};

class NamedBinary4RegisterDualOp_Smlawx_Rule_171_A1_340
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Smlawx_Rule_171_A1_340()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smlawx_Rule_171_A1_340")
  {}
  virtual ~NamedBinary4RegisterDualOp_Smlawx_Rule_171_A1_340() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smlawx_Rule_171_A1_340 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smlawx_Rule_171_A1_340);
};

class NamedBinary4RegisterDualOp_Smlaxx_Rule_166_A1_P330
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Smlaxx_Rule_166_A1_P330()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smlaxx_Rule_166_A1_P330")
  {}
  virtual ~NamedBinary4RegisterDualOp_Smlaxx_Rule_166_A1_P330() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smlaxx_Rule_166_A1_P330);
};

class NamedBinary4RegisterDualOp_Smlsd_Rule_172_P342
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Smlsd_Rule_172_P342()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smlsd_Rule_172_P342")
  {}
  virtual ~NamedBinary4RegisterDualOp_Smlsd_Rule_172_P342() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smlsd_Rule_172_P342 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smlsd_Rule_172_P342);
};

class NamedBinary4RegisterDualOp_Smmla_Rule_174_P346
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Smmla_Rule_174_P346()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smmla_Rule_174_P346")
  {}
  virtual ~NamedBinary4RegisterDualOp_Smmla_Rule_174_P346() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smmla_Rule_174_P346 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smmla_Rule_174_P346);
};

class NamedBinary4RegisterDualOp_Smmls_Rule_175_P348
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Smmls_Rule_175_P348()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smmls_Rule_175_P348")
  {}
  virtual ~NamedBinary4RegisterDualOp_Smmls_Rule_175_P348() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smmls_Rule_175_P348 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smmls_Rule_175_P348);
};

class NamedBinary4RegisterDualOp_Usda8_Rule_254_A1_P502
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualOp_Usda8_Rule_254_A1_P502()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Usda8_Rule_254_A1_P502")
  {}
  virtual ~NamedBinary4RegisterDualOp_Usda8_Rule_254_A1_P502() {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Usda8_Rule_254_A1_P502 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Usda8_Rule_254_A1_P502);
};

class NamedBinary4RegisterDualResult_Smlal_Rule_168_A1_P334
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualResult_Smlal_Rule_168_A1_P334()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Smlal_Rule_168_A1_P334")
  {}
  virtual ~NamedBinary4RegisterDualResult_Smlal_Rule_168_A1_P334() {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Smlal_Rule_168_A1_P334 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Smlal_Rule_168_A1_P334);
};

class NamedBinary4RegisterDualResult_Smlald_Rule_170_P336
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualResult_Smlald_Rule_170_P336()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Smlald_Rule_170_P336")
  {}
  virtual ~NamedBinary4RegisterDualResult_Smlald_Rule_170_P336() {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Smlald_Rule_170_P336 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Smlald_Rule_170_P336);
};

class NamedBinary4RegisterDualResult_Smlalxx_Rule_169_A1_P336
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualResult_Smlalxx_Rule_169_A1_P336()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Smlalxx_Rule_169_A1_P336")
  {}
  virtual ~NamedBinary4RegisterDualResult_Smlalxx_Rule_169_A1_P336() {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Smlalxx_Rule_169_A1_P336);
};

class NamedBinary4RegisterDualResult_Smlsld_Rule_173_P344
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualResult_Smlsld_Rule_173_P344()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Smlsld_Rule_173_P344")
  {}
  virtual ~NamedBinary4RegisterDualResult_Smlsld_Rule_173_P344() {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Smlsld_Rule_173_P344 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Smlsld_Rule_173_P344);
};

class NamedBinary4RegisterDualResult_Smull_Rule_179_A1_P356
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualResult_Smull_Rule_179_A1_P356()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Smull_Rule_179_A1_P356")
  {}
  virtual ~NamedBinary4RegisterDualResult_Smull_Rule_179_A1_P356() {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Smull_Rule_179_A1_P356 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Smull_Rule_179_A1_P356);
};

class NamedBinary4RegisterDualResult_Umaal_Rule_244_A1_P482
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualResult_Umaal_Rule_244_A1_P482()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Umaal_Rule_244_A1_P482")
  {}
  virtual ~NamedBinary4RegisterDualResult_Umaal_Rule_244_A1_P482() {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Umaal_Rule_244_A1_P482 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Umaal_Rule_244_A1_P482);
};

class NamedBinary4RegisterDualResult_Umlal_Rule_245_A1_P484
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualResult_Umlal_Rule_245_A1_P484()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Umlal_Rule_245_A1_P484")
  {}
  virtual ~NamedBinary4RegisterDualResult_Umlal_Rule_245_A1_P484() {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Umlal_Rule_245_A1_P484 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Umlal_Rule_245_A1_P484);
};

class NamedBinary4RegisterDualResult_Umull_Rule_246_A1_P486
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterDualResult_Umull_Rule_246_A1_P486()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Umull_Rule_246_A1_P486")
  {}
  virtual ~NamedBinary4RegisterDualResult_Umull_Rule_246_A1_P486() {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Umull_Rule_246_A1_P486 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Umull_Rule_246_A1_P486);
};

class NamedBinary4RegisterShiftedOp_Adc_Rule_3_A1_P18
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Adc_Rule_3_A1_P18()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Adc_Rule_3_A1_P18")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Adc_Rule_3_A1_P18() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Adc_Rule_3_A1_P18);
};

class NamedBinary4RegisterShiftedOp_Add_Rule_7_A1_P26
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Add_Rule_7_A1_P26()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Add_Rule_7_A1_P26")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Add_Rule_7_A1_P26() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Add_Rule_7_A1_P26 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Add_Rule_7_A1_P26);
};

class NamedBinary4RegisterShiftedOp_And_Rule_13_A1_P38
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_And_Rule_13_A1_P38()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp And_Rule_13_A1_P38")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_And_Rule_13_A1_P38() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_And_Rule_13_A1_P38 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_And_Rule_13_A1_P38);
};

class NamedBinary4RegisterShiftedOp_Bic_Rule_21_A1_P54
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Bic_Rule_21_A1_P54()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Bic_Rule_21_A1_P54")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Bic_Rule_21_A1_P54() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Bic_Rule_21_A1_P54);
};

class NamedBinary4RegisterShiftedOp_Eor_Rule_46_A1_P98
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Eor_Rule_46_A1_P98()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Eor_Rule_46_A1_P98")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Eor_Rule_46_A1_P98() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Eor_Rule_46_A1_P98);
};

class NamedBinary4RegisterShiftedOp_Orr_Rule_115_A1_P212
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Orr_Rule_115_A1_P212()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Orr_Rule_115_A1_P212")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Orr_Rule_115_A1_P212() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Orr_Rule_115_A1_P212);
};

class NamedBinary4RegisterShiftedOp_Rsb_Rule_144_A1_P288
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Rsb_Rule_144_A1_P288()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Rsb_Rule_144_A1_P288")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Rsb_Rule_144_A1_P288() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Rsb_Rule_144_A1_P288);
};

class NamedBinary4RegisterShiftedOp_Rsc_Rule_147_A1_P294
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Rsc_Rule_147_A1_P294()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Rsc_Rule_147_A1_P294")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Rsc_Rule_147_A1_P294() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Rsc_Rule_147_A1_P294);
};

class NamedBinary4RegisterShiftedOp_Sbc_Rule_153_A1_P306
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Sbc_Rule_153_A1_P306()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Sbc_Rule_153_A1_P306")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Sbc_Rule_153_A1_P306() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Sbc_Rule_153_A1_P306);
};

class NamedBinary4RegisterShiftedOp_Sub_Rule_214_A1_P424
    : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp_Sub_Rule_214_A1_P424()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp Sub_Rule_214_A1_P424")
  {}
  virtual ~NamedBinary4RegisterShiftedOp_Sub_Rule_214_A1_P424() {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_Sub_Rule_214_A1_P424);
};

class NamedBinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74
    : public NamedClassDecoder {
 public:
  inline NamedBinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74()
    : NamedClassDecoder(decoder_, "BinaryRegisterImmediateTest Cmn_Rule_32_A1_P74")
  {}
  virtual ~NamedBinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74() {}

 private:
  nacl_arm_dec::BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74);
};

class NamedBinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80
    : public NamedClassDecoder {
 public:
  inline NamedBinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80()
    : NamedClassDecoder(decoder_, "BinaryRegisterImmediateTest Cmp_Rule_35_A1_P80")
  {}
  virtual ~NamedBinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80() {}

 private:
  nacl_arm_dec::BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80);
};

class NamedBinaryRegisterImmediateTest_Teq_Rule_227_A1_P448
    : public NamedClassDecoder {
 public:
  inline NamedBinaryRegisterImmediateTest_Teq_Rule_227_A1_P448()
    : NamedClassDecoder(decoder_, "BinaryRegisterImmediateTest Teq_Rule_227_A1_P448")
  {}
  virtual ~NamedBinaryRegisterImmediateTest_Teq_Rule_227_A1_P448() {}

 private:
  nacl_arm_dec::BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinaryRegisterImmediateTest_Teq_Rule_227_A1_P448);
};

class NamedBranch_None
    : public NamedClassDecoder {
 public:
  inline NamedBranch_None()
    : NamedClassDecoder(decoder_, "Branch None")
  {}
  virtual ~NamedBranch_None() {}

 private:
  nacl_arm_dec::Branch_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranch_None);
};

class NamedBranchToRegister_Blx_Rule_24_A1_P60
    : public NamedClassDecoder {
 public:
  inline NamedBranchToRegister_Blx_Rule_24_A1_P60()
    : NamedClassDecoder(decoder_, "BranchToRegister Blx_Rule_24_A1_P60")
  {}
  virtual ~NamedBranchToRegister_Blx_Rule_24_A1_P60() {}

 private:
  nacl_arm_dec::BranchToRegister_Blx_Rule_24_A1_P60 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchToRegister_Blx_Rule_24_A1_P60);
};

class NamedBranchToRegister_Bx_Rule_25_A1_P62
    : public NamedClassDecoder {
 public:
  inline NamedBranchToRegister_Bx_Rule_25_A1_P62()
    : NamedClassDecoder(decoder_, "BranchToRegister Bx_Rule_25_A1_P62")
  {}
  virtual ~NamedBranchToRegister_Bx_Rule_25_A1_P62() {}

 private:
  nacl_arm_dec::BranchToRegister_Bx_Rule_25_A1_P62 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchToRegister_Bx_Rule_25_A1_P62);
};

class NamedBreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56
    : public NamedClassDecoder {
 public:
  inline NamedBreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56()
    : NamedClassDecoder(decoder_, "BreakPointAndConstantPoolHead Bkpt_Rule_22_A1_P56")
  {}
  virtual ~NamedBreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56() {}

 private:
  nacl_arm_dec::BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56);
};

class NamedCondNop_Dbg_Rule_40_A1_P88
    : public NamedClassDecoder {
 public:
  inline NamedCondNop_Dbg_Rule_40_A1_P88()
    : NamedClassDecoder(decoder_, "CondNop Dbg_Rule_40_A1_P88")
  {}
  virtual ~NamedCondNop_Dbg_Rule_40_A1_P88() {}

 private:
  nacl_arm_dec::CondNop_Dbg_Rule_40_A1_P88 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondNop_Dbg_Rule_40_A1_P88);
};

class NamedCondNop_Nop_Rule_110_A1_P222
    : public NamedClassDecoder {
 public:
  inline NamedCondNop_Nop_Rule_110_A1_P222()
    : NamedClassDecoder(decoder_, "CondNop Nop_Rule_110_A1_P222")
  {}
  virtual ~NamedCondNop_Nop_Rule_110_A1_P222() {}

 private:
  nacl_arm_dec::CondNop_Nop_Rule_110_A1_P222 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondNop_Nop_Rule_110_A1_P222);
};

class NamedCondNop_Yield_Rule_413_A1_P812
    : public NamedClassDecoder {
 public:
  inline NamedCondNop_Yield_Rule_413_A1_P812()
    : NamedClassDecoder(decoder_, "CondNop Yield_Rule_413_A1_P812")
  {}
  virtual ~NamedCondNop_Yield_Rule_413_A1_P812() {}

 private:
  nacl_arm_dec::CondNop_Yield_Rule_413_A1_P812 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondNop_Yield_Rule_413_A1_P812);
};

class NamedCondVfpOp_Vadd_Rule_271_A2_P536
    : public NamedClassDecoder {
 public:
  inline NamedCondVfpOp_Vadd_Rule_271_A2_P536()
    : NamedClassDecoder(decoder_, "CondVfpOp Vadd_Rule_271_A2_P536")
  {}
  virtual ~NamedCondVfpOp_Vadd_Rule_271_A2_P536() {}

 private:
  nacl_arm_dec::CondVfpOp_Vadd_Rule_271_A2_P536 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vadd_Rule_271_A2_P536);
};

class NamedCondVfpOp_Vdiv_Rule_301_A1_P590
    : public NamedClassDecoder {
 public:
  inline NamedCondVfpOp_Vdiv_Rule_301_A1_P590()
    : NamedClassDecoder(decoder_, "CondVfpOp Vdiv_Rule_301_A1_P590")
  {}
  virtual ~NamedCondVfpOp_Vdiv_Rule_301_A1_P590() {}

 private:
  nacl_arm_dec::CondVfpOp_Vdiv_Rule_301_A1_P590 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vdiv_Rule_301_A1_P590);
};

class NamedCondVfpOp_Vm_la_ls_Rule_423_A2_P636
    : public NamedClassDecoder {
 public:
  inline NamedCondVfpOp_Vm_la_ls_Rule_423_A2_P636()
    : NamedClassDecoder(decoder_, "CondVfpOp Vm_la_ls_Rule_423_A2_P636")
  {}
  virtual ~NamedCondVfpOp_Vm_la_ls_Rule_423_A2_P636() {}

 private:
  nacl_arm_dec::CondVfpOp_Vm_la_ls_Rule_423_A2_P636 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vm_la_ls_Rule_423_A2_P636);
};

class NamedCondVfpOp_Vmul_Rule_338_A2_P664
    : public NamedClassDecoder {
 public:
  inline NamedCondVfpOp_Vmul_Rule_338_A2_P664()
    : NamedClassDecoder(decoder_, "CondVfpOp Vmul_Rule_338_A2_P664")
  {}
  virtual ~NamedCondVfpOp_Vmul_Rule_338_A2_P664() {}

 private:
  nacl_arm_dec::CondVfpOp_Vmul_Rule_338_A2_P664 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vmul_Rule_338_A2_P664);
};

class NamedCondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674
    : public NamedClassDecoder {
 public:
  inline NamedCondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674()
    : NamedClassDecoder(decoder_, "CondVfpOp Vnm_la_ls_ul_Rule_343_A1_P674")
  {}
  virtual ~NamedCondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674() {}

 private:
  nacl_arm_dec::CondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674);
};

class NamedCondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674
    : public NamedClassDecoder {
 public:
  inline NamedCondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674()
    : NamedClassDecoder(decoder_, "CondVfpOp Vnm_la_ls_ul_Rule_343_A2_P674")
  {}
  virtual ~NamedCondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674() {}

 private:
  nacl_arm_dec::CondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674);
};

class NamedCondVfpOp_Vsub_Rule_402_A2_P790
    : public NamedClassDecoder {
 public:
  inline NamedCondVfpOp_Vsub_Rule_402_A2_P790()
    : NamedClassDecoder(decoder_, "CondVfpOp Vsub_Rule_402_A2_P790")
  {}
  virtual ~NamedCondVfpOp_Vsub_Rule_402_A2_P790() {}

 private:
  nacl_arm_dec::CondVfpOp_Vsub_Rule_402_A2_P790 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vsub_Rule_402_A2_P790);
};

class NamedCoprocessorOp_None
    : public NamedClassDecoder {
 public:
  inline NamedCoprocessorOp_None()
    : NamedClassDecoder(decoder_, "CoprocessorOp None")
  {}
  virtual ~NamedCoprocessorOp_None() {}

 private:
  nacl_arm_dec::CoprocessorOp_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCoprocessorOp_None);
};

class NamedDataProc_None
    : public NamedClassDecoder {
 public:
  inline NamedDataProc_None()
    : NamedClassDecoder(decoder_, "DataProc None")
  {}
  virtual ~NamedDataProc_None() {}

 private:
  nacl_arm_dec::DataProc_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDataProc_None);
};

class NamedDeprecated_None
    : public NamedClassDecoder {
 public:
  inline NamedDeprecated_None()
    : NamedClassDecoder(decoder_, "Deprecated None")
  {}
  virtual ~NamedDeprecated_None() {}

 private:
  nacl_arm_dec::Deprecated_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDeprecated_None);
};

class NamedEffectiveNoOp_None
    : public NamedClassDecoder {
 public:
  inline NamedEffectiveNoOp_None()
    : NamedClassDecoder(decoder_, "EffectiveNoOp None")
  {}
  virtual ~NamedEffectiveNoOp_None() {}

 private:
  nacl_arm_dec::EffectiveNoOp_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedEffectiveNoOp_None);
};

class NamedForbidden_None
    : public NamedClassDecoder {
 public:
  inline NamedForbidden_None()
    : NamedClassDecoder(decoder_, "Forbidden None")
  {}
  virtual ~NamedForbidden_None() {}

 private:
  nacl_arm_dec::Forbidden_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_None);
};

class NamedForbiddenCondNop_Bxj_Rule_26_A1_P64
    : public NamedClassDecoder {
 public:
  inline NamedForbiddenCondNop_Bxj_Rule_26_A1_P64()
    : NamedClassDecoder(decoder_, "ForbiddenCondNop Bxj_Rule_26_A1_P64")
  {}
  virtual ~NamedForbiddenCondNop_Bxj_Rule_26_A1_P64() {}

 private:
  nacl_arm_dec::ForbiddenCondNop_Bxj_Rule_26_A1_P64 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondNop_Bxj_Rule_26_A1_P64);
};

class NamedForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12
    : public NamedClassDecoder {
 public:
  inline NamedForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12()
    : NamedClassDecoder(decoder_, "ForbiddenCondNop Msr_Rule_B6_1_6_A1_PB6_12")
  {}
  virtual ~NamedForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12() {}

 private:
  nacl_arm_dec::ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12);
};

class NamedForbiddenCondNop_Msr_Rule_B6_1_7_P14
    : public NamedClassDecoder {
 public:
  inline NamedForbiddenCondNop_Msr_Rule_B6_1_7_P14()
    : NamedClassDecoder(decoder_, "ForbiddenCondNop Msr_Rule_B6_1_7_P14")
  {}
  virtual ~NamedForbiddenCondNop_Msr_Rule_B6_1_7_P14() {}

 private:
  nacl_arm_dec::ForbiddenCondNop_Msr_Rule_B6_1_7_P14 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondNop_Msr_Rule_B6_1_7_P14);
};

class NamedForbiddenCondNop_Sev_Rule_158_A1_P316
    : public NamedClassDecoder {
 public:
  inline NamedForbiddenCondNop_Sev_Rule_158_A1_P316()
    : NamedClassDecoder(decoder_, "ForbiddenCondNop Sev_Rule_158_A1_P316")
  {}
  virtual ~NamedForbiddenCondNop_Sev_Rule_158_A1_P316() {}

 private:
  nacl_arm_dec::ForbiddenCondNop_Sev_Rule_158_A1_P316 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondNop_Sev_Rule_158_A1_P316);
};

class NamedForbiddenCondNop_Smc_Rule_B6_1_9_P18
    : public NamedClassDecoder {
 public:
  inline NamedForbiddenCondNop_Smc_Rule_B6_1_9_P18()
    : NamedClassDecoder(decoder_, "ForbiddenCondNop Smc_Rule_B6_1_9_P18")
  {}
  virtual ~NamedForbiddenCondNop_Smc_Rule_B6_1_9_P18() {}

 private:
  nacl_arm_dec::ForbiddenCondNop_Smc_Rule_B6_1_9_P18 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondNop_Smc_Rule_B6_1_9_P18);
};

class NamedForbiddenCondNop_Wfe_Rule_411_A1_P808
    : public NamedClassDecoder {
 public:
  inline NamedForbiddenCondNop_Wfe_Rule_411_A1_P808()
    : NamedClassDecoder(decoder_, "ForbiddenCondNop Wfe_Rule_411_A1_P808")
  {}
  virtual ~NamedForbiddenCondNop_Wfe_Rule_411_A1_P808() {}

 private:
  nacl_arm_dec::ForbiddenCondNop_Wfe_Rule_411_A1_P808 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondNop_Wfe_Rule_411_A1_P808);
};

class NamedForbiddenCondNop_Wfi_Rule_412_A1_P810
    : public NamedClassDecoder {
 public:
  inline NamedForbiddenCondNop_Wfi_Rule_412_A1_P810()
    : NamedClassDecoder(decoder_, "ForbiddenCondNop Wfi_Rule_412_A1_P810")
  {}
  virtual ~NamedForbiddenCondNop_Wfi_Rule_412_A1_P810() {}

 private:
  nacl_arm_dec::ForbiddenCondNop_Wfi_Rule_412_A1_P810 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondNop_Wfi_Rule_412_A1_P810);
};

class NamedLoad2RegisterImm12Op_Ldr_Rule_58_A1_P120
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm12Op_Ldr_Rule_58_A1_P120()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op Ldr_Rule_58_A1_P120")
  {}
  virtual ~NamedLoad2RegisterImm12Op_Ldr_Rule_58_A1_P120() {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_Ldr_Rule_58_A1_P120 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_Ldr_Rule_58_A1_P120);
};

class NamedLoad2RegisterImm12Op_Ldr_Rule_59_A1_P122
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm12Op_Ldr_Rule_59_A1_P122()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op Ldr_Rule_59_A1_P122")
  {}
  virtual ~NamedLoad2RegisterImm12Op_Ldr_Rule_59_A1_P122() {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_Ldr_Rule_59_A1_P122 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_Ldr_Rule_59_A1_P122);
};

class NamedLoad2RegisterImm12Op_Ldrb_Rule_62_A1_P128
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm12Op_Ldrb_Rule_62_A1_P128()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op Ldrb_Rule_62_A1_P128")
  {}
  virtual ~NamedLoad2RegisterImm12Op_Ldrb_Rule_62_A1_P128() {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_Ldrb_Rule_62_A1_P128);
};

class NamedLoad2RegisterImm12Op_Ldrb_Rule_63_A1_P130
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm12Op_Ldrb_Rule_63_A1_P130()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op Ldrb_Rule_63_A1_P130")
  {}
  virtual ~NamedLoad2RegisterImm12Op_Ldrb_Rule_63_A1_P130() {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_Ldrb_Rule_63_A1_P130);
};

class NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8DoubleOp Ldrd_Rule_66_A1_P136")
  {}
  virtual ~NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136() {}

 private:
  nacl_arm_dec::Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136);
};

class NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8DoubleOp Ldrd_Rule_67_A1_P138")
  {}
  virtual ~NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138() {}

 private:
  nacl_arm_dec::Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138);
};

class NamedLoad2RegisterImm8Op_Ldrh_Rule_74_A1_P152
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm8Op_Ldrh_Rule_74_A1_P152()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrh_Rule_74_A1_P152")
  {}
  virtual ~NamedLoad2RegisterImm8Op_Ldrh_Rule_74_A1_P152() {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrh_Rule_74_A1_P152);
};

class NamedLoad2RegisterImm8Op_Ldrh_Rule_75_A1_P154
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm8Op_Ldrh_Rule_75_A1_P154()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrh_Rule_75_A1_P154")
  {}
  virtual ~NamedLoad2RegisterImm8Op_Ldrh_Rule_75_A1_P154() {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrh_Rule_75_A1_P154);
};

class NamedLoad2RegisterImm8Op_Ldrsb_Rule_78_A1_P160
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm8Op_Ldrsb_Rule_78_A1_P160()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrsb_Rule_78_A1_P160")
  {}
  virtual ~NamedLoad2RegisterImm8Op_Ldrsb_Rule_78_A1_P160() {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrsb_Rule_78_A1_P160);
};

class NamedLoad2RegisterImm8Op_Ldrsh_Rule_82_A1_P168
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm8Op_Ldrsh_Rule_82_A1_P168()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrsh_Rule_82_A1_P168")
  {}
  virtual ~NamedLoad2RegisterImm8Op_Ldrsh_Rule_82_A1_P168() {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrsh_Rule_82_A1_P168);
};

class NamedLoad2RegisterImm8Op_Ldrsh_Rule_83_A1_P170
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm8Op_Ldrsh_Rule_83_A1_P170()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrsh_Rule_83_A1_P170")
  {}
  virtual ~NamedLoad2RegisterImm8Op_Ldrsh_Rule_83_A1_P170() {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrsh_Rule_83_A1_P170);
};

class NamedLoad2RegisterImm8Op_ldrsb_Rule_79_A1_162
    : public NamedClassDecoder {
 public:
  inline NamedLoad2RegisterImm8Op_ldrsb_Rule_79_A1_162()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op ldrsb_Rule_79_A1_162")
  {}
  virtual ~NamedLoad2RegisterImm8Op_ldrsb_Rule_79_A1_162() {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_ldrsb_Rule_79_A1_162 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_ldrsb_Rule_79_A1_162);
};

class NamedLoad3RegisterDoubleOp_Ldrd_Rule_68_A1_P140
    : public NamedClassDecoder {
 public:
  inline NamedLoad3RegisterDoubleOp_Ldrd_Rule_68_A1_P140()
    : NamedClassDecoder(decoder_, "Load3RegisterDoubleOp Ldrd_Rule_68_A1_P140")
  {}
  virtual ~NamedLoad3RegisterDoubleOp_Ldrd_Rule_68_A1_P140() {}

 private:
  nacl_arm_dec::Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterDoubleOp_Ldrd_Rule_68_A1_P140);
};

class NamedLoad3RegisterImm5Op_Ldr_Rule_60_A1_P124
    : public NamedClassDecoder {
 public:
  inline NamedLoad3RegisterImm5Op_Ldr_Rule_60_A1_P124()
    : NamedClassDecoder(decoder_, "Load3RegisterImm5Op Ldr_Rule_60_A1_P124")
  {}
  virtual ~NamedLoad3RegisterImm5Op_Ldr_Rule_60_A1_P124() {}

 private:
  nacl_arm_dec::Load3RegisterImm5Op_Ldr_Rule_60_A1_P124 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterImm5Op_Ldr_Rule_60_A1_P124);
};

class NamedLoad3RegisterImm5Op_Ldrb_Rule_64_A1_P132
    : public NamedClassDecoder {
 public:
  inline NamedLoad3RegisterImm5Op_Ldrb_Rule_64_A1_P132()
    : NamedClassDecoder(decoder_, "Load3RegisterImm5Op Ldrb_Rule_64_A1_P132")
  {}
  virtual ~NamedLoad3RegisterImm5Op_Ldrb_Rule_64_A1_P132() {}

 private:
  nacl_arm_dec::Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterImm5Op_Ldrb_Rule_64_A1_P132);
};

class NamedLoad3RegisterOp_Ldrh_Rule_76_A1_P156
    : public NamedClassDecoder {
 public:
  inline NamedLoad3RegisterOp_Ldrh_Rule_76_A1_P156()
    : NamedClassDecoder(decoder_, "Load3RegisterOp Ldrh_Rule_76_A1_P156")
  {}
  virtual ~NamedLoad3RegisterOp_Ldrh_Rule_76_A1_P156() {}

 private:
  nacl_arm_dec::Load3RegisterOp_Ldrh_Rule_76_A1_P156 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_Ldrh_Rule_76_A1_P156);
};

class NamedLoad3RegisterOp_Ldrsb_Rule_80_A1_P164
    : public NamedClassDecoder {
 public:
  inline NamedLoad3RegisterOp_Ldrsb_Rule_80_A1_P164()
    : NamedClassDecoder(decoder_, "Load3RegisterOp Ldrsb_Rule_80_A1_P164")
  {}
  virtual ~NamedLoad3RegisterOp_Ldrsb_Rule_80_A1_P164() {}

 private:
  nacl_arm_dec::Load3RegisterOp_Ldrsb_Rule_80_A1_P164 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_Ldrsb_Rule_80_A1_P164);
};

class NamedLoad3RegisterOp_Ldrsh_Rule_84_A1_P172
    : public NamedClassDecoder {
 public:
  inline NamedLoad3RegisterOp_Ldrsh_Rule_84_A1_P172()
    : NamedClassDecoder(decoder_, "Load3RegisterOp Ldrsh_Rule_84_A1_P172")
  {}
  virtual ~NamedLoad3RegisterOp_Ldrsh_Rule_84_A1_P172() {}

 private:
  nacl_arm_dec::Load3RegisterOp_Ldrsh_Rule_84_A1_P172 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_Ldrsh_Rule_84_A1_P172);
};

class NamedLoadCoprocessor_None
    : public NamedClassDecoder {
 public:
  inline NamedLoadCoprocessor_None()
    : NamedClassDecoder(decoder_, "LoadCoprocessor None")
  {}
  virtual ~NamedLoadCoprocessor_None() {}

 private:
  nacl_arm_dec::LoadCoprocessor_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadCoprocessor_None);
};

class NamedLoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146
    : public NamedClassDecoder {
 public:
  inline NamedLoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterDoubleOp Ldrexd_Rule_71_A1_P146")
  {}
  virtual ~NamedLoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146() {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146);
};

class NamedLoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142
    : public NamedClassDecoder {
 public:
  inline NamedLoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp Ldrex_Rule_69_A1_P142")
  {}
  virtual ~NamedLoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142() {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142);
};

class NamedLoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144
    : public NamedClassDecoder {
 public:
  inline NamedLoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp Ldrexb_Rule_70_A1_P144")
  {}
  virtual ~NamedLoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144() {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144);
};

class NamedLoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148
    : public NamedClassDecoder {
 public:
  inline NamedLoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp Ldrexh_Rule_72_A1_P148")
  {}
  virtual ~NamedLoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148() {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148);
};

class NamedLoadMultiple_None
    : public NamedClassDecoder {
 public:
  inline NamedLoadMultiple_None()
    : NamedClassDecoder(decoder_, "LoadMultiple None")
  {}
  virtual ~NamedLoadMultiple_None() {}

 private:
  nacl_arm_dec::LoadMultiple_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadMultiple_None);
};

class NamedMaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50
    : public NamedClassDecoder {
 public:
  inline NamedMaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50()
    : NamedClassDecoder(decoder_, "MaskedBinary2RegisterImmediateOp Bic_Rule_19_A1_P50")
  {}
  virtual ~NamedMaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50() {}

 private:
  nacl_arm_dec::MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50);
};

class NamedMaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454
    : public NamedClassDecoder {
 public:
  inline NamedMaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454()
    : NamedClassDecoder(decoder_, "MaskedBinaryRegisterImmediateTest Tst_Rule_230_A1_P454")
  {}
  virtual ~NamedMaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454() {}

 private:
  nacl_arm_dec::MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454);
};

class NamedMoveDoubleFromCoprocessor_None
    : public NamedClassDecoder {
 public:
  inline NamedMoveDoubleFromCoprocessor_None()
    : NamedClassDecoder(decoder_, "MoveDoubleFromCoprocessor None")
  {}
  virtual ~NamedMoveDoubleFromCoprocessor_None() {}

 private:
  nacl_arm_dec::MoveDoubleFromCoprocessor_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveDoubleFromCoprocessor_None);
};

class NamedMoveFromCoprocessor_None
    : public NamedClassDecoder {
 public:
  inline NamedMoveFromCoprocessor_None()
    : NamedClassDecoder(decoder_, "MoveFromCoprocessor None")
  {}
  virtual ~NamedMoveFromCoprocessor_None() {}

 private:
  nacl_arm_dec::MoveFromCoprocessor_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveFromCoprocessor_None);
};

class NamedMoveImmediate12ToApsr_Msr_Rule_103_A1_P208
    : public NamedClassDecoder {
 public:
  inline NamedMoveImmediate12ToApsr_Msr_Rule_103_A1_P208()
    : NamedClassDecoder(decoder_, "MoveImmediate12ToApsr Msr_Rule_103_A1_P208")
  {}
  virtual ~NamedMoveImmediate12ToApsr_Msr_Rule_103_A1_P208() {}

 private:
  nacl_arm_dec::MoveImmediate12ToApsr_Msr_Rule_103_A1_P208 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveImmediate12ToApsr_Msr_Rule_103_A1_P208);
};

class NamedPackSatRev_None
    : public NamedClassDecoder {
 public:
  inline NamedPackSatRev_None()
    : NamedClassDecoder(decoder_, "PackSatRev None")
  {}
  virtual ~NamedPackSatRev_None() {}

 private:
  nacl_arm_dec::PackSatRev_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPackSatRev_None);
};

class NamedRoadblock_None
    : public NamedClassDecoder {
 public:
  inline NamedRoadblock_None()
    : NamedClassDecoder(decoder_, "Roadblock None")
  {}
  virtual ~NamedRoadblock_None() {}

 private:
  nacl_arm_dec::Roadblock_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedRoadblock_None);
};

class NamedStore2RegisterImm12Op_Str_Rule_194_A1_P384
    : public NamedClassDecoder {
 public:
  inline NamedStore2RegisterImm12Op_Str_Rule_194_A1_P384()
    : NamedClassDecoder(decoder_, "Store2RegisterImm12Op Str_Rule_194_A1_P384")
  {}
  virtual ~NamedStore2RegisterImm12Op_Str_Rule_194_A1_P384() {}

 private:
  nacl_arm_dec::Store2RegisterImm12Op_Str_Rule_194_A1_P384 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm12Op_Str_Rule_194_A1_P384);
};

class NamedStore2RegisterImm12Op_Strb_Rule_197_A1_P390
    : public NamedClassDecoder {
 public:
  inline NamedStore2RegisterImm12Op_Strb_Rule_197_A1_P390()
    : NamedClassDecoder(decoder_, "Store2RegisterImm12Op Strb_Rule_197_A1_P390")
  {}
  virtual ~NamedStore2RegisterImm12Op_Strb_Rule_197_A1_P390() {}

 private:
  nacl_arm_dec::Store2RegisterImm12Op_Strb_Rule_197_A1_P390 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm12Op_Strb_Rule_197_A1_P390);
};

class NamedStore2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396
    : public NamedClassDecoder {
 public:
  inline NamedStore2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396()
    : NamedClassDecoder(decoder_, "Store2RegisterImm8DoubleOp Strd_Rule_200_A1_P396")
  {}
  virtual ~NamedStore2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396() {}

 private:
  nacl_arm_dec::Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396);
};

class NamedStore2RegisterImm8Op_Strh_Rule_207_A1_P410
    : public NamedClassDecoder {
 public:
  inline NamedStore2RegisterImm8Op_Strh_Rule_207_A1_P410()
    : NamedClassDecoder(decoder_, "Store2RegisterImm8Op Strh_Rule_207_A1_P410")
  {}
  virtual ~NamedStore2RegisterImm8Op_Strh_Rule_207_A1_P410() {}

 private:
  nacl_arm_dec::Store2RegisterImm8Op_Strh_Rule_207_A1_P410 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm8Op_Strh_Rule_207_A1_P410);
};

class NamedStore3RegisterDoubleOp_Strd_Rule_201_A1_P398
    : public NamedClassDecoder {
 public:
  inline NamedStore3RegisterDoubleOp_Strd_Rule_201_A1_P398()
    : NamedClassDecoder(decoder_, "Store3RegisterDoubleOp Strd_Rule_201_A1_P398")
  {}
  virtual ~NamedStore3RegisterDoubleOp_Strd_Rule_201_A1_P398() {}

 private:
  nacl_arm_dec::Store3RegisterDoubleOp_Strd_Rule_201_A1_P398 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterDoubleOp_Strd_Rule_201_A1_P398);
};

class NamedStore3RegisterImm5Op_Str_Rule_195_A1_P386
    : public NamedClassDecoder {
 public:
  inline NamedStore3RegisterImm5Op_Str_Rule_195_A1_P386()
    : NamedClassDecoder(decoder_, "Store3RegisterImm5Op Str_Rule_195_A1_P386")
  {}
  virtual ~NamedStore3RegisterImm5Op_Str_Rule_195_A1_P386() {}

 private:
  nacl_arm_dec::Store3RegisterImm5Op_Str_Rule_195_A1_P386 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterImm5Op_Str_Rule_195_A1_P386);
};

class NamedStore3RegisterImm5Op_Strb_Rule_198_A1_P392
    : public NamedClassDecoder {
 public:
  inline NamedStore3RegisterImm5Op_Strb_Rule_198_A1_P392()
    : NamedClassDecoder(decoder_, "Store3RegisterImm5Op Strb_Rule_198_A1_P392")
  {}
  virtual ~NamedStore3RegisterImm5Op_Strb_Rule_198_A1_P392() {}

 private:
  nacl_arm_dec::Store3RegisterImm5Op_Strb_Rule_198_A1_P392 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterImm5Op_Strb_Rule_198_A1_P392);
};

class NamedStore3RegisterOp_Strh_Rule_208_A1_P412
    : public NamedClassDecoder {
 public:
  inline NamedStore3RegisterOp_Strh_Rule_208_A1_P412()
    : NamedClassDecoder(decoder_, "Store3RegisterOp Strh_Rule_208_A1_P412")
  {}
  virtual ~NamedStore3RegisterOp_Strh_Rule_208_A1_P412() {}

 private:
  nacl_arm_dec::Store3RegisterOp_Strh_Rule_208_A1_P412 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterOp_Strh_Rule_208_A1_P412);
};

class NamedStoreCoprocessor_None
    : public NamedClassDecoder {
 public:
  inline NamedStoreCoprocessor_None()
    : NamedClassDecoder(decoder_, "StoreCoprocessor None")
  {}
  virtual ~NamedStoreCoprocessor_None() {}

 private:
  nacl_arm_dec::StoreCoprocessor_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreCoprocessor_None);
};

class NamedStoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404
    : public NamedClassDecoder {
 public:
  inline NamedStoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterDoubleOp Strexd_Rule_204_A1_P404")
  {}
  virtual ~NamedStoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404() {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404);
};

class NamedStoreExclusive3RegisterOp_Strex_Rule_202_A1_P400
    : public NamedClassDecoder {
 public:
  inline NamedStoreExclusive3RegisterOp_Strex_Rule_202_A1_P400()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp Strex_Rule_202_A1_P400")
  {}
  virtual ~NamedStoreExclusive3RegisterOp_Strex_Rule_202_A1_P400() {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_Strex_Rule_202_A1_P400);
};

class NamedStoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402
    : public NamedClassDecoder {
 public:
  inline NamedStoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp Strexb_Rule_203_A1_P402")
  {}
  virtual ~NamedStoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402() {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402);
};

class NamedStoreExclusive3RegisterOp_cccc00011110nnnndddd11111001tttt
    : public NamedClassDecoder {
 public:
  inline NamedStoreExclusive3RegisterOp_cccc00011110nnnndddd11111001tttt()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp cccc00011110nnnndddd11111001tttt")
  {}
  virtual ~NamedStoreExclusive3RegisterOp_cccc00011110nnnndddd11111001tttt() {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_cccc00011110nnnndddd11111001tttt decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_cccc00011110nnnndddd11111001tttt);
};

class NamedStoreImmediate_None
    : public NamedClassDecoder {
 public:
  inline NamedStoreImmediate_None()
    : NamedClassDecoder(decoder_, "StoreImmediate None")
  {}
  virtual ~NamedStoreImmediate_None() {}

 private:
  nacl_arm_dec::StoreImmediate_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreImmediate_None);
};

class NamedUnary1RegisterBitRange_Bfc_17_A1_P46
    : public NamedClassDecoder {
 public:
  inline NamedUnary1RegisterBitRange_Bfc_17_A1_P46()
    : NamedClassDecoder(decoder_, "Unary1RegisterBitRange Bfc_17_A1_P46")
  {}
  virtual ~NamedUnary1RegisterBitRange_Bfc_17_A1_P46() {}

 private:
  nacl_arm_dec::Unary1RegisterBitRange_Bfc_17_A1_P46 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterBitRange_Bfc_17_A1_P46);
};

class NamedUnary1RegisterImmediateOp_Adr_Rule_10_A1_P32
    : public NamedClassDecoder {
 public:
  inline NamedUnary1RegisterImmediateOp_Adr_Rule_10_A1_P32()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOp Adr_Rule_10_A1_P32")
  {}
  virtual ~NamedUnary1RegisterImmediateOp_Adr_Rule_10_A1_P32() {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOp_Adr_Rule_10_A1_P32);
};

class NamedUnary1RegisterImmediateOp_Adr_Rule_10_A2_P32
    : public NamedClassDecoder {
 public:
  inline NamedUnary1RegisterImmediateOp_Adr_Rule_10_A2_P32()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOp Adr_Rule_10_A2_P32")
  {}
  virtual ~NamedUnary1RegisterImmediateOp_Adr_Rule_10_A2_P32() {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOp_Adr_Rule_10_A2_P32);
};

class NamedUnary1RegisterImmediateOp_Mov_Rule_96_A1_P194
    : public NamedClassDecoder {
 public:
  inline NamedUnary1RegisterImmediateOp_Mov_Rule_96_A1_P194()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOp Mov_Rule_96_A1_P194")
  {}
  virtual ~NamedUnary1RegisterImmediateOp_Mov_Rule_96_A1_P194() {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOp_Mov_Rule_96_A1_P194);
};

class NamedUnary1RegisterImmediateOp_Mov_Rule_96_A2_P_194
    : public NamedClassDecoder {
 public:
  inline NamedUnary1RegisterImmediateOp_Mov_Rule_96_A2_P_194()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOp Mov_Rule_96_A2_P_194")
  {}
  virtual ~NamedUnary1RegisterImmediateOp_Mov_Rule_96_A2_P_194() {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOp_Mov_Rule_96_A2_P_194 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOp_Mov_Rule_96_A2_P_194);
};

class NamedUnary1RegisterImmediateOp_Mvn_Rule_106_A1_P214
    : public NamedClassDecoder {
 public:
  inline NamedUnary1RegisterImmediateOp_Mvn_Rule_106_A1_P214()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOp Mvn_Rule_106_A1_P214")
  {}
  virtual ~NamedUnary1RegisterImmediateOp_Mvn_Rule_106_A1_P214() {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOp_Mvn_Rule_106_A1_P214);
};

class NamedUnary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10
    : public NamedClassDecoder {
 public:
  inline NamedUnary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10()
    : NamedClassDecoder(decoder_, "Unary1RegisterSet Mrs_Rule_102_A1_P206_Or_B6_10")
  {}
  virtual ~NamedUnary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10() {}

 private:
  nacl_arm_dec::Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10);
};

class NamedUnary1RegisterUse_Msr_Rule_104_A1_P210
    : public NamedClassDecoder {
 public:
  inline NamedUnary1RegisterUse_Msr_Rule_104_A1_P210()
    : NamedClassDecoder(decoder_, "Unary1RegisterUse Msr_Rule_104_A1_P210")
  {}
  virtual ~NamedUnary1RegisterUse_Msr_Rule_104_A1_P210() {}

 private:
  nacl_arm_dec::Unary1RegisterUse_Msr_Rule_104_A1_P210 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterUse_Msr_Rule_104_A1_P210);
};

class NamedUnary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40
    : public NamedClassDecoder {
 public:
  inline NamedUnary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp Asr_Rule_14_A1_P40")
  {}
  virtual ~NamedUnary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40() {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40);
};

class NamedUnary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178
    : public NamedClassDecoder {
 public:
  inline NamedUnary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp Lsl_Rule_88_A1_P178")
  {}
  virtual ~NamedUnary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178() {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178);
};

class NamedUnary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182
    : public NamedClassDecoder {
 public:
  inline NamedUnary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp Lsr_Rule_90_A1_P182")
  {}
  virtual ~NamedUnary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182() {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182);
};

class NamedUnary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216
    : public NamedClassDecoder {
 public:
  inline NamedUnary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp Mvn_Rule_107_A1_P216")
  {}
  virtual ~NamedUnary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216() {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216);
};

class NamedUnary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278
    : public NamedClassDecoder {
 public:
  inline NamedUnary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp Ror_Rule_139_A1_P278")
  {}
  virtual ~NamedUnary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278() {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278);
};

class NamedUnary2RegisterOp_Mov_Rule_97_A1_P196
    : public NamedClassDecoder {
 public:
  inline NamedUnary2RegisterOp_Mov_Rule_97_A1_P196()
    : NamedClassDecoder(decoder_, "Unary2RegisterOp Mov_Rule_97_A1_P196")
  {}
  virtual ~NamedUnary2RegisterOp_Mov_Rule_97_A1_P196() {}

 private:
  nacl_arm_dec::Unary2RegisterOp_Mov_Rule_97_A1_P196 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOp_Mov_Rule_97_A1_P196);
};

class NamedUnary2RegisterOp_Rrx_Rule_141_A1_P282
    : public NamedClassDecoder {
 public:
  inline NamedUnary2RegisterOp_Rrx_Rule_141_A1_P282()
    : NamedClassDecoder(decoder_, "Unary2RegisterOp Rrx_Rule_141_A1_P282")
  {}
  virtual ~NamedUnary2RegisterOp_Rrx_Rule_141_A1_P282() {}

 private:
  nacl_arm_dec::Unary2RegisterOp_Rrx_Rule_141_A1_P282 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOp_Rrx_Rule_141_A1_P282);
};

class NamedUnary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72
    : public NamedClassDecoder {
 public:
  inline NamedUnary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPc Clz_Rule_31_A1_P72")
  {}
  virtual ~NamedUnary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72() {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72);
};

class NamedUnary3RegisterShiftedOp_Mvn_Rule_108_A1_P218
    : public NamedClassDecoder {
 public:
  inline NamedUnary3RegisterShiftedOp_Mvn_Rule_108_A1_P218()
    : NamedClassDecoder(decoder_, "Unary3RegisterShiftedOp Mvn_Rule_108_A1_P218")
  {}
  virtual ~NamedUnary3RegisterShiftedOp_Mvn_Rule_108_A1_P218() {}

 private:
  nacl_arm_dec::Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary3RegisterShiftedOp_Mvn_Rule_108_A1_P218);
};

class NamedUndefined_None
    : public NamedClassDecoder {
 public:
  inline NamedUndefined_None()
    : NamedClassDecoder(decoder_, "Undefined None")
  {}
  virtual ~NamedUndefined_None() {}

 private:
  nacl_arm_dec::Undefined_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUndefined_None);
};

class NamedUnpredictable_None
    : public NamedClassDecoder {
 public:
  inline NamedUnpredictable_None()
    : NamedClassDecoder(decoder_, "Unpredictable None")
  {}
  virtual ~NamedUnpredictable_None() {}

 private:
  nacl_arm_dec::Unpredictable_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnpredictable_None);
};

class NamedVectorLoad_None
    : public NamedClassDecoder {
 public:
  inline NamedVectorLoad_None()
    : NamedClassDecoder(decoder_, "VectorLoad None")
  {}
  virtual ~NamedVectorLoad_None() {}

 private:
  nacl_arm_dec::VectorLoad_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoad_None);
};

class NamedVectorStore_None
    : public NamedClassDecoder {
 public:
  inline NamedVectorStore_None()
    : NamedClassDecoder(decoder_, "VectorStore None")
  {}
  virtual ~NamedVectorStore_None() {}

 private:
  nacl_arm_dec::VectorStore_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorStore_None);
};

class NamedBreakpoint_Bkpt_Rule_22_A1_P56
    : public NamedClassDecoder {
 public:
  inline NamedBreakpoint_Bkpt_Rule_22_A1_P56()
    : NamedClassDecoder(decoder_, "Breakpoint Bkpt_Rule_22_A1_P56")
  {}
  virtual ~NamedBreakpoint_Bkpt_Rule_22_A1_P56() {}

 private:
  nacl_arm_dec::Breakpoint_Bkpt_Rule_22_A1_P56 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBreakpoint_Bkpt_Rule_22_A1_P56);
};

class NamedBxBlx_Blx_Rule_24_A1_P60
    : public NamedClassDecoder {
 public:
  inline NamedBxBlx_Blx_Rule_24_A1_P60()
    : NamedClassDecoder(decoder_, "BxBlx Blx_Rule_24_A1_P60")
  {}
  virtual ~NamedBxBlx_Blx_Rule_24_A1_P60() {}

 private:
  nacl_arm_dec::BxBlx_Blx_Rule_24_A1_P60 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBxBlx_Blx_Rule_24_A1_P60);
};

class NamedBxBlx_Bx_Rule_25_A1_P62
    : public NamedClassDecoder {
 public:
  inline NamedBxBlx_Bx_Rule_25_A1_P62()
    : NamedClassDecoder(decoder_, "BxBlx Bx_Rule_25_A1_P62")
  {}
  virtual ~NamedBxBlx_Bx_Rule_25_A1_P62() {}

 private:
  nacl_arm_dec::BxBlx_Bx_Rule_25_A1_P62 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBxBlx_Bx_Rule_25_A1_P62);
};

class NamedDefs12To15_Adc_Rule_2_A1_P16
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Adc_Rule_2_A1_P16()
    : NamedClassDecoder(decoder_, "Defs12To15 Adc_Rule_2_A1_P16")
  {}
  virtual ~NamedDefs12To15_Adc_Rule_2_A1_P16() {}

 private:
  nacl_arm_dec::Defs12To15_Adc_Rule_2_A1_P16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Adc_Rule_2_A1_P16);
};

class NamedDefs12To15_Adc_Rule_6_A1_P14
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Adc_Rule_6_A1_P14()
    : NamedClassDecoder(decoder_, "Defs12To15 Adc_Rule_6_A1_P14")
  {}
  virtual ~NamedDefs12To15_Adc_Rule_6_A1_P14() {}

 private:
  nacl_arm_dec::Defs12To15_Adc_Rule_6_A1_P14 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Adc_Rule_6_A1_P14);
};

class NamedDefs12To15_Add_Rule_5_A1_P22
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Add_Rule_5_A1_P22()
    : NamedClassDecoder(decoder_, "Defs12To15 Add_Rule_5_A1_P22")
  {}
  virtual ~NamedDefs12To15_Add_Rule_5_A1_P22() {}

 private:
  nacl_arm_dec::Defs12To15_Add_Rule_5_A1_P22 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Add_Rule_5_A1_P22);
};

class NamedDefs12To15_Add_Rule_6_A1_P24
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Add_Rule_6_A1_P24()
    : NamedClassDecoder(decoder_, "Defs12To15 Add_Rule_6_A1_P24")
  {}
  virtual ~NamedDefs12To15_Add_Rule_6_A1_P24() {}

 private:
  nacl_arm_dec::Defs12To15_Add_Rule_6_A1_P24 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Add_Rule_6_A1_P24);
};

class NamedDefs12To15_Adr_Rule_10_A1_P32
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Adr_Rule_10_A1_P32()
    : NamedClassDecoder(decoder_, "Defs12To15 Adr_Rule_10_A1_P32")
  {}
  virtual ~NamedDefs12To15_Adr_Rule_10_A1_P32() {}

 private:
  nacl_arm_dec::Defs12To15_Adr_Rule_10_A1_P32 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Adr_Rule_10_A1_P32);
};

class NamedDefs12To15_Adr_Rule_10_A2_P32
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Adr_Rule_10_A2_P32()
    : NamedClassDecoder(decoder_, "Defs12To15 Adr_Rule_10_A2_P32")
  {}
  virtual ~NamedDefs12To15_Adr_Rule_10_A2_P32() {}

 private:
  nacl_arm_dec::Defs12To15_Adr_Rule_10_A2_P32 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Adr_Rule_10_A2_P32);
};

class NamedDefs12To15_And_Rule_11_A1_P34
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_And_Rule_11_A1_P34()
    : NamedClassDecoder(decoder_, "Defs12To15 And_Rule_11_A1_P34")
  {}
  virtual ~NamedDefs12To15_And_Rule_11_A1_P34() {}

 private:
  nacl_arm_dec::Defs12To15_And_Rule_11_A1_P34 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_And_Rule_11_A1_P34);
};

class NamedDefs12To15_And_Rule_7_A1_P36
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_And_Rule_7_A1_P36()
    : NamedClassDecoder(decoder_, "Defs12To15 And_Rule_7_A1_P36")
  {}
  virtual ~NamedDefs12To15_And_Rule_7_A1_P36() {}

 private:
  nacl_arm_dec::Defs12To15_And_Rule_7_A1_P36 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_And_Rule_7_A1_P36);
};

class NamedDefs12To15_Asr_Rule_14_A1_P40
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Asr_Rule_14_A1_P40()
    : NamedClassDecoder(decoder_, "Defs12To15 Asr_Rule_14_A1_P40")
  {}
  virtual ~NamedDefs12To15_Asr_Rule_14_A1_P40() {}

 private:
  nacl_arm_dec::Defs12To15_Asr_Rule_14_A1_P40 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Asr_Rule_14_A1_P40);
};

class NamedDefs12To15_Bfi_Rule_18_A1_P48
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Bfi_Rule_18_A1_P48()
    : NamedClassDecoder(decoder_, "Defs12To15 Bfi_Rule_18_A1_P48")
  {}
  virtual ~NamedDefs12To15_Bfi_Rule_18_A1_P48() {}

 private:
  nacl_arm_dec::Defs12To15_Bfi_Rule_18_A1_P48 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Bfi_Rule_18_A1_P48);
};

class NamedDefs12To15_Bic_Rule_20_A1_P52
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Bic_Rule_20_A1_P52()
    : NamedClassDecoder(decoder_, "Defs12To15 Bic_Rule_20_A1_P52")
  {}
  virtual ~NamedDefs12To15_Bic_Rule_20_A1_P52() {}

 private:
  nacl_arm_dec::Defs12To15_Bic_Rule_20_A1_P52 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Bic_Rule_20_A1_P52);
};

class NamedDefs12To15_Eor_Rule_44_A1_P94
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Eor_Rule_44_A1_P94()
    : NamedClassDecoder(decoder_, "Defs12To15 Eor_Rule_44_A1_P94")
  {}
  virtual ~NamedDefs12To15_Eor_Rule_44_A1_P94() {}

 private:
  nacl_arm_dec::Defs12To15_Eor_Rule_44_A1_P94 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Eor_Rule_44_A1_P94);
};

class NamedDefs12To15_Lsl_Rule_88_A1_P178
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Lsl_Rule_88_A1_P178()
    : NamedClassDecoder(decoder_, "Defs12To15 Lsl_Rule_88_A1_P178")
  {}
  virtual ~NamedDefs12To15_Lsl_Rule_88_A1_P178() {}

 private:
  nacl_arm_dec::Defs12To15_Lsl_Rule_88_A1_P178 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Lsl_Rule_88_A1_P178);
};

class NamedDefs12To15_Lsr_Rule_90_A1_P182
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Lsr_Rule_90_A1_P182()
    : NamedClassDecoder(decoder_, "Defs12To15 Lsr_Rule_90_A1_P182")
  {}
  virtual ~NamedDefs12To15_Lsr_Rule_90_A1_P182() {}

 private:
  nacl_arm_dec::Defs12To15_Lsr_Rule_90_A1_P182 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Lsr_Rule_90_A1_P182);
};

class NamedDefs12To15_Mov_Rule_96_A1_P194
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Mov_Rule_96_A1_P194()
    : NamedClassDecoder(decoder_, "Defs12To15 Mov_Rule_96_A1_P194")
  {}
  virtual ~NamedDefs12To15_Mov_Rule_96_A1_P194() {}

 private:
  nacl_arm_dec::Defs12To15_Mov_Rule_96_A1_P194 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Mov_Rule_96_A1_P194);
};

class NamedDefs12To15_Mov_Rule_96_A2_P_194
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Mov_Rule_96_A2_P_194()
    : NamedClassDecoder(decoder_, "Defs12To15 Mov_Rule_96_A2_P_194")
  {}
  virtual ~NamedDefs12To15_Mov_Rule_96_A2_P_194() {}

 private:
  nacl_arm_dec::Defs12To15_Mov_Rule_96_A2_P_194 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Mov_Rule_96_A2_P_194);
};

class NamedDefs12To15_Mov_Rule_97_A1_P196
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Mov_Rule_97_A1_P196()
    : NamedClassDecoder(decoder_, "Defs12To15 Mov_Rule_97_A1_P196")
  {}
  virtual ~NamedDefs12To15_Mov_Rule_97_A1_P196() {}

 private:
  nacl_arm_dec::Defs12To15_Mov_Rule_97_A1_P196 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Mov_Rule_97_A1_P196);
};

class NamedDefs12To15_Mvn_Rule_106_A1_P214
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Mvn_Rule_106_A1_P214()
    : NamedClassDecoder(decoder_, "Defs12To15 Mvn_Rule_106_A1_P214")
  {}
  virtual ~NamedDefs12To15_Mvn_Rule_106_A1_P214() {}

 private:
  nacl_arm_dec::Defs12To15_Mvn_Rule_106_A1_P214 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Mvn_Rule_106_A1_P214);
};

class NamedDefs12To15_Mvn_Rule_107_A1_P216
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Mvn_Rule_107_A1_P216()
    : NamedClassDecoder(decoder_, "Defs12To15 Mvn_Rule_107_A1_P216")
  {}
  virtual ~NamedDefs12To15_Mvn_Rule_107_A1_P216() {}

 private:
  nacl_arm_dec::Defs12To15_Mvn_Rule_107_A1_P216 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Mvn_Rule_107_A1_P216);
};

class NamedDefs12To15_Orr_Rule_113_A1_P228
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Orr_Rule_113_A1_P228()
    : NamedClassDecoder(decoder_, "Defs12To15 Orr_Rule_113_A1_P228")
  {}
  virtual ~NamedDefs12To15_Orr_Rule_113_A1_P228() {}

 private:
  nacl_arm_dec::Defs12To15_Orr_Rule_113_A1_P228 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Orr_Rule_113_A1_P228);
};

class NamedDefs12To15_Orr_Rule_114_A1_P230
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Orr_Rule_114_A1_P230()
    : NamedClassDecoder(decoder_, "Defs12To15 Orr_Rule_114_A1_P230")
  {}
  virtual ~NamedDefs12To15_Orr_Rule_114_A1_P230() {}

 private:
  nacl_arm_dec::Defs12To15_Orr_Rule_114_A1_P230 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Orr_Rule_114_A1_P230);
};

class NamedDefs12To15_Ror_Rule_139_A1_P278
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Ror_Rule_139_A1_P278()
    : NamedClassDecoder(decoder_, "Defs12To15 Ror_Rule_139_A1_P278")
  {}
  virtual ~NamedDefs12To15_Ror_Rule_139_A1_P278() {}

 private:
  nacl_arm_dec::Defs12To15_Ror_Rule_139_A1_P278 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Ror_Rule_139_A1_P278);
};

class NamedDefs12To15_Rrx_Rule_141_A1_P282
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Rrx_Rule_141_A1_P282()
    : NamedClassDecoder(decoder_, "Defs12To15 Rrx_Rule_141_A1_P282")
  {}
  virtual ~NamedDefs12To15_Rrx_Rule_141_A1_P282() {}

 private:
  nacl_arm_dec::Defs12To15_Rrx_Rule_141_A1_P282 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Rrx_Rule_141_A1_P282);
};

class NamedDefs12To15_Rsb_Rule_142_A1_P284
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Rsb_Rule_142_A1_P284()
    : NamedClassDecoder(decoder_, "Defs12To15 Rsb_Rule_142_A1_P284")
  {}
  virtual ~NamedDefs12To15_Rsb_Rule_142_A1_P284() {}

 private:
  nacl_arm_dec::Defs12To15_Rsb_Rule_142_A1_P284 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Rsb_Rule_142_A1_P284);
};

class NamedDefs12To15_Rsb_Rule_143_P286
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Rsb_Rule_143_P286()
    : NamedClassDecoder(decoder_, "Defs12To15 Rsb_Rule_143_P286")
  {}
  virtual ~NamedDefs12To15_Rsb_Rule_143_P286() {}

 private:
  nacl_arm_dec::Defs12To15_Rsb_Rule_143_P286 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Rsb_Rule_143_P286);
};

class NamedDefs12To15_Rsc_Rule_145_A1_P290
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Rsc_Rule_145_A1_P290()
    : NamedClassDecoder(decoder_, "Defs12To15 Rsc_Rule_145_A1_P290")
  {}
  virtual ~NamedDefs12To15_Rsc_Rule_145_A1_P290() {}

 private:
  nacl_arm_dec::Defs12To15_Rsc_Rule_145_A1_P290 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Rsc_Rule_145_A1_P290);
};

class NamedDefs12To15_Rsc_Rule_146_A1_P292
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Rsc_Rule_146_A1_P292()
    : NamedClassDecoder(decoder_, "Defs12To15 Rsc_Rule_146_A1_P292")
  {}
  virtual ~NamedDefs12To15_Rsc_Rule_146_A1_P292() {}

 private:
  nacl_arm_dec::Defs12To15_Rsc_Rule_146_A1_P292 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Rsc_Rule_146_A1_P292);
};

class NamedDefs12To15_Sbc_Rule_151_A1_P302
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Sbc_Rule_151_A1_P302()
    : NamedClassDecoder(decoder_, "Defs12To15 Sbc_Rule_151_A1_P302")
  {}
  virtual ~NamedDefs12To15_Sbc_Rule_151_A1_P302() {}

 private:
  nacl_arm_dec::Defs12To15_Sbc_Rule_151_A1_P302 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Sbc_Rule_151_A1_P302);
};

class NamedDefs12To15_Sbc_Rule_152_A1_P304
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Sbc_Rule_152_A1_P304()
    : NamedClassDecoder(decoder_, "Defs12To15 Sbc_Rule_152_A1_P304")
  {}
  virtual ~NamedDefs12To15_Sbc_Rule_152_A1_P304() {}

 private:
  nacl_arm_dec::Defs12To15_Sbc_Rule_152_A1_P304 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Sbc_Rule_152_A1_P304);
};

class NamedDefs12To15_SubRule_213_A1_P422
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_SubRule_213_A1_P422()
    : NamedClassDecoder(decoder_, "Defs12To15 SubRule_213_A1_P422")
  {}
  virtual ~NamedDefs12To15_SubRule_213_A1_P422() {}

 private:
  nacl_arm_dec::Defs12To15_SubRule_213_A1_P422 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_SubRule_213_A1_P422);
};

class NamedDefs12To15_Sub_Rule_212_A1_P420
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15_Sub_Rule_212_A1_P420()
    : NamedClassDecoder(decoder_, "Defs12To15 Sub_Rule_212_A1_P420")
  {}
  virtual ~NamedDefs12To15_Sub_Rule_212_A1_P420() {}

 private:
  nacl_arm_dec::Defs12To15_Sub_Rule_212_A1_P420 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15_Sub_Rule_212_A1_P420);
};

class NamedDefs12To15CondsDontCare_Eor_Rule_45_A1_P96
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCare_Eor_Rule_45_A1_P96()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCare Eor_Rule_45_A1_P96")
  {}
  virtual ~NamedDefs12To15CondsDontCare_Eor_Rule_45_A1_P96() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCare_Eor_Rule_45_A1_P96 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCare_Eor_Rule_45_A1_P96);
};

class NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnRsRmNotPc Eor_Rule_46_A1_P98")
  {}
  virtual ~NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qadd16_Rule_125_A1_P252")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qadd8_Rule_126_A1_P254")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qadd_Rule_124_A1_P250")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qasx_Rule_127_A1_P256")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qdadd_Rule_128_A1_P258")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qdsub_Rule_129_A1_P260")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qsax_Rule_130_A1_P262")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qsub16_Rule_132_A1_P266")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qsub8_Rule_133_A1_P268")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qsub_Rule_131_A1_P264")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sadd16_Rule_148_A1_P296")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sasx_Rule_150_A1_P300")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shadd16_Rule_159_A1_P318")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shadd8_Rule_160_A1_P320")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shasx_Rule_161_A1_P322")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shsax_Rule_162_A1_P324")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shsub16_Rule_163_A1_P326")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shsub8_Rule_164_A1_P328")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssad8_Rule_149_A1_P298
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssad8_Rule_149_A1_P298()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Ssad8_Rule_149_A1_P298")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssad8_Rule_149_A1_P298() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Ssad8_Rule_149_A1_P298 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssad8_Rule_149_A1_P298);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Ssax_Rule_185_A1_P366")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Ssub16_Rule_186_A1_P368")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Ssub8_Rule_187_A1_P370")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uadd16_Rule_233_A1_P460")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uadd8_Rule_234_A1_P462")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uasx_Rule_235_A1_P464")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhadd16_Rule_238_A1_P470")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhadd8_Rule_239_A1_P472")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhasx_Rule_240_A1_P474")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhsax_Rule_241_A1_P476")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhsub16_Rule_242_A1_P478")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhsub8_Rule_243_A1_P480")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqadd16_Rule_247_A1_P488")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqadd8_Rule_248_A1_P490")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqasx_Rule_249_A1_P492")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqsax_Rule_250_A1_P494")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqsub16_Rule_251_A1_P496")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqsub8_Rule_252_A1_P498")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Usax_Rule_257_A1_P508")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Usub16_Rule_258_A1_P510")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Usub8_Rule_259_A1_P512")
  {}
  virtual ~NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512() {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512);
};

class NamedDefs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42()
    : NamedClassDecoder(decoder_, "Defs12To15RdRmRnNotPc Asr_Rule_15_A1_P42")
  {}
  virtual ~NamedDefs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42() {}

 private:
  nacl_arm_dec::Defs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42);
};

class NamedDefs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180()
    : NamedClassDecoder(decoder_, "Defs12To15RdRmRnNotPc Lsl_Rule_89_A1_P180")
  {}
  virtual ~NamedDefs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180() {}

 private:
  nacl_arm_dec::Defs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180);
};

class NamedDefs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184()
    : NamedClassDecoder(decoder_, "Defs12To15RdRmRnNotPc Lsr_Rule_91_A1_P184")
  {}
  virtual ~NamedDefs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184() {}

 private:
  nacl_arm_dec::Defs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184);
};

class NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218()
    : NamedClassDecoder(decoder_, "Defs12To15RdRmRnNotPc Mvn_Rule_108_A1_P218")
  {}
  virtual ~NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218() {}

 private:
  nacl_arm_dec::Defs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218);
};

class NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280()
    : NamedClassDecoder(decoder_, "Defs12To15RdRmRnNotPc Ror_Rule_140_A1_P280")
  {}
  virtual ~NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280() {}

 private:
  nacl_arm_dec::Defs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280);
};

class NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnNotPc Clz_Rule_31_A1_P72")
  {}
  virtual ~NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72() {}

 private:
  nacl_arm_dec::Defs12To15RdRnNotPc_Clz_Rule_31_A1_P72 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72);
};

class NamedDefs12To15RdRnNotPc_Sbfx_Rule_154_A1_P308
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnNotPc_Sbfx_Rule_154_A1_P308()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnNotPc Sbfx_Rule_154_A1_P308")
  {}
  virtual ~NamedDefs12To15RdRnNotPc_Sbfx_Rule_154_A1_P308() {}

 private:
  nacl_arm_dec::Defs12To15RdRnNotPc_Sbfx_Rule_154_A1_P308 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnNotPc_Sbfx_Rule_154_A1_P308);
};

class NamedDefs12To15RdRnNotPc_Ubfx_Rule_236_A1_P466
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnNotPc_Ubfx_Rule_236_A1_P466()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnNotPc Ubfx_Rule_236_A1_P466")
  {}
  virtual ~NamedDefs12To15RdRnNotPc_Ubfx_Rule_236_A1_P466() {}

 private:
  nacl_arm_dec::Defs12To15RdRnNotPc_Ubfx_Rule_236_A1_P466 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnNotPc_Ubfx_Rule_236_A1_P466);
};

class NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc Adc_Rule_3_A1_P18")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18);
};

class NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc Add_Rule_7_A1_P26")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26);
};

class NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc And_Rule_13_A1_P38")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38);
};

class NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc Bic_Rule_21_A1_P54")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54);
};

class NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc Orr_Rule_115_A1_P212")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212);
};

class NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc Rsb_Rule_144_A1_P288")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288);
};

class NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc Rsc_Rule_147_A1_P294")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294);
};

class NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc Sbc_Rule_153_A1_P306")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306);
};

class NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnRsRmNotPc Sub_Rule_214_A1_P424")
  {}
  virtual ~NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424() {}

 private:
  nacl_arm_dec::Defs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Smlal_Rule_168_A1_P334")
  {}
  virtual ~NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334() {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Smlald_Rule_170_P336")
  {}
  virtual ~NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336() {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Smlalxx_Rule_169_A1_P336")
  {}
  virtual ~NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336() {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Smlsld_Rule_173_P344")
  {}
  virtual ~NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344() {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Smull_Rule_179_A1_P356")
  {}
  virtual ~NamedDefs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356() {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Umaal_Rule_244_A1_P482")
  {}
  virtual ~NamedDefs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482() {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Umlal_Rule_245_A1_P484")
  {}
  virtual ~NamedDefs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484() {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486
    : public NamedClassDecoder {
 public:
  inline NamedDefs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Umull_Rule_246_A1_P486")
  {}
  virtual ~NamedDefs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486() {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Mla_Rule_94_A1_P190")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Mls_Rule_95_A1_P192")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smlad_Rule_167_P332")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smlawx_Rule_171_A1_340")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smlaxx_Rule_166_A1_P330")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smlsd_Rule_172_P342")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smmla_Rule_174_P346")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smmls_Rule_175_P348")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usda8_Rule_254_A1_P502
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usda8_Rule_254_A1_P502()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Usda8_Rule_254_A1_P502")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usda8_Rule_254_A1_P502() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Usda8_Rule_254_A1_P502 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usda8_Rule_254_A1_P502);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Mul_Rule_105_A1_P212")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smmul_Rule_176_P350")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smuad_Rule_177_P352")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smulwx_Rule_180_A1_P358")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smulxx_Rule_178_P354")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smusd_Rule_181_P360")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500
    : public NamedClassDecoder {
 public:
  inline NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Usad8_Rule_253_A1_P500")
  {}
  virtual ~NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500() {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500);
};

class NamedDontCareInst_Cmn_Rule_32_A1_P74
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Cmn_Rule_32_A1_P74()
    : NamedClassDecoder(decoder_, "DontCareInst Cmn_Rule_32_A1_P74")
  {}
  virtual ~NamedDontCareInst_Cmn_Rule_32_A1_P74() {}

 private:
  nacl_arm_dec::DontCareInst_Cmn_Rule_32_A1_P74 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Cmn_Rule_32_A1_P74);
};

class NamedDontCareInst_Cmn_Rule_33_A1_P76
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Cmn_Rule_33_A1_P76()
    : NamedClassDecoder(decoder_, "DontCareInst Cmn_Rule_33_A1_P76")
  {}
  virtual ~NamedDontCareInst_Cmn_Rule_33_A1_P76() {}

 private:
  nacl_arm_dec::DontCareInst_Cmn_Rule_33_A1_P76 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Cmn_Rule_33_A1_P76);
};

class NamedDontCareInst_Cmp_Rule_35_A1_P80
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Cmp_Rule_35_A1_P80()
    : NamedClassDecoder(decoder_, "DontCareInst Cmp_Rule_35_A1_P80")
  {}
  virtual ~NamedDontCareInst_Cmp_Rule_35_A1_P80() {}

 private:
  nacl_arm_dec::DontCareInst_Cmp_Rule_35_A1_P80 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Cmp_Rule_35_A1_P80);
};

class NamedDontCareInst_Cmp_Rule_36_A1_P82
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Cmp_Rule_36_A1_P82()
    : NamedClassDecoder(decoder_, "DontCareInst Cmp_Rule_36_A1_P82")
  {}
  virtual ~NamedDontCareInst_Cmp_Rule_36_A1_P82() {}

 private:
  nacl_arm_dec::DontCareInst_Cmp_Rule_36_A1_P82 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Cmp_Rule_36_A1_P82);
};

class NamedDontCareInst_Dbg_Rule_40_A1_P88
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Dbg_Rule_40_A1_P88()
    : NamedClassDecoder(decoder_, "DontCareInst Dbg_Rule_40_A1_P88")
  {}
  virtual ~NamedDontCareInst_Dbg_Rule_40_A1_P88() {}

 private:
  nacl_arm_dec::DontCareInst_Dbg_Rule_40_A1_P88 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Dbg_Rule_40_A1_P88);
};

class NamedDontCareInst_Msr_Rule_103_A1_P208
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Msr_Rule_103_A1_P208()
    : NamedClassDecoder(decoder_, "DontCareInst Msr_Rule_103_A1_P208")
  {}
  virtual ~NamedDontCareInst_Msr_Rule_103_A1_P208() {}

 private:
  nacl_arm_dec::DontCareInst_Msr_Rule_103_A1_P208 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Msr_Rule_103_A1_P208);
};

class NamedDontCareInst_Nop_Rule_110_A1_P222
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Nop_Rule_110_A1_P222()
    : NamedClassDecoder(decoder_, "DontCareInst Nop_Rule_110_A1_P222")
  {}
  virtual ~NamedDontCareInst_Nop_Rule_110_A1_P222() {}

 private:
  nacl_arm_dec::DontCareInst_Nop_Rule_110_A1_P222 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Nop_Rule_110_A1_P222);
};

class NamedDontCareInst_Teq_Rule_227_A1_P448
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Teq_Rule_227_A1_P448()
    : NamedClassDecoder(decoder_, "DontCareInst Teq_Rule_227_A1_P448")
  {}
  virtual ~NamedDontCareInst_Teq_Rule_227_A1_P448() {}

 private:
  nacl_arm_dec::DontCareInst_Teq_Rule_227_A1_P448 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Teq_Rule_227_A1_P448);
};

class NamedDontCareInst_Teq_Rule_228_A1_P450
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Teq_Rule_228_A1_P450()
    : NamedClassDecoder(decoder_, "DontCareInst Teq_Rule_228_A1_P450")
  {}
  virtual ~NamedDontCareInst_Teq_Rule_228_A1_P450() {}

 private:
  nacl_arm_dec::DontCareInst_Teq_Rule_228_A1_P450 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Teq_Rule_228_A1_P450);
};

class NamedDontCareInst_Tst_Rule_231_A1_P456
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Tst_Rule_231_A1_P456()
    : NamedClassDecoder(decoder_, "DontCareInst Tst_Rule_231_A1_P456")
  {}
  virtual ~NamedDontCareInst_Tst_Rule_231_A1_P456() {}

 private:
  nacl_arm_dec::DontCareInst_Tst_Rule_231_A1_P456 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Tst_Rule_231_A1_P456);
};

class NamedDontCareInst_Yield_Rule_413_A1_P812
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInst_Yield_Rule_413_A1_P812()
    : NamedClassDecoder(decoder_, "DontCareInst Yield_Rule_413_A1_P812")
  {}
  virtual ~NamedDontCareInst_Yield_Rule_413_A1_P812() {}

 private:
  nacl_arm_dec::DontCareInst_Yield_Rule_413_A1_P812 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Yield_Rule_413_A1_P812);
};

class NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78()
    : NamedClassDecoder(decoder_, "DontCareInstRnRsRmNotPc Cmn_Rule_34_A1_P78")
  {}
  virtual ~NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78() {}

 private:
  nacl_arm_dec::DontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78);
};

class NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84()
    : NamedClassDecoder(decoder_, "DontCareInstRnRsRmNotPc Cmp_Rule_37_A1_P84")
  {}
  virtual ~NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84() {}

 private:
  nacl_arm_dec::DontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84);
};

class NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452()
    : NamedClassDecoder(decoder_, "DontCareInstRnRsRmNotPc Teq_Rule_229_A1_P452")
  {}
  virtual ~NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452() {}

 private:
  nacl_arm_dec::DontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452);
};

class NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458
    : public NamedClassDecoder {
 public:
  inline NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458()
    : NamedClassDecoder(decoder_, "DontCareInstRnRsRmNotPc Tst_Rule_232_A1_P458")
  {}
  virtual ~NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458() {}

 private:
  nacl_arm_dec::DontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458);
};

class NamedForbidden_Bxj_Rule_26_A1_P64
    : public NamedClassDecoder {
 public:
  inline NamedForbidden_Bxj_Rule_26_A1_P64()
    : NamedClassDecoder(decoder_, "Forbidden Bxj_Rule_26_A1_P64")
  {}
  virtual ~NamedForbidden_Bxj_Rule_26_A1_P64() {}

 private:
  nacl_arm_dec::Forbidden_Bxj_Rule_26_A1_P64 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Bxj_Rule_26_A1_P64);
};

class NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12
    : public NamedClassDecoder {
 public:
  inline NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12()
    : NamedClassDecoder(decoder_, "Forbidden Msr_Rule_B6_1_6_A1_PB6_12")
  {}
  virtual ~NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12() {}

 private:
  nacl_arm_dec::Forbidden_Msr_Rule_B6_1_6_A1_PB6_12 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12);
};

class NamedForbidden_Msr_Rule_B6_1_7_P14
    : public NamedClassDecoder {
 public:
  inline NamedForbidden_Msr_Rule_B6_1_7_P14()
    : NamedClassDecoder(decoder_, "Forbidden Msr_Rule_B6_1_7_P14")
  {}
  virtual ~NamedForbidden_Msr_Rule_B6_1_7_P14() {}

 private:
  nacl_arm_dec::Forbidden_Msr_Rule_B6_1_7_P14 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Msr_Rule_B6_1_7_P14);
};

class NamedForbidden_Sev_Rule_158_A1_P316
    : public NamedClassDecoder {
 public:
  inline NamedForbidden_Sev_Rule_158_A1_P316()
    : NamedClassDecoder(decoder_, "Forbidden Sev_Rule_158_A1_P316")
  {}
  virtual ~NamedForbidden_Sev_Rule_158_A1_P316() {}

 private:
  nacl_arm_dec::Forbidden_Sev_Rule_158_A1_P316 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Sev_Rule_158_A1_P316);
};

class NamedForbidden_Smc_Rule_B6_1_9_P18
    : public NamedClassDecoder {
 public:
  inline NamedForbidden_Smc_Rule_B6_1_9_P18()
    : NamedClassDecoder(decoder_, "Forbidden Smc_Rule_B6_1_9_P18")
  {}
  virtual ~NamedForbidden_Smc_Rule_B6_1_9_P18() {}

 private:
  nacl_arm_dec::Forbidden_Smc_Rule_B6_1_9_P18 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Smc_Rule_B6_1_9_P18);
};

class NamedForbidden_Wfe_Rule_411_A1_P808
    : public NamedClassDecoder {
 public:
  inline NamedForbidden_Wfe_Rule_411_A1_P808()
    : NamedClassDecoder(decoder_, "Forbidden Wfe_Rule_411_A1_P808")
  {}
  virtual ~NamedForbidden_Wfe_Rule_411_A1_P808() {}

 private:
  nacl_arm_dec::Forbidden_Wfe_Rule_411_A1_P808 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Wfe_Rule_411_A1_P808);
};

class NamedForbidden_Wfi_Rule_412_A1_P810
    : public NamedClassDecoder {
 public:
  inline NamedForbidden_Wfi_Rule_412_A1_P810()
    : NamedClassDecoder(decoder_, "Forbidden Wfi_Rule_412_A1_P810")
  {}
  virtual ~NamedForbidden_Wfi_Rule_412_A1_P810() {}

 private:
  nacl_arm_dec::Forbidden_Wfi_Rule_412_A1_P810 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Wfi_Rule_412_A1_P810);
};

class NamedLdrImmediate_Ldr_Rule_58_A1_P120
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldr_Rule_58_A1_P120()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldr_Rule_58_A1_P120")
  {}
  virtual ~NamedLdrImmediate_Ldr_Rule_58_A1_P120() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldr_Rule_58_A1_P120 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldr_Rule_58_A1_P120);
};

class NamedLdrImmediate_Ldr_Rule_59_A1_P122
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldr_Rule_59_A1_P122()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldr_Rule_59_A1_P122")
  {}
  virtual ~NamedLdrImmediate_Ldr_Rule_59_A1_P122() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldr_Rule_59_A1_P122 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldr_Rule_59_A1_P122);
};

class NamedLdrImmediate_Ldrb_Rule_62_A1_P128
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldrb_Rule_62_A1_P128()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldrb_Rule_62_A1_P128")
  {}
  virtual ~NamedLdrImmediate_Ldrb_Rule_62_A1_P128() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldrb_Rule_62_A1_P128 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldrb_Rule_62_A1_P128);
};

class NamedLdrImmediate_Ldrb_Rule_63_A1_P130
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldrb_Rule_63_A1_P130()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldrb_Rule_63_A1_P130")
  {}
  virtual ~NamedLdrImmediate_Ldrb_Rule_63_A1_P130() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldrb_Rule_63_A1_P130 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldrb_Rule_63_A1_P130);
};

class NamedLdrImmediate_Ldrh_Rule_74_A1_P152
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldrh_Rule_74_A1_P152()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldrh_Rule_74_A1_P152")
  {}
  virtual ~NamedLdrImmediate_Ldrh_Rule_74_A1_P152() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldrh_Rule_74_A1_P152 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldrh_Rule_74_A1_P152);
};

class NamedLdrImmediate_Ldrh_Rule_75_A1_P154
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldrh_Rule_75_A1_P154()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldrh_Rule_75_A1_P154")
  {}
  virtual ~NamedLdrImmediate_Ldrh_Rule_75_A1_P154() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldrh_Rule_75_A1_P154 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldrh_Rule_75_A1_P154);
};

class NamedLdrImmediate_Ldrsb_Rule_78_A1_P160
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldrsb_Rule_78_A1_P160()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldrsb_Rule_78_A1_P160")
  {}
  virtual ~NamedLdrImmediate_Ldrsb_Rule_78_A1_P160() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldrsb_Rule_78_A1_P160 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldrsb_Rule_78_A1_P160);
};

class NamedLdrImmediate_Ldrsh_Rule_82_A1_P168
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldrsh_Rule_82_A1_P168()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldrsh_Rule_82_A1_P168")
  {}
  virtual ~NamedLdrImmediate_Ldrsh_Rule_82_A1_P168() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldrsh_Rule_82_A1_P168 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldrsh_Rule_82_A1_P168);
};

class NamedLdrImmediate_Ldrsh_Rule_83_A1_P170
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_Ldrsh_Rule_83_A1_P170()
    : NamedClassDecoder(decoder_, "LdrImmediate Ldrsh_Rule_83_A1_P170")
  {}
  virtual ~NamedLdrImmediate_Ldrsh_Rule_83_A1_P170() {}

 private:
  nacl_arm_dec::LdrImmediate_Ldrsh_Rule_83_A1_P170 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_Ldrsh_Rule_83_A1_P170);
};

class NamedLdrImmediate_ldrsb_Rule_79_A1_162
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediate_ldrsb_Rule_79_A1_162()
    : NamedClassDecoder(decoder_, "LdrImmediate ldrsb_Rule_79_A1_162")
  {}
  virtual ~NamedLdrImmediate_ldrsb_Rule_79_A1_162() {}

 private:
  nacl_arm_dec::LdrImmediate_ldrsb_Rule_79_A1_162 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediate_ldrsb_Rule_79_A1_162);
};

class NamedLdrImmediateDouble_Ldrd_Rule_66_A1_P136
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediateDouble_Ldrd_Rule_66_A1_P136()
    : NamedClassDecoder(decoder_, "LdrImmediateDouble Ldrd_Rule_66_A1_P136")
  {}
  virtual ~NamedLdrImmediateDouble_Ldrd_Rule_66_A1_P136() {}

 private:
  nacl_arm_dec::LdrImmediateDouble_Ldrd_Rule_66_A1_P136 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediateDouble_Ldrd_Rule_66_A1_P136);
};

class NamedLdrImmediateDouble_Ldrd_Rule_67_A1_P138
    : public NamedClassDecoder {
 public:
  inline NamedLdrImmediateDouble_Ldrd_Rule_67_A1_P138()
    : NamedClassDecoder(decoder_, "LdrImmediateDouble Ldrd_Rule_67_A1_P138")
  {}
  virtual ~NamedLdrImmediateDouble_Ldrd_Rule_67_A1_P138() {}

 private:
  nacl_arm_dec::LdrImmediateDouble_Ldrd_Rule_67_A1_P138 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediateDouble_Ldrd_Rule_67_A1_P138);
};

class NamedLdrRegister_Ldr_Rule_60_A1_P124
    : public NamedClassDecoder {
 public:
  inline NamedLdrRegister_Ldr_Rule_60_A1_P124()
    : NamedClassDecoder(decoder_, "LdrRegister Ldr_Rule_60_A1_P124")
  {}
  virtual ~NamedLdrRegister_Ldr_Rule_60_A1_P124() {}

 private:
  nacl_arm_dec::LdrRegister_Ldr_Rule_60_A1_P124 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrRegister_Ldr_Rule_60_A1_P124);
};

class NamedLdrRegister_Ldrb_Rule_64_A1_P132
    : public NamedClassDecoder {
 public:
  inline NamedLdrRegister_Ldrb_Rule_64_A1_P132()
    : NamedClassDecoder(decoder_, "LdrRegister Ldrb_Rule_64_A1_P132")
  {}
  virtual ~NamedLdrRegister_Ldrb_Rule_64_A1_P132() {}

 private:
  nacl_arm_dec::LdrRegister_Ldrb_Rule_64_A1_P132 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrRegister_Ldrb_Rule_64_A1_P132);
};

class NamedLdrRegister_Ldrh_Rule_76_A1_P156
    : public NamedClassDecoder {
 public:
  inline NamedLdrRegister_Ldrh_Rule_76_A1_P156()
    : NamedClassDecoder(decoder_, "LdrRegister Ldrh_Rule_76_A1_P156")
  {}
  virtual ~NamedLdrRegister_Ldrh_Rule_76_A1_P156() {}

 private:
  nacl_arm_dec::LdrRegister_Ldrh_Rule_76_A1_P156 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrRegister_Ldrh_Rule_76_A1_P156);
};

class NamedLdrRegister_Ldrsb_Rule_80_A1_P164
    : public NamedClassDecoder {
 public:
  inline NamedLdrRegister_Ldrsb_Rule_80_A1_P164()
    : NamedClassDecoder(decoder_, "LdrRegister Ldrsb_Rule_80_A1_P164")
  {}
  virtual ~NamedLdrRegister_Ldrsb_Rule_80_A1_P164() {}

 private:
  nacl_arm_dec::LdrRegister_Ldrsb_Rule_80_A1_P164 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrRegister_Ldrsb_Rule_80_A1_P164);
};

class NamedLdrRegister_Ldrsh_Rule_84_A1_P172
    : public NamedClassDecoder {
 public:
  inline NamedLdrRegister_Ldrsh_Rule_84_A1_P172()
    : NamedClassDecoder(decoder_, "LdrRegister Ldrsh_Rule_84_A1_P172")
  {}
  virtual ~NamedLdrRegister_Ldrsh_Rule_84_A1_P172() {}

 private:
  nacl_arm_dec::LdrRegister_Ldrsh_Rule_84_A1_P172 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrRegister_Ldrsh_Rule_84_A1_P172);
};

class NamedLdrRegisterDouble_Ldrd_Rule_68_A1_P140
    : public NamedClassDecoder {
 public:
  inline NamedLdrRegisterDouble_Ldrd_Rule_68_A1_P140()
    : NamedClassDecoder(decoder_, "LdrRegisterDouble Ldrd_Rule_68_A1_P140")
  {}
  virtual ~NamedLdrRegisterDouble_Ldrd_Rule_68_A1_P140() {}

 private:
  nacl_arm_dec::LdrRegisterDouble_Ldrd_Rule_68_A1_P140 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrRegisterDouble_Ldrd_Rule_68_A1_P140);
};

class NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142
    : public NamedClassDecoder {
 public:
  inline NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142()
    : NamedClassDecoder(decoder_, "LoadBasedMemory Ldrex_Rule_69_A1_P142")
  {}
  virtual ~NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142() {}

 private:
  nacl_arm_dec::LoadBasedMemory_Ldrex_Rule_69_A1_P142 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142);
};

class NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144
    : public NamedClassDecoder {
 public:
  inline NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144()
    : NamedClassDecoder(decoder_, "LoadBasedMemory Ldrexb_Rule_70_A1_P144")
  {}
  virtual ~NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144() {}

 private:
  nacl_arm_dec::LoadBasedMemory_Ldrexb_Rule_70_A1_P144 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144);
};

class NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148
    : public NamedClassDecoder {
 public:
  inline NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148()
    : NamedClassDecoder(decoder_, "LoadBasedMemory Ldrexh_Rule_72_A1_P148")
  {}
  virtual ~NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148() {}

 private:
  nacl_arm_dec::LoadBasedMemory_Ldrexh_Rule_72_A1_P148 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148);
};

class NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146
    : public NamedClassDecoder {
 public:
  inline NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146()
    : NamedClassDecoder(decoder_, "LoadBasedMemoryDouble Ldrexd_Rule_71_A1_P146")
  {}
  virtual ~NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146() {}

 private:
  nacl_arm_dec::LoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146);
};

class NamedMaskAddress_Bic_Rule_19_A1_P50
    : public NamedClassDecoder {
 public:
  inline NamedMaskAddress_Bic_Rule_19_A1_P50()
    : NamedClassDecoder(decoder_, "MaskAddress Bic_Rule_19_A1_P50")
  {}
  virtual ~NamedMaskAddress_Bic_Rule_19_A1_P50() {}

 private:
  nacl_arm_dec::MaskAddress_Bic_Rule_19_A1_P50 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMaskAddress_Bic_Rule_19_A1_P50);
};

class NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404
    : public NamedClassDecoder {
 public:
  inline NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404()
    : NamedClassDecoder(decoder_, "StoreBasedMemoryDoubleRtBits0To3 Strexd_Rule_204_A1_P404")
  {}
  virtual ~NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404() {}

 private:
  nacl_arm_dec::StoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404);
};

class NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400
    : public NamedClassDecoder {
 public:
  inline NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400()
    : NamedClassDecoder(decoder_, "StoreBasedMemoryRtBits0To3 Strex_Rule_202_A1_P400")
  {}
  virtual ~NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400() {}

 private:
  nacl_arm_dec::StoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400);
};

class NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402
    : public NamedClassDecoder {
 public:
  inline NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402()
    : NamedClassDecoder(decoder_, "StoreBasedMemoryRtBits0To3 Strexb_Rule_203_A1_P402")
  {}
  virtual ~NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402() {}

 private:
  nacl_arm_dec::StoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402);
};

class NamedStoreBasedMemoryRtBits0To3_cccc00011110nnnndddd11111001tttt
    : public NamedClassDecoder {
 public:
  inline NamedStoreBasedMemoryRtBits0To3_cccc00011110nnnndddd11111001tttt()
    : NamedClassDecoder(decoder_, "StoreBasedMemoryRtBits0To3 cccc00011110nnnndddd11111001tttt")
  {}
  virtual ~NamedStoreBasedMemoryRtBits0To3_cccc00011110nnnndddd11111001tttt() {}

 private:
  nacl_arm_dec::StoreBasedMemoryRtBits0To3_cccc00011110nnnndddd11111001tttt decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedMemoryRtBits0To3_cccc00011110nnnndddd11111001tttt);
};

class NamedStrImmediate_Str_Rule_194_A1_P384
    : public NamedClassDecoder {
 public:
  inline NamedStrImmediate_Str_Rule_194_A1_P384()
    : NamedClassDecoder(decoder_, "StrImmediate Str_Rule_194_A1_P384")
  {}
  virtual ~NamedStrImmediate_Str_Rule_194_A1_P384() {}

 private:
  nacl_arm_dec::StrImmediate_Str_Rule_194_A1_P384 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStrImmediate_Str_Rule_194_A1_P384);
};

class NamedStrImmediate_Strb_Rule_197_A1_P390
    : public NamedClassDecoder {
 public:
  inline NamedStrImmediate_Strb_Rule_197_A1_P390()
    : NamedClassDecoder(decoder_, "StrImmediate Strb_Rule_197_A1_P390")
  {}
  virtual ~NamedStrImmediate_Strb_Rule_197_A1_P390() {}

 private:
  nacl_arm_dec::StrImmediate_Strb_Rule_197_A1_P390 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStrImmediate_Strb_Rule_197_A1_P390);
};

class NamedStrImmediate_Strh_Rule_207_A1_P410
    : public NamedClassDecoder {
 public:
  inline NamedStrImmediate_Strh_Rule_207_A1_P410()
    : NamedClassDecoder(decoder_, "StrImmediate Strh_Rule_207_A1_P410")
  {}
  virtual ~NamedStrImmediate_Strh_Rule_207_A1_P410() {}

 private:
  nacl_arm_dec::StrImmediate_Strh_Rule_207_A1_P410 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStrImmediate_Strh_Rule_207_A1_P410);
};

class NamedStrImmediateDouble_Strd_Rule_200_A1_P396
    : public NamedClassDecoder {
 public:
  inline NamedStrImmediateDouble_Strd_Rule_200_A1_P396()
    : NamedClassDecoder(decoder_, "StrImmediateDouble Strd_Rule_200_A1_P396")
  {}
  virtual ~NamedStrImmediateDouble_Strd_Rule_200_A1_P396() {}

 private:
  nacl_arm_dec::StrImmediateDouble_Strd_Rule_200_A1_P396 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStrImmediateDouble_Strd_Rule_200_A1_P396);
};

class NamedStrRegister_Str_Rule_195_A1_P386
    : public NamedClassDecoder {
 public:
  inline NamedStrRegister_Str_Rule_195_A1_P386()
    : NamedClassDecoder(decoder_, "StrRegister Str_Rule_195_A1_P386")
  {}
  virtual ~NamedStrRegister_Str_Rule_195_A1_P386() {}

 private:
  nacl_arm_dec::StrRegister_Str_Rule_195_A1_P386 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStrRegister_Str_Rule_195_A1_P386);
};

class NamedStrRegister_Strb_Rule_198_A1_P392
    : public NamedClassDecoder {
 public:
  inline NamedStrRegister_Strb_Rule_198_A1_P392()
    : NamedClassDecoder(decoder_, "StrRegister Strb_Rule_198_A1_P392")
  {}
  virtual ~NamedStrRegister_Strb_Rule_198_A1_P392() {}

 private:
  nacl_arm_dec::StrRegister_Strb_Rule_198_A1_P392 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStrRegister_Strb_Rule_198_A1_P392);
};

class NamedStrRegister_Strh_Rule_208_A1_P412
    : public NamedClassDecoder {
 public:
  inline NamedStrRegister_Strh_Rule_208_A1_P412()
    : NamedClassDecoder(decoder_, "StrRegister Strh_Rule_208_A1_P412")
  {}
  virtual ~NamedStrRegister_Strh_Rule_208_A1_P412() {}

 private:
  nacl_arm_dec::StrRegister_Strh_Rule_208_A1_P412 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStrRegister_Strh_Rule_208_A1_P412);
};

class NamedStrRegisterDouble_Strd_Rule_201_A1_P398
    : public NamedClassDecoder {
 public:
  inline NamedStrRegisterDouble_Strd_Rule_201_A1_P398()
    : NamedClassDecoder(decoder_, "StrRegisterDouble Strd_Rule_201_A1_P398")
  {}
  virtual ~NamedStrRegisterDouble_Strd_Rule_201_A1_P398() {}

 private:
  nacl_arm_dec::StrRegisterDouble_Strd_Rule_201_A1_P398 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStrRegisterDouble_Strd_Rule_201_A1_P398);
};

class NamedTestIfAddressMasked_Tst_Rule_230_A1_P454
    : public NamedClassDecoder {
 public:
  inline NamedTestIfAddressMasked_Tst_Rule_230_A1_P454()
    : NamedClassDecoder(decoder_, "TestIfAddressMasked Tst_Rule_230_A1_P454")
  {}
  virtual ~NamedTestIfAddressMasked_Tst_Rule_230_A1_P454() {}

 private:
  nacl_arm_dec::TestIfAddressMasked_Tst_Rule_230_A1_P454 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedTestIfAddressMasked_Tst_Rule_230_A1_P454);
};

class NamedVfpOp_Vadd_Rule_271_A2_P536
    : public NamedClassDecoder {
 public:
  inline NamedVfpOp_Vadd_Rule_271_A2_P536()
    : NamedClassDecoder(decoder_, "VfpOp Vadd_Rule_271_A2_P536")
  {}
  virtual ~NamedVfpOp_Vadd_Rule_271_A2_P536() {}

 private:
  nacl_arm_dec::VfpOp_Vadd_Rule_271_A2_P536 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vadd_Rule_271_A2_P536);
};

class NamedVfpOp_Vdiv_Rule_301_A1_P590
    : public NamedClassDecoder {
 public:
  inline NamedVfpOp_Vdiv_Rule_301_A1_P590()
    : NamedClassDecoder(decoder_, "VfpOp Vdiv_Rule_301_A1_P590")
  {}
  virtual ~NamedVfpOp_Vdiv_Rule_301_A1_P590() {}

 private:
  nacl_arm_dec::VfpOp_Vdiv_Rule_301_A1_P590 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vdiv_Rule_301_A1_P590);
};

class NamedVfpOp_Vm_la_ls_Rule_423_A2_P636
    : public NamedClassDecoder {
 public:
  inline NamedVfpOp_Vm_la_ls_Rule_423_A2_P636()
    : NamedClassDecoder(decoder_, "VfpOp Vm_la_ls_Rule_423_A2_P636")
  {}
  virtual ~NamedVfpOp_Vm_la_ls_Rule_423_A2_P636() {}

 private:
  nacl_arm_dec::VfpOp_Vm_la_ls_Rule_423_A2_P636 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vm_la_ls_Rule_423_A2_P636);
};

class NamedVfpOp_Vmul_Rule_338_A2_P664
    : public NamedClassDecoder {
 public:
  inline NamedVfpOp_Vmul_Rule_338_A2_P664()
    : NamedClassDecoder(decoder_, "VfpOp Vmul_Rule_338_A2_P664")
  {}
  virtual ~NamedVfpOp_Vmul_Rule_338_A2_P664() {}

 private:
  nacl_arm_dec::VfpOp_Vmul_Rule_338_A2_P664 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vmul_Rule_338_A2_P664);
};

class NamedVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674
    : public NamedClassDecoder {
 public:
  inline NamedVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674()
    : NamedClassDecoder(decoder_, "VfpOp Vnm_la_ls_ul_Rule_343_A1_P674")
  {}
  virtual ~NamedVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674() {}

 private:
  nacl_arm_dec::VfpOp_Vnm_la_ls_ul_Rule_343_A1_P674 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674);
};

class NamedVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674
    : public NamedClassDecoder {
 public:
  inline NamedVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674()
    : NamedClassDecoder(decoder_, "VfpOp Vnm_la_ls_ul_Rule_343_A2_P674")
  {}
  virtual ~NamedVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674() {}

 private:
  nacl_arm_dec::VfpOp_Vnm_la_ls_ul_Rule_343_A2_P674 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674);
};

class NamedVfpOp_Vsub_Rule_402_A2_P790
    : public NamedClassDecoder {
 public:
  inline NamedVfpOp_Vsub_Rule_402_A2_P790()
    : NamedClassDecoder(decoder_, "VfpOp Vsub_Rule_402_A2_P790")
  {}
  virtual ~NamedVfpOp_Vsub_Rule_402_A2_P790() {}

 private:
  nacl_arm_dec::VfpOp_Vsub_Rule_402_A2_P790 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vsub_Rule_402_A2_P790);
};


// Defines the default parse action if the table doesn't define
// an action.
class NotImplementedNamed : public NamedClassDecoder {
 public:
  inline NotImplementedNamed()
    : NamedClassDecoder(decoder_, "not implemented")
  {}
  virtual ~NotImplementedNamed() {}

 private:
  nacl_arm_dec::NotImplemented decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NotImplementedNamed);
};

} // namespace nacl_arm_test
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_CLASSES_H_
