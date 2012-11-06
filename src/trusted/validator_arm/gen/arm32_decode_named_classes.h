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

class Binary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48
    : public Binary2RegisterBitRangeMsbGeLsb {
};

class Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {
};

class Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {
};

class Binary2RegisterImmedShiftedTest_CMN_register
    : public Binary2RegisterImmedShiftedTest {
};

class Binary2RegisterImmedShiftedTest_CMP_register
    : public Binary2RegisterImmedShiftedTest {
};

class Binary2RegisterImmedShiftedTest_TEQ_register
    : public Binary2RegisterImmedShiftedTest {
};

class Binary2RegisterImmedShiftedTest_TST_register
    : public Binary2RegisterImmedShiftedTest {
};

class Binary2RegisterImmediateOp_ADC_immediate
    : public Binary2RegisterImmediateOp {
};

class Binary2RegisterImmediateOp_AND_immediate
    : public Binary2RegisterImmediateOp {
};

class Binary2RegisterImmediateOp_EOR_immediate
    : public Binary2RegisterImmediateOp {
};

class Binary2RegisterImmediateOp_RSB_immediate
    : public Binary2RegisterImmediateOp {
};

class Binary2RegisterImmediateOp_RSC_immediate
    : public Binary2RegisterImmediateOp {
};

class Binary2RegisterImmediateOp_SBC_immediate
    : public Binary2RegisterImmediateOp {
};

class Binary2RegisterImmediateOpAddSub_ADD_immediate
    : public Binary2RegisterImmediateOpAddSub {
};

class Binary2RegisterImmediateOpAddSub_SUB_immediate
    : public Binary2RegisterImmediateOpAddSub {
};

class Binary2RegisterImmediateOpDynCodeReplace_ORR_immediate
    : public Binary2RegisterImmediateOpDynCodeReplace {
};

class Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234
    : public Binary3RegisterImmedShiftedOpRegsNotPc {
};

class Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436
    : public Binary3RegisterImmedShiftedOpRegsNotPc {
};

class Binary3RegisterOp_ASR_register
    : public Binary3RegisterOp {
};

class Binary3RegisterOp_LSL_register
    : public Binary3RegisterOp {
};

class Binary3RegisterOp_LSR_register
    : public Binary3RegisterOp {
};

class Binary3RegisterOp_ROR_register
    : public Binary3RegisterOp {
};

class Binary3RegisterOpAltA_MUL_A1
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_Sdiv_Rule_A1
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_Smmul_Rule_176_P350
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_Smuad_Rule_177_P352
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_Smulxx_Rule_178_P354
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_Smusd_Rule_181_P360
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_Udiv_Rule_A1
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltBNoCondUpdates_QADD
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QDADD
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QDSUB
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QSUB
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterShiftedOp_ADC_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_ADD_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_AND_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_BIC_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_EOR_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_ORR_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_RSB_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_RSC_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_SBC_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedOp_SUB_register
    : public Binary3RegisterShiftedOp {
};

class Binary3RegisterShiftedTest_CMN_register_shifted_register
    : public Binary3RegisterShiftedTest {
};

class Binary3RegisterShiftedTest_CMP_register_shifted_register
    : public Binary3RegisterShiftedTest {
};

class Binary3RegisterShiftedTest_TEQ_register_shifted_register
    : public Binary3RegisterShiftedTest {
};

class Binary3RegisterShiftedTest_TST_register_shifted_register
    : public Binary3RegisterShiftedTest {
};

class Binary4RegisterDualOp_MLS_A1
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_Smlad_Rule_167_P332
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_Smlawx_Rule_171_A1_340
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_Smlsd_Rule_172_P342
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_Smmla_Rule_174_P346
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_Smmls_Rule_175_P348
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_Usada8_Rule_254_A1_P502
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOpLtV6RdNotRn_MLA_A1
    : public Binary4RegisterDualOpLtV6RdNotRn {
};

class Binary4RegisterDualResult_UMAAL_A1
    : public Binary4RegisterDualResult {
};

class Binary4RegisterDualResult_Smlald_Rule_170_P336
    : public Binary4RegisterDualResult {
};

class Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336
    : public Binary4RegisterDualResult {
};

class Binary4RegisterDualResult_Smlsld_Rule_173_P344
    : public Binary4RegisterDualResult {
};

class Binary4RegisterDualResultLtV6RdHiLoNotRn_SMLAL_A1
    : public Binary4RegisterDualResultLtV6RdHiLoNotRn {
};

class Binary4RegisterDualResultLtV6RdHiLoNotRn_UMLAL_A1
    : public Binary4RegisterDualResultLtV6RdHiLoNotRn {
};

class Binary4RegisterDualResultUsesRnRm_SMULL_A1
    : public Binary4RegisterDualResultUsesRnRm {
};

class Binary4RegisterDualResultUsesRnRm_UMULL_A1
    : public Binary4RegisterDualResultUsesRnRm {
};

class Binary4RegisterShiftedOp_ADC_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_ADD_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_AND_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_BIC_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_EOR_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_ORR_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_RSB_register_shfited_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_RSC_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_SBC_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class Binary4RegisterShiftedOp_SUB_register_shifted_register
    : public Binary4RegisterShiftedOp {
};

class BinaryRegisterImmediateTest_CMN_immediate
    : public BinaryRegisterImmediateTest {
};

class BinaryRegisterImmediateTest_CMP_immediate
    : public BinaryRegisterImmediateTest {
};

class BinaryRegisterImmediateTest_TEQ_immediate
    : public BinaryRegisterImmediateTest {
};

class BranchImmediate24_B_Rule_16_A1_P44
    : public BranchImmediate24 {
};

class BranchImmediate24_Bl_Blx_Rule_23_A1_P58
    : public BranchImmediate24 {
};

class BranchToRegister_Blx_Rule_24_A1_P60
    : public BranchToRegister {
};

class BranchToRegister_Bx_Rule_25_A1_P62
    : public BranchToRegister {
};

class BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56
    : public BreakPointAndConstantPoolHead {
};

class CondDecoder_Nop_Rule_110_A1_P222
    : public CondDecoder {
};

class CondDecoder_Yield_Rule_413_A1_P812
    : public CondDecoder {
};

class CondVfpOp_Vabs_Rule_269_A2_P532
    : public CondVfpOp {
};

class CondVfpOp_Vadd_Rule_271_A2_P536
    : public CondVfpOp {
};

class CondVfpOp_Vcmp_Vcmpe_Rule_A1
    : public CondVfpOp {
};

class CondVfpOp_Vcmp_Vcmpe_Rule_A2
    : public CondVfpOp {
};

class CondVfpOp_Vcvt_Rule_297_A1_P582
    : public CondVfpOp {
};

class CondVfpOp_Vcvt_Rule_298_A1_P584
    : public CondVfpOp {
};

class CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578
    : public CondVfpOp {
};

class CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588
    : public CondVfpOp {
};

class CondVfpOp_Vdiv_Rule_301_A1_P590
    : public CondVfpOp {
};

class CondVfpOp_Vfma_vfms_Rule_A1
    : public CondVfpOp {
};

class CondVfpOp_Vfnma_vfnms_Rule_A1
    : public CondVfpOp {
};

class CondVfpOp_Vmla_vmls_Rule_423_A2_P636
    : public CondVfpOp {
};

class CondVfpOp_Vmov_Rule_326_A2_P640
    : public CondVfpOp {
};

class CondVfpOp_Vmov_Rule_327_A2_P642
    : public CondVfpOp {
};

class CondVfpOp_Vmul_Rule_338_A2_P664
    : public CondVfpOp {
};

class CondVfpOp_Vneg_Rule_342_A2_P672
    : public CondVfpOp {
};

class CondVfpOp_Vnmla_vnmls_Rule_343_A1_P674
    : public CondVfpOp {
};

class CondVfpOp_Vnmul_Rule_343_A2_P674
    : public CondVfpOp {
};

class CondVfpOp_Vsqrt_Rule_388_A1_P762
    : public CondVfpOp {
};

class CondVfpOp_Vsub_Rule_402_A2_P790
    : public CondVfpOp {
};

class DataBarrier_Dmb_Rule_41_A1_P90
    : public DataBarrier {
};

class DataBarrier_Dsb_Rule_42_A1_P92
    : public DataBarrier {
};

class Deprecated_Swp_Swpb_Rule_A1
    : public Deprecated {
};

class DuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594
    : public DuplicateToAdvSIMDRegisters {
};

class Forbidden_None
    : public Forbidden {
};

class ForbiddenCondDecoder_Bxj_Rule_26_A1_P64
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Cdp_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Dbg_Rule_40_A1_P88
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Eret_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Hvc_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldc_immediate_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldc_literal_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldrt_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldrt_Rule_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldrtb_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldrtb_Rule_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Mcr_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Mcrr_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Mrc_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Mrrc_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Sev_Rule_158_A1_P316
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Smc_Rule_B6_1_9_P18
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Stc_Rule_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Strt_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Strt_Rule_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Strtb_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Strtb_Rule_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Svc_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Wfe_Rule_411_A1_P808
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Wfi_Rule_412_A1_P810
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged
    : public ForbiddenCondDecoder {
};

class ForbiddenUncondDecoder_Blx_Rule_23_A2_P58
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Clrex_Rule_30_A1_P70
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Setend_Rule_157_P314
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Stc2_Rule_188_A2_P372
    : public ForbiddenUncondDecoder {
};

class ForbiddenUncondDecoder_Unallocated_hints
    : public ForbiddenUncondDecoder {
};

class InstructionBarrier_Isb_Rule_49_A1_P102
    : public InstructionBarrier {
};

class LdrImmediateOp_Ldr_Rule_58_A1_P120
    : public LdrImmediateOp {
};

class Load2RegisterImm12Op_Ldr_Rule_59_A1_P122
    : public Load2RegisterImm12Op {
};

class Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128
    : public Load2RegisterImm12Op {
};

class Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130
    : public Load2RegisterImm12Op {
};

class Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136
    : public Load2RegisterImm8DoubleOp {
};

class Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138
    : public Load2RegisterImm8DoubleOp {
};

class Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152
    : public Load2RegisterImm8Op {
};

class Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154
    : public Load2RegisterImm8Op {
};

class Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160
    : public Load2RegisterImm8Op {
};

class Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168
    : public Load2RegisterImm8Op {
};

class Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170
    : public Load2RegisterImm8Op {
};

class Load2RegisterImm8Op_ldrsb_Rule_79_A1_162
    : public Load2RegisterImm8Op {
};

class Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140
    : public Load3RegisterDoubleOp {
};

class Load3RegisterImm5Op_Ldr_Rule_60_A1_P124
    : public Load3RegisterImm5Op {
};

class Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132
    : public Load3RegisterImm5Op {
};

class Load3RegisterOp_Ldrh_Rule_76_A1_P156
    : public Load3RegisterOp {
};

class Load3RegisterOp_Ldrsb_Rule_80_A1_P164
    : public Load3RegisterOp {
};

class Load3RegisterOp_Ldrsh_Rule_84_A1_P172
    : public Load3RegisterOp {
};

class LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146
    : public LoadExclusive2RegisterDoubleOp {
};

class LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142
    : public LoadExclusive2RegisterOp {
};

class LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144
    : public LoadExclusive2RegisterOp {
};

class LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148
    : public LoadExclusive2RegisterOp {
};

class LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110
    : public LoadRegisterList {
};

class LoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112
    : public LoadRegisterList {
};

class LoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114
    : public LoadRegisterList {
};

class LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116
    : public LoadRegisterList {
};

class LoadRegisterList_Pop_Rule_A1
    : public LoadRegisterList {
};

class LoadVectorRegister_Vldr_Rule_320_A1_A2_P628
    : public LoadVectorRegister {
};

class LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626
    : public LoadVectorRegisterList {
};

class LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626
    : public LoadVectorRegisterList {
};

class LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694
    : public LoadVectorRegisterList {
};

class MaskedBinary2RegisterImmediateOp_BIC_immediate
    : public MaskedBinary2RegisterImmediateOp {
};

class MaskedBinaryRegisterImmediateTest_TST_immediate
    : public MaskedBinaryRegisterImmediateTest {
};

class MoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1
    : public MoveDoubleVfpRegisterOp {
};

class MoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1
    : public MoveDoubleVfpRegisterOp {
};

class MoveImmediate12ToApsr_Msr_Rule_103_A1_P208
    : public MoveImmediate12ToApsr {
};

class MoveVfpRegisterOp_Vmov_Rule_330_A1_P648
    : public MoveVfpRegisterOp {
};

class MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644
    : public MoveVfpRegisterOpWithTypeSel {
};

class MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646
    : public MoveVfpRegisterOpWithTypeSel {
};

class PermanentlyUndefined_Udf_Rule_A1
    : public PermanentlyUndefined {
};

class PreloadRegisterImm12Op_Pld_Rule_117_A1_P236
    : public PreloadRegisterImm12Op {
};

class PreloadRegisterImm12Op_Pld_Rule_118_A1_P238
    : public PreloadRegisterImm12Op {
};

class PreloadRegisterImm12Op_Pldw_Rule_117_A1_P236
    : public PreloadRegisterImm12Op {
};

class PreloadRegisterImm12Op_Pli_Rule_120_A1_P242
    : public PreloadRegisterImm12Op {
};

class PreloadRegisterPairOp_Pld_Rule_119_A1_P240
    : public PreloadRegisterPairOp {
};

class PreloadRegisterPairOp_Pldw_Rule_119_A1_P240
    : public PreloadRegisterPairOp {
};

class PreloadRegisterPairOp_Pli_Rule_121_A1_P244
    : public PreloadRegisterPairOp {
};

class Store2RegisterImm12Op_Str_Rule_194_A1_P384
    : public Store2RegisterImm12Op {
};

class Store2RegisterImm12Op_Strb_Rule_197_A1_P390
    : public Store2RegisterImm12Op {
};

class Store2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248
    : public Store2RegisterImm12OpRnNotRtOnWriteback {
};

class Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396
    : public Store2RegisterImm8DoubleOp {
};

class Store2RegisterImm8Op_Strh_Rule_207_A1_P410
    : public Store2RegisterImm8Op {
};

class Store3RegisterDoubleOp_Strd_Rule_201_A1_P398
    : public Store3RegisterDoubleOp {
};

class Store3RegisterImm5Op_Str_Rule_195_A1_P386
    : public Store3RegisterImm5Op {
};

class Store3RegisterImm5Op_Strb_Rule_198_A1_P392
    : public Store3RegisterImm5Op {
};

class Store3RegisterOp_Strh_Rule_208_A1_P412
    : public Store3RegisterOp {
};

class StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404
    : public StoreExclusive3RegisterDoubleOp {
};

class StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400
    : public StoreExclusive3RegisterOp {
};

class StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402
    : public StoreExclusive3RegisterOp {
};

class StoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406
    : public StoreExclusive3RegisterOp {
};

class StoreRegisterList_Push_Rule_A1
    : public StoreRegisterList {
};

class StoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374
    : public StoreRegisterList {
};

class StoreRegisterList_Stmda_Stmed_Rule_190_A1_P376
    : public StoreRegisterList {
};

class StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378
    : public StoreRegisterList {
};

class StoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380
    : public StoreRegisterList {
};

class StoreVectorRegister_Vstr_Rule_400_A1_A2_P786
    : public StoreVectorRegister {
};

class StoreVectorRegisterList_Vpush_355_A1_A2_P696
    : public StoreVectorRegisterList {
};

class StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784
    : public StoreVectorRegisterList {
};

class Unary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46
    : public Unary1RegisterBitRangeMsbGeLsb {
};

class Unary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1
    : public Unary1RegisterImmediateOp12DynCodeReplace {
};

class Unary1RegisterImmediateOp12DynCodeReplace_MVN_immediate
    : public Unary1RegisterImmediateOp12DynCodeReplace {
};

class Unary1RegisterImmediateOpDynCodeReplace_Movt_Rule_99_A1_P200
    : public Unary1RegisterImmediateOpDynCodeReplace {
};

class Unary1RegisterImmediateOpDynCodeReplace_Movw_Rule_96_A2_P194
    : public Unary1RegisterImmediateOpDynCodeReplace {
};

class Unary1RegisterImmediateOpPc_ADR_A1
    : public Unary1RegisterImmediateOpPc {
};

class Unary1RegisterImmediateOpPc_ADR_A2
    : public Unary1RegisterImmediateOpPc {
};

class Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10
    : public Unary1RegisterSet {
};

class Unary1RegisterUse_Msr_Rule_104_A1_P210
    : public Unary1RegisterUse {
};

class Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442
    : public Unary2RegisterImmedShiftedOpRegsNotPc {
};

class Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440
    : public Unary2RegisterImmedShiftedOpRegsNotPc {
};

class Unary2RegisterOp_MOV_register
    : public Unary2RegisterOp {
};

class Unary2RegisterOp_RRX
    : public Unary2RegisterOp {
};

class Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72
    : public Unary2RegisterOpNotRmIsPc {
};

class Unary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270
    : public Unary2RegisterOpNotRmIsPcNoCondUpdates {
};

class Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274
    : public Unary2RegisterOpNotRmIsPcNoCondUpdates {
};

class Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272
    : public Unary2RegisterOpNotRmIsPcNoCondUpdates {
};

class Unary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276
    : public Unary2RegisterOpNotRmIsPcNoCondUpdates {
};

class Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444
    : public Unary2RegisterOpNotRmIsPcNoCondUpdates {
};

class Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522
    : public Unary2RegisterOpNotRmIsPcNoCondUpdates {
};

class Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520
    : public Unary2RegisterOpNotRmIsPcNoCondUpdates {
};

class Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524
    : public Unary2RegisterOpNotRmIsPcNoCondUpdates {
};

class Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364
    : public Unary2RegisterSatImmedShiftedOp {
};

class Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362
    : public Unary2RegisterSatImmedShiftedOp {
};

class Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506
    : public Unary2RegisterSatImmedShiftedOp {
};

class Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504
    : public Unary2RegisterSatImmedShiftedOp {
};

class Unary2RegisterShiftedOp_ASR_immediate
    : public Unary2RegisterShiftedOp {
};

class Unary2RegisterShiftedOp_LSR_immediate
    : public Unary2RegisterShiftedOp {
};

class Unary2RegisterShiftedOp_MVN_register
    : public Unary2RegisterShiftedOp {
};

class Unary2RegisterShiftedOpImmNotZero_LSL_immediate
    : public Unary2RegisterShiftedOpImmNotZero {
};

class Unary2RegisterShiftedOpImmNotZero_ROR_immediate
    : public Unary2RegisterShiftedOpImmNotZero {
};

class Unary3RegisterShiftedOp_MVN_register_shifted_register
    : public Unary3RegisterShiftedOp {
};

class Undefined_None
    : public Undefined {
};

class UndefinedCondDecoder_Undefined_A5_6
    : public UndefinedCondDecoder {
};

class UndefinedCondDecoder_None
    : public UndefinedCondDecoder {
};

class UnpredictableUncondDecoder_Unpredictable
    : public UnpredictableUncondDecoder {
};

class Vector1RegisterImmediate_BIT_VBIC_immediate
    : public Vector1RegisterImmediate_BIT {
};

class Vector1RegisterImmediate_BIT_VORR_immediate
    : public Vector1RegisterImmediate_BIT {
};

class Vector1RegisterImmediate_MOV_VMOV_immediate_A1
    : public Vector1RegisterImmediate_MOV {
};

class Vector1RegisterImmediate_MVN_VMVN_immediate
    : public Vector1RegisterImmediate_MVN {
};

class Vector2RegisterMiscellaneous_CVT_F2I_VCVT
    : public Vector2RegisterMiscellaneous_CVT_F2I {
};

class Vector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision
    : public Vector2RegisterMiscellaneous_CVT_H2S {
};

class Vector2RegisterMiscellaneous_F32_VABS_A1
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_F32_VCEQ_immediate_0
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_F32_VCGE_immediate_0
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_F32_VCGT_immediate_0
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_F32_VCLE_immediate_0
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_F32_VCLT_immediate_0
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_F32_VNEG
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_F32_VRECPE
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_F32_VRSQRTE
    : public Vector2RegisterMiscellaneous_F32 {
};

class Vector2RegisterMiscellaneous_I16_32_64N_VQMOVN
    : public Vector2RegisterMiscellaneous_I16_32_64N {
};

class Vector2RegisterMiscellaneous_I16_32_64N_VQMOVUN
    : public Vector2RegisterMiscellaneous_I16_32_64N {
};

class Vector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2
    : public Vector2RegisterMiscellaneous_I8_16_32L {
};

class Vector2RegisterMiscellaneous_RG_VREV16
    : public Vector2RegisterMiscellaneous_RG {
};

class Vector2RegisterMiscellaneous_RG_VREV32
    : public Vector2RegisterMiscellaneous_RG {
};

class Vector2RegisterMiscellaneous_RG_VREV64
    : public Vector2RegisterMiscellaneous_RG {
};

class Vector2RegisterMiscellaneous_V16_32_64N_VMOVN
    : public Vector2RegisterMiscellaneous_V16_32_64N {
};

class Vector2RegisterMiscellaneous_V8_VCNT
    : public Vector2RegisterMiscellaneous_V8 {
};

class Vector2RegisterMiscellaneous_V8_VMVN_register
    : public Vector2RegisterMiscellaneous_V8 {
};

class Vector2RegisterMiscellaneous_V8S_VSWP
    : public Vector2RegisterMiscellaneous_V8S {
};

class Vector2RegisterMiscellaneous_V8_16_32_VABS_A1
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VCLS
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VCLZ
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VNEG
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VPADAL
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VPADDL
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VQABS
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32_VQNEG
    : public Vector2RegisterMiscellaneous_V8_16_32 {
};

class Vector2RegisterMiscellaneous_V8_16_32I_VUZP
    : public Vector2RegisterMiscellaneous_V8_16_32I {
};

class Vector2RegisterMiscellaneous_V8_16_32I_VZIP
    : public Vector2RegisterMiscellaneous_V8_16_32I {
};

class Vector2RegisterMiscellaneous_V8_16_32T_VTRN
    : public Vector2RegisterMiscellaneous_V8_16_32T {
};

class VectorBinary2RegisterScalar_F32_VMLA_by_scalar_A1
    : public VectorBinary2RegisterScalar_F32 {
};

class VectorBinary2RegisterScalar_F32_VMLS_by_scalar_A1
    : public VectorBinary2RegisterScalar_F32 {
};

class VectorBinary2RegisterScalar_F32_VMUL_by_scalar_A1
    : public VectorBinary2RegisterScalar_F32 {
};

class VectorBinary2RegisterScalar_I16_32_VMLA_by_scalar_A1
    : public VectorBinary2RegisterScalar_I16_32 {
};

class VectorBinary2RegisterScalar_I16_32_VMLS_by_scalar_A1
    : public VectorBinary2RegisterScalar_I16_32 {
};

class VectorBinary2RegisterScalar_I16_32_VMUL_by_scalar_A1
    : public VectorBinary2RegisterScalar_I16_32 {
};

class VectorBinary2RegisterScalar_I16_32_VQDMULH_A2
    : public VectorBinary2RegisterScalar_I16_32 {
};

class VectorBinary2RegisterScalar_I16_32_VQRDMULH
    : public VectorBinary2RegisterScalar_I16_32 {
};

class VectorBinary2RegisterScalar_I16_32L_VMLAL_by_scalar_A2
    : public VectorBinary2RegisterScalar_I16_32L {
};

class VectorBinary2RegisterScalar_I16_32L_VMLSL_by_scalar_A2
    : public VectorBinary2RegisterScalar_I16_32L {
};

class VectorBinary2RegisterScalar_I16_32L_VMULL_by_scalar_A2
    : public VectorBinary2RegisterScalar_I16_32L {
};

class VectorBinary2RegisterScalar_I16_32L_VQDMLAL_A1
    : public VectorBinary2RegisterScalar_I16_32L {
};

class VectorBinary2RegisterScalar_I16_32L_VQDMLSL_A1
    : public VectorBinary2RegisterScalar_I16_32L {
};

class VectorBinary2RegisterScalar_I16_32L_VQDMULL_A2
    : public VectorBinary2RegisterScalar_I16_32L {
};

class VectorBinary2RegisterShiftAmount_CVT_VCVT_between_floating_point_and_fixed_point
    : public VectorBinary2RegisterShiftAmount_CVT {
};

class VectorBinary2RegisterShiftAmount_E8_16_32L_VSHLL_A1_or_VMOVL
    : public VectorBinary2RegisterShiftAmount_E8_16_32L {
};

class VectorBinary2RegisterShiftAmount_I_VRSHR
    : public VectorBinary2RegisterShiftAmount_I {
};

class VectorBinary2RegisterShiftAmount_I_VRSRA
    : public VectorBinary2RegisterShiftAmount_I {
};

class VectorBinary2RegisterShiftAmount_I_VSHL_immediate
    : public VectorBinary2RegisterShiftAmount_I {
};

class VectorBinary2RegisterShiftAmount_I_VSHR
    : public VectorBinary2RegisterShiftAmount_I {
};

class VectorBinary2RegisterShiftAmount_I_VSLI
    : public VectorBinary2RegisterShiftAmount_I {
};

class VectorBinary2RegisterShiftAmount_I_VSRA
    : public VectorBinary2RegisterShiftAmount_I {
};

class VectorBinary2RegisterShiftAmount_I_VSRI
    : public VectorBinary2RegisterShiftAmount_I {
};

class VectorBinary2RegisterShiftAmount_ILS_VQSHL_VQSHLU_immediate
    : public VectorBinary2RegisterShiftAmount_ILS {
};

class VectorBinary2RegisterShiftAmount_N16_32_64R_VRSHRN
    : public VectorBinary2RegisterShiftAmount_N16_32_64R {
};

class VectorBinary2RegisterShiftAmount_N16_32_64R_VSHRN
    : public VectorBinary2RegisterShiftAmount_N16_32_64R {
};

class VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN
    : public VectorBinary2RegisterShiftAmount_N16_32_64RS {
};

class VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN
    : public VectorBinary2RegisterShiftAmount_N16_32_64RS {
};

class VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRN
    : public VectorBinary2RegisterShiftAmount_N16_32_64RS {
};

class VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRUN
    : public VectorBinary2RegisterShiftAmount_N16_32_64RS {
};

class VectorBinary3RegisterDifferentLength_I16_32L_VQDMLAL_VQDMLSL_A1
    : public VectorBinary3RegisterDifferentLength_I16_32L {
};

class VectorBinary3RegisterDifferentLength_I16_32L_VQDMULL_A1
    : public VectorBinary3RegisterDifferentLength_I16_32L {
};

class VectorBinary3RegisterDifferentLength_I16_32_64_VADDHN
    : public VectorBinary3RegisterDifferentLength_I16_32_64 {
};

class VectorBinary3RegisterDifferentLength_I16_32_64_VRADDHN
    : public VectorBinary3RegisterDifferentLength_I16_32_64 {
};

class VectorBinary3RegisterDifferentLength_I16_32_64_VRSUBHN
    : public VectorBinary3RegisterDifferentLength_I16_32_64 {
};

class VectorBinary3RegisterDifferentLength_I16_32_64_VSUBHN
    : public VectorBinary3RegisterDifferentLength_I16_32_64 {
};

class VectorBinary3RegisterDifferentLength_I8_16_32_VADDL_VADDW
    : public VectorBinary3RegisterDifferentLength_I8_16_32 {
};

class VectorBinary3RegisterDifferentLength_I8_16_32_VSUBL_VSUBW
    : public VectorBinary3RegisterDifferentLength_I8_16_32 {
};

class VectorBinary3RegisterDifferentLength_I8_16_32L_VABAL_A2
    : public VectorBinary3RegisterDifferentLength_I8_16_32L {
};

class VectorBinary3RegisterDifferentLength_I8_16_32L_VABDL_integer_A2
    : public VectorBinary3RegisterDifferentLength_I8_16_32L {
};

class VectorBinary3RegisterDifferentLength_I8_16_32L_VMLAL_VMLSL_integer_A2
    : public VectorBinary3RegisterDifferentLength_I8_16_32L {
};

class VectorBinary3RegisterDifferentLength_I8_16_32L_VMULL_integer_A2
    : public VectorBinary3RegisterDifferentLength_I8_16_32L {
};

class VectorBinary3RegisterDifferentLength_P8_VMULL_polynomial_A2
    : public VectorBinary3RegisterDifferentLength_P8 {
};

class VectorBinary3RegisterImmOp_Vext_Rule_305_A1_P598
    : public VectorBinary3RegisterImmOp {
};

class VectorBinary3RegisterLookupOp_Vtbl_Vtbx_Rule_406_A1_P798
    : public VectorBinary3RegisterLookupOp {
};

class VectorBinary3RegisterSameLength32P_VPADD_floating_point
    : public VectorBinary3RegisterSameLength32P {
};

class VectorBinary3RegisterSameLength32P_VPMAX
    : public VectorBinary3RegisterSameLength32P {
};

class VectorBinary3RegisterSameLength32P_VPMIN
    : public VectorBinary3RegisterSameLength32P {
};

class VectorBinary3RegisterSameLength32_DQ_VABD_floating_point
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VACGE
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VACGT
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VCGE_register_A2
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VCGT_register_A2
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VFMA_A1
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VFMS_A1
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VMAX_floating_point
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VMIN_floating_point
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VRECPS
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VRSQRTS
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1
    : public VectorBinary3RegisterSameLength32_DQ {
};

class VectorBinary3RegisterSameLengthDI_VPADD_integer
    : public VectorBinary3RegisterSameLengthDI {
};

class VectorBinary3RegisterSameLengthDI_VPMAX
    : public VectorBinary3RegisterSameLengthDI {
};

class VectorBinary3RegisterSameLengthDI_VPMIN
    : public VectorBinary3RegisterSameLengthDI {
};

class VectorBinary3RegisterSameLengthDQ_VADD_integer
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VAND_register
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VBIC_register
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VBIF
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VBIT
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VBSL
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VEOR
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VORN_register
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VQADD
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VQRSHL
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VQSHL_register
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VQSUB
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VRSHL
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VSHL_register
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQ_VSUB_integer
    : public VectorBinary3RegisterSameLengthDQ {
};

class VectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1
    : public VectorBinary3RegisterSameLengthDQI16_32 {
};

class VectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1
    : public VectorBinary3RegisterSameLengthDQI16_32 {
};

class VectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1
    : public VectorBinary3RegisterSameLengthDQI8P {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VABA
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VABD
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VHADD
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VHSUB
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VMAX
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VMIN
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VRHADD
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorBinary3RegisterSameLengthDQI8_16_32_VTST
    : public VectorBinary3RegisterSameLengthDQI8_16_32 {
};

class VectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes
    : public VectorLoadSingle1AllLanes {
};

class VectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes
    : public VectorLoadSingle2AllLanes {
};

class VectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes
    : public VectorLoadSingle3AllLanes {
};

class VectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes
    : public VectorLoadSingle4AllLanes {
};

class VectorLoadStoreMultiple1_VLD1_multiple_single_elements
    : public VectorLoadStoreMultiple1 {
};

class VectorLoadStoreMultiple1_VST1_multiple_single_elements
    : public VectorLoadStoreMultiple1 {
};

class VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures
    : public VectorLoadStoreMultiple2 {
};

class VectorLoadStoreMultiple2_VST2_multiple_2_element_structures
    : public VectorLoadStoreMultiple2 {
};

class VectorLoadStoreMultiple3_VLD3_multiple_3_element_structures
    : public VectorLoadStoreMultiple3 {
};

class VectorLoadStoreMultiple3_VST3_multiple_3_element_structures
    : public VectorLoadStoreMultiple3 {
};

class VectorLoadStoreMultiple4_VLD4_multiple_4_element_structures
    : public VectorLoadStoreMultiple4 {
};

class VectorLoadStoreMultiple4_VST4_multiple_4_element_structures
    : public VectorLoadStoreMultiple4 {
};

class VectorLoadStoreSingle1_VLD1_single_element_to_one_lane
    : public VectorLoadStoreSingle1 {
};

class VectorLoadStoreSingle1_VST1_single_element_from_one_lane
    : public VectorLoadStoreSingle1 {
};

class VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane
    : public VectorLoadStoreSingle2 {
};

class VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane
    : public VectorLoadStoreSingle2 {
};

class VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane
    : public VectorLoadStoreSingle3 {
};

class VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane
    : public VectorLoadStoreSingle3 {
};

class VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane
    : public VectorLoadStoreSingle4 {
};

class VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane
    : public VectorLoadStoreSingle4 {
};

class VectorUnary2RegisterDup_Vdup_Rule_302_A1_P592
    : public VectorUnary2RegisterDup {
};

class VfpMrsOp_Vmrs_Rule_335_A1_P658
    : public VfpMrsOp {
};

class VfpUsesRegOp_Vmsr_Rule_336_A1_P660
    : public VfpUsesRegOp {
};

class Branch_B_Rule_16_A1_P44
    : public Branch {
};

class Branch_Bl_Blx_Rule_23_A1_P58
    : public Branch {
};

class Breakpoint_Bkpt_Rule_22_A1_P56
    : public Breakpoint {
};

class BxBlx_Blx_Rule_24_A1_P60
    : public BxBlx {
};

class BxBlx_Bx_Rule_25_A1_P62
    : public BxBlx {
};

class Defs12To15CondsDontCareMsbGeLsb_Bfi_Rule_18_A1_P48
    : public Defs12To15CondsDontCareMsbGeLsb {
};

class Defs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524
    : public Defs12To15CondsDontCareRdRnNotPc {
};

class Defs12To15CondsDontCareRdRnNotPcBitfieldExtract_Sbfx_Rule_154_A1_P308
    : public Defs12To15CondsDontCareRdRnNotPcBitfieldExtract {
};

class Defs12To15CondsDontCareRdRnNotPcBitfieldExtract_Ubfx_Rule_236_A1_P466
    : public Defs12To15CondsDontCareRdRnNotPcBitfieldExtract {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sadd8_Rule_149_A1_P298
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sxtab16_Rule_221_A1_P436
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sxtab_Rule_220_A1_P434
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Sxtah_Rule_222_A1_P438
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uxtab16_Rule_262_A1_P516
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uxtab_Rule_260_A1_P514
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15CondsDontCareRnRdRmNotPc_Uxtah_Rule_262_A1_P518
    : public Defs12To15CondsDontCareRnRdRmNotPc {
};

class Defs12To15RdRnNotPc_Clz_Rule_31_A1_P72
    : public Defs12To15RdRnNotPc {
};

class Defs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336
    : public Defs12To19CondsDontCareRdRmRnNotPc {
};

class Defs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336
    : public Defs12To19CondsDontCareRdRmRnNotPc {
};

class Defs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344
    : public Defs12To19CondsDontCareRdRmRnNotPc {
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
};

class Defs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502
    : public Defs16To19CondsDontCareRdRaRmRnNotPc {
};

class Defs16To19CondsDontCareRdRmRnNotPc_Sdiv_Rule_A1
    : public Defs16To19CondsDontCareRdRmRnNotPc {
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350
    : public Defs16To19CondsDontCareRdRmRnNotPc {
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352
    : public Defs16To19CondsDontCareRdRmRnNotPc {
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358
    : public Defs16To19CondsDontCareRdRmRnNotPc {
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354
    : public Defs16To19CondsDontCareRdRmRnNotPc {
};

class Defs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360
    : public Defs16To19CondsDontCareRdRmRnNotPc {
};

class Defs16To19CondsDontCareRdRmRnNotPc_Udiv_Rule_A1
    : public Defs16To19CondsDontCareRdRmRnNotPc {
};

class Defs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500
    : public Defs16To19CondsDontCareRdRmRnNotPc {
};

class DontCareInst_Msr_Rule_103_A1_P208
    : public DontCareInst {
};

class DontCareInst_Nop_Rule_110_A1_P222
    : public DontCareInst {
};

class DontCareInst_Yield_Rule_413_A1_P812
    : public DontCareInst {
};

class DontCareInstRdNotPc_Vmsr_Rule_336_A1_P660
    : public DontCareInstRdNotPc {
};

class Forbidden_Blx_Rule_23_A2_P58
    : public Forbidden {
};

class Forbidden_Bxj_Rule_26_A1_P64
    : public Forbidden {
};

class Forbidden_Cdp2_Rule_28_A2_P68
    : public Forbidden {
};

class Forbidden_Cdp_Rule_A1
    : public Forbidden {
};

class Forbidden_Clrex_Rule_30_A1_P70
    : public Forbidden {
};

class Forbidden_Cps_Rule_b6_1_1_A1_B6_3
    : public Forbidden {
};

class Forbidden_Dbg_Rule_40_A1_P88
    : public Forbidden {
};

class Forbidden_Eret_Rule_A1
    : public Forbidden {
};

class Forbidden_Hvc_Rule_A1
    : public Forbidden {
};

class Forbidden_Ldc2_Rule_51_A2_P106
    : public Forbidden {
};

class Forbidden_Ldc2_Rule_52_A2_P108
    : public Forbidden {
};

class Forbidden_Ldc_immediate_Rule_A1
    : public Forbidden {
};

class Forbidden_Ldc_literal_Rule_A1
    : public Forbidden {
};

class Forbidden_Ldm_Rule_2_B6_A1_P5
    : public Forbidden {
};

class Forbidden_Ldm_Rule_3_B6_A1_P7
    : public Forbidden {
};

class Forbidden_Ldrt_Rule_A1
    : public Forbidden {
};

class Forbidden_Ldrt_Rule_A2
    : public Forbidden {
};

class Forbidden_Ldrtb_Rule_A1
    : public Forbidden {
};

class Forbidden_Ldrtb_Rule_A2
    : public Forbidden {
};

class Forbidden_Mcr2_Rule_92_A2_P186
    : public Forbidden {
};

class Forbidden_Mcr_Rule_A1
    : public Forbidden {
};

class Forbidden_Mcrr2_Rule_93_A2_P188
    : public Forbidden {
};

class Forbidden_Mcrr_Rule_A1
    : public Forbidden {
};

class Forbidden_Mrc2_Rule_100_A2_P202
    : public Forbidden {
};

class Forbidden_Mrc_Rule_A1
    : public Forbidden {
};

class Forbidden_Mrrc2_Rule_101_A2_P204
    : public Forbidden {
};

class Forbidden_Mrrc_Rule_A1
    : public Forbidden {
};

class Forbidden_Msr_Rule_B6_1_6_A1_PB6_12
    : public Forbidden {
};

class Forbidden_Msr_Rule_B6_1_7_P14
    : public Forbidden {
};

class Forbidden_Msr_Rule_Banked_register_A1_B9_1990
    : public Forbidden {
};

class Forbidden_Msr_Rule_Banked_register_A1_B9_1992
    : public Forbidden {
};

class Forbidden_Rfe_Rule_B6_1_10_A1_B6_16
    : public Forbidden {
};

class Forbidden_Setend_Rule_157_P314
    : public Forbidden {
};

class Forbidden_Sev_Rule_158_A1_P316
    : public Forbidden {
};

class Forbidden_Smc_Rule_B6_1_9_P18
    : public Forbidden {
};

class Forbidden_Srs_Rule_B6_1_10_A1_B6_20
    : public Forbidden {
};

class Forbidden_Stc2_Rule_188_A2_P372
    : public Forbidden {
};

class Forbidden_Stc_Rule_A2
    : public Forbidden {
};

class Forbidden_Stm_Rule_11_B6_A1_P22
    : public Forbidden {
};

class Forbidden_Strt_Rule_A1
    : public Forbidden {
};

class Forbidden_Strt_Rule_A2
    : public Forbidden {
};

class Forbidden_Strtb_Rule_A1
    : public Forbidden {
};

class Forbidden_Strtb_Rule_A2
    : public Forbidden {
};

class Forbidden_Svc_Rule_A1
    : public Forbidden {
};

class Forbidden_Unallocated_hints
    : public Forbidden {
};

class Forbidden_Wfe_Rule_411_A1_P808
    : public Forbidden {
};

class Forbidden_Wfi_Rule_412_A1_P810
    : public Forbidden {
};

class Forbidden_extra_load_store_instructions_unpriviledged
    : public Forbidden {
};

class LoadBasedImmedMemory_Ldr_Rule_59_A1_P122
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemory_Ldrb_Rule_62_A1_P128
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemory_Ldrb_Rule_63_A1_P130
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemory_Ldrh_Rule_74_A1_P152
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemory_Ldrh_Rule_75_A1_P154
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemory_ldrsb_Rule_79_A1_162
    : public LoadBasedImmedMemory {
};

class LoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136
    : public LoadBasedImmedMemoryDouble {
};

class LoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138
    : public LoadBasedImmedMemoryDouble {
};

class LoadBasedMemory_Ldrex_Rule_69_A1_P142
    : public LoadBasedMemory {
};

class LoadBasedMemory_Ldrexb_Rule_70_A1_P144
    : public LoadBasedMemory {
};

class LoadBasedMemory_Ldrexh_Rule_72_A1_P148
    : public LoadBasedMemory {
};

class LoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146
    : public LoadBasedMemoryDouble {
};

class LoadBasedOffsetMemory_Ldr_Rule_60_A1_P124
    : public LoadBasedOffsetMemory {
};

class LoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132
    : public LoadBasedOffsetMemory {
};

class LoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156
    : public LoadBasedOffsetMemory {
};

class LoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164
    : public LoadBasedOffsetMemory {
};

class LoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172
    : public LoadBasedOffsetMemory {
};

class LoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140
    : public LoadBasedOffsetMemoryDouble {
};

class LoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110
    : public LoadMultiple {
};

class LoadMultiple_Ldmda_Ldmfa_Rule_54_A1_P112
    : public LoadMultiple {
};

class LoadMultiple_Ldmdb_Ldmea_Rule_55_A1_P114
    : public LoadMultiple {
};

class LoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116
    : public LoadMultiple {
};

class LoadMultiple_Pop_Rule_A1
    : public LoadMultiple {
};

class StoreBasedImmedMemory_Str_Rule_194_A1_P384
    : public StoreBasedImmedMemory {
};

class StoreBasedImmedMemory_Strb_Rule_197_A1_P390
    : public StoreBasedImmedMemory {
};

class StoreBasedImmedMemory_Strh_Rule_207_A1_P410
    : public StoreBasedImmedMemory {
};

class StoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396
    : public StoreBasedImmedMemoryDouble {
};

class StoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404
    : public StoreBasedMemoryDoubleRtBits0To3 {
};

class StoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400
    : public StoreBasedMemoryRtBits0To3 {
};

class StoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402
    : public StoreBasedMemoryRtBits0To3 {
};

class StoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406
    : public StoreBasedMemoryRtBits0To3 {
};

class StoreBasedOffsetMemory_Str_Rule_195_A1_P386
    : public StoreBasedOffsetMemory {
};

class StoreBasedOffsetMemory_Strb_Rule_198_A1_P392
    : public StoreBasedOffsetMemory {
};

class StoreBasedOffsetMemory_Strh_Rule_208_A1_P412
    : public StoreBasedOffsetMemory {
};

class StoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398
    : public StoreBasedOffsetMemoryDouble {
};

class Undefined_Undefined_A5_6
    : public Undefined {
};

class Unpredictable_Unpredictable
    : public Unpredictable {
};

class VfpOp_Vabs_Rule_269_A2_P532
    : public VfpOp {
};

class VfpOp_Vadd_Rule_271_A2_P536
    : public VfpOp {
};

class VfpOp_Vcmp_Vcmpe_Rule_A1
    : public VfpOp {
};

class VfpOp_Vcmp_Vcmpe_Rule_A2
    : public VfpOp {
};

class VfpOp_Vcvt_Rule_297_A1_P582
    : public VfpOp {
};

class VfpOp_Vcvt_Rule_298_A1_P584
    : public VfpOp {
};

class VfpOp_Vcvt_Vcvtr_Rule_295_A1_P578
    : public VfpOp {
};

class VfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588
    : public VfpOp {
};

class VfpOp_Vdiv_Rule_301_A1_P590
    : public VfpOp {
};

class VfpOp_Vfma_vfms_Rule_A1
    : public VfpOp {
};

class VfpOp_Vfnma_vfnms_Rule_A1
    : public VfpOp {
};

class VfpOp_Vmla_vmls_Rule_423_A2_P636
    : public VfpOp {
};

class VfpOp_Vmov_Rule_326_A2_P640
    : public VfpOp {
};

class VfpOp_Vmov_Rule_327_A2_P642
    : public VfpOp {
};

class VfpOp_Vmul_Rule_338_A2_P664
    : public VfpOp {
};

class VfpOp_Vneg_Rule_342_A2_P672
    : public VfpOp {
};

class VfpOp_Vnmla_vnmls_Rule_343_A1_P674
    : public VfpOp {
};

class VfpOp_Vnmul_Rule_343_A2_P674
    : public VfpOp {
};

class VfpOp_Vsqrt_Rule_388_A1_P762
    : public VfpOp {
};

class VfpOp_Vsub_Rule_402_A2_P790
    : public VfpOp {
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

class NamedBinary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRangeMsbGeLsb Bfi_Rule_18_A1_P48")
  {}

 private:
  nacl_arm_dec::Binary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48);
};

class NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRangeNotRnIsPcBitfieldExtract Sbfx_Rule_154_A1_P308")
  {}

 private:
  nacl_arm_dec::Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308);
};

class NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRangeNotRnIsPcBitfieldExtract Ubfx_Rule_236_A1_P466")
  {}

 private:
  nacl_arm_dec::Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466);
};

class NamedBinary2RegisterImmedShiftedTest_CMN_register
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmedShiftedTest_CMN_register()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmedShiftedTest CMN_register")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest_CMN_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmedShiftedTest_CMN_register);
};

class NamedBinary2RegisterImmedShiftedTest_CMP_register
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmedShiftedTest_CMP_register()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmedShiftedTest CMP_register")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest_CMP_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmedShiftedTest_CMP_register);
};

class NamedBinary2RegisterImmedShiftedTest_TEQ_register
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmedShiftedTest_TEQ_register()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmedShiftedTest TEQ_register")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest_TEQ_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmedShiftedTest_TEQ_register);
};

class NamedBinary2RegisterImmedShiftedTest_TST_register
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmedShiftedTest_TST_register()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmedShiftedTest TST_register")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest_TST_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmedShiftedTest_TST_register);
};

class NamedBinary2RegisterImmediateOp_ADC_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOp_ADC_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp ADC_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_ADC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_ADC_immediate);
};

class NamedBinary2RegisterImmediateOp_AND_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOp_AND_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp AND_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_AND_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_AND_immediate);
};

class NamedBinary2RegisterImmediateOp_EOR_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOp_EOR_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp EOR_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_EOR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_EOR_immediate);
};

class NamedBinary2RegisterImmediateOp_RSB_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOp_RSB_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp RSB_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_RSB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_RSB_immediate);
};

class NamedBinary2RegisterImmediateOp_RSC_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOp_RSC_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp RSC_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_RSC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_RSC_immediate);
};

class NamedBinary2RegisterImmediateOp_SBC_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOp_SBC_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOp SBC_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOp_SBC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOp_SBC_immediate);
};

class NamedBinary2RegisterImmediateOpAddSub_ADD_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOpAddSub_ADD_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOpAddSub ADD_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOpAddSub_ADD_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOpAddSub_ADD_immediate);
};

class NamedBinary2RegisterImmediateOpAddSub_SUB_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOpAddSub_SUB_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOpAddSub SUB_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOpAddSub_SUB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOpAddSub_SUB_immediate);
};

class NamedBinary2RegisterImmediateOpDynCodeReplace_ORR_immediate
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterImmediateOpDynCodeReplace_ORR_immediate()
    : NamedClassDecoder(decoder_, "Binary2RegisterImmediateOpDynCodeReplace ORR_immediate")
  {}

 private:
  nacl_arm_dec::Binary2RegisterImmediateOpDynCodeReplace_ORR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterImmediateOpDynCodeReplace_ORR_immediate);
};

class NamedBinary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOpRegsNotPc Pkh_Rule_116_A1_P234")
  {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234);
};

class NamedBinary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436()
    : NamedClassDecoder(decoder_, "Binary3RegisterImmedShiftedOpRegsNotPc Sxtab16_Rule_221_A1_P436")
  {}

 private:
  nacl_arm_dec::Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436);
};

class NamedBinary3RegisterOp_ASR_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOp_ASR_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterOp ASR_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOp_ASR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOp_ASR_register);
};

class NamedBinary3RegisterOp_LSL_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOp_LSL_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterOp LSL_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOp_LSL_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOp_LSL_register);
};

class NamedBinary3RegisterOp_LSR_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOp_LSR_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterOp LSR_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOp_LSR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOp_LSR_register);
};

class NamedBinary3RegisterOp_ROR_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOp_ROR_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterOp ROR_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOp_ROR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOp_ROR_register);
};

class NamedBinary3RegisterOpAltA_MUL_A1
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_MUL_A1()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA MUL_A1")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_MUL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_MUL_A1);
};

class NamedBinary3RegisterOpAltA_Sdiv_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_Sdiv_Rule_A1()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Sdiv_Rule_A1")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Sdiv_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Sdiv_Rule_A1);
};

class NamedBinary3RegisterOpAltA_Smmul_Rule_176_P350
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_Smmul_Rule_176_P350()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smmul_Rule_176_P350")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smmul_Rule_176_P350 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smmul_Rule_176_P350);
};

class NamedBinary3RegisterOpAltA_Smuad_Rule_177_P352
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_Smuad_Rule_177_P352()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smuad_Rule_177_P352")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smuad_Rule_177_P352 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smuad_Rule_177_P352);
};

class NamedBinary3RegisterOpAltA_Smulwx_Rule_180_A1_P358
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_Smulwx_Rule_180_A1_P358()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smulwx_Rule_180_A1_P358")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smulwx_Rule_180_A1_P358);
};

class NamedBinary3RegisterOpAltA_Smulxx_Rule_178_P354
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_Smulxx_Rule_178_P354()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smulxx_Rule_178_P354")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smulxx_Rule_178_P354 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smulxx_Rule_178_P354);
};

class NamedBinary3RegisterOpAltA_Smusd_Rule_181_P360
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_Smusd_Rule_181_P360()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Smusd_Rule_181_P360")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Smusd_Rule_181_P360 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Smusd_Rule_181_P360);
};

class NamedBinary3RegisterOpAltA_Udiv_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_Udiv_Rule_A1()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Udiv_Rule_A1")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Udiv_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Udiv_Rule_A1);
};

class NamedBinary3RegisterOpAltA_Usad8_Rule_253_A1_P500
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_Usad8_Rule_253_A1_P500()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA Usad8_Rule_253_A1_P500")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_Usad8_Rule_253_A1_P500);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_QADD
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QADD()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QADD")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QADD);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_QDADD
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QDADD()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QDADD")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QDADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QDADD);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_QDSUB
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QDSUB()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QDSUB")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QDSUB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QDSUB);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_QSUB
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QSUB()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QSUB")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QSUB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QSUB);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Qadd16_Rule_125_A1_P252")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Qadd8_Rule_126_A1_P254")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Qasx_Rule_127_A1_P256")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Qsax_Rule_130_A1_P262")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Qsub16_Rule_132_A1_P266")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Qsub8_Rule_133_A1_P268")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Sadd16_Rule_148_A1_P296")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Sadd8_Rule_149_A1_P298")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Sasx_Rule_150_A1_P300")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Sel_Rule_156_A1_P312")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shadd16_Rule_159_A1_P318")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shadd8_Rule_160_A1_P320")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shasx_Rule_161_A1_P322")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shsax_Rule_162_A1_P324")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shsub16_Rule_163_A1_P326")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Shsub8_Rule_164_A1_P328")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Ssax_Rule_185_A1_P366")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Ssub16_Rule_186_A1_P368")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Ssub8_Rule_187_A1_P370")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Sxtab_Rule_220_A1_P434")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Sxtah_Rule_222_A1_P438")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uadd16_Rule_233_A1_P460")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uadd8_Rule_234_A1_P462")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uasx_Rule_235_A1_P464")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhadd16_Rule_238_A1_P470")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhadd8_Rule_239_A1_P472")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhasx_Rule_240_A1_P474")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhsax_Rule_241_A1_P476")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhsub16_Rule_242_A1_P478")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uhsub8_Rule_243_A1_P480")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uqadd16_Rule_247_A1_P488")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uqadd8_Rule_248_A1_P490")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uqasx_Rule_249_A1_P492")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uqsax_Rule_250_A1_P494")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uqsub16_Rule_251_A1_P496")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uqsub8_Rule_252_A1_P498")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Usax_Rule_257_A1_P508")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Usub16_Rule_258_A1_P510")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Usub8_Rule_259_A1_P512")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uxtab16_Rule_262_A1_P516")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uxtab_Rule_260_A1_P514")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates Uxtah_Rule_262_A1_P518")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518);
};

class NamedBinary3RegisterShiftedOp_ADC_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_ADC_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp ADC_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_ADC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_ADC_register);
};

class NamedBinary3RegisterShiftedOp_ADD_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_ADD_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp ADD_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_ADD_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_ADD_register);
};

class NamedBinary3RegisterShiftedOp_AND_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_AND_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp AND_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_AND_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_AND_register);
};

class NamedBinary3RegisterShiftedOp_BIC_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_BIC_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp BIC_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_BIC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_BIC_register);
};

class NamedBinary3RegisterShiftedOp_EOR_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_EOR_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp EOR_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_EOR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_EOR_register);
};

class NamedBinary3RegisterShiftedOp_ORR_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_ORR_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp ORR_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_ORR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_ORR_register);
};

class NamedBinary3RegisterShiftedOp_RSB_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_RSB_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp RSB_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_RSB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_RSB_register);
};

class NamedBinary3RegisterShiftedOp_RSC_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_RSC_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp RSC_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_RSC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_RSC_register);
};

class NamedBinary3RegisterShiftedOp_SBC_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_SBC_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp SBC_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_SBC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_SBC_register);
};

class NamedBinary3RegisterShiftedOp_SUB_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedOp_SUB_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedOp SUB_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedOp_SUB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedOp_SUB_register);
};

class NamedBinary3RegisterShiftedTest_CMN_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedTest_CMN_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest CMN_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedTest_CMN_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedTest_CMN_register_shifted_register);
};

class NamedBinary3RegisterShiftedTest_CMP_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedTest_CMP_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest CMP_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedTest_CMP_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedTest_CMP_register_shifted_register);
};

class NamedBinary3RegisterShiftedTest_TEQ_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedTest_TEQ_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest TEQ_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedTest_TEQ_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedTest_TEQ_register_shifted_register);
};

class NamedBinary3RegisterShiftedTest_TST_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterShiftedTest_TST_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest TST_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary3RegisterShiftedTest_TST_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterShiftedTest_TST_register_shifted_register);
};

class NamedBinary4RegisterDualOp_MLS_A1
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_MLS_A1()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp MLS_A1")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_MLS_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_MLS_A1);
};

class NamedBinary4RegisterDualOp_Smlad_Rule_167_P332
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_Smlad_Rule_167_P332()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smlad_Rule_167_P332")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smlad_Rule_167_P332 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smlad_Rule_167_P332);
};

class NamedBinary4RegisterDualOp_Smlawx_Rule_171_A1_340
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_Smlawx_Rule_171_A1_340()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smlawx_Rule_171_A1_340")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smlawx_Rule_171_A1_340 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smlawx_Rule_171_A1_340);
};

class NamedBinary4RegisterDualOp_Smlaxx_Rule_166_A1_P330
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_Smlaxx_Rule_166_A1_P330()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smlaxx_Rule_166_A1_P330")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smlaxx_Rule_166_A1_P330);
};

class NamedBinary4RegisterDualOp_Smlsd_Rule_172_P342
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_Smlsd_Rule_172_P342()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smlsd_Rule_172_P342")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smlsd_Rule_172_P342 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smlsd_Rule_172_P342);
};

class NamedBinary4RegisterDualOp_Smmla_Rule_174_P346
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_Smmla_Rule_174_P346()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smmla_Rule_174_P346")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smmla_Rule_174_P346 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smmla_Rule_174_P346);
};

class NamedBinary4RegisterDualOp_Smmls_Rule_175_P348
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_Smmls_Rule_175_P348()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Smmls_Rule_175_P348")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Smmls_Rule_175_P348 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Smmls_Rule_175_P348);
};

class NamedBinary4RegisterDualOp_Usada8_Rule_254_A1_P502
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_Usada8_Rule_254_A1_P502()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp Usada8_Rule_254_A1_P502")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_Usada8_Rule_254_A1_P502 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_Usada8_Rule_254_A1_P502);
};

class NamedBinary4RegisterDualOpLtV6RdNotRn_MLA_A1
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOpLtV6RdNotRn_MLA_A1()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOpLtV6RdNotRn MLA_A1")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOpLtV6RdNotRn_MLA_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOpLtV6RdNotRn_MLA_A1);
};

class NamedBinary4RegisterDualResult_UMAAL_A1
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResult_UMAAL_A1()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult UMAAL_A1")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_UMAAL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_UMAAL_A1);
};

class NamedBinary4RegisterDualResult_Smlald_Rule_170_P336
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResult_Smlald_Rule_170_P336()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Smlald_Rule_170_P336")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Smlald_Rule_170_P336 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Smlald_Rule_170_P336);
};

class NamedBinary4RegisterDualResult_Smlalxx_Rule_169_A1_P336
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResult_Smlalxx_Rule_169_A1_P336()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Smlalxx_Rule_169_A1_P336")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Smlalxx_Rule_169_A1_P336);
};

class NamedBinary4RegisterDualResult_Smlsld_Rule_173_P344
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResult_Smlsld_Rule_173_P344()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult Smlsld_Rule_173_P344")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_Smlsld_Rule_173_P344 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_Smlsld_Rule_173_P344);
};

class NamedBinary4RegisterDualResultLtV6RdHiLoNotRn_SMLAL_A1
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResultLtV6RdHiLoNotRn_SMLAL_A1()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResultLtV6RdHiLoNotRn SMLAL_A1")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResultLtV6RdHiLoNotRn_SMLAL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResultLtV6RdHiLoNotRn_SMLAL_A1);
};

class NamedBinary4RegisterDualResultLtV6RdHiLoNotRn_UMLAL_A1
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResultLtV6RdHiLoNotRn_UMLAL_A1()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResultLtV6RdHiLoNotRn UMLAL_A1")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResultLtV6RdHiLoNotRn_UMLAL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResultLtV6RdHiLoNotRn_UMLAL_A1);
};

class NamedBinary4RegisterDualResultUsesRnRm_SMULL_A1
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResultUsesRnRm_SMULL_A1()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResultUsesRnRm SMULL_A1")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResultUsesRnRm_SMULL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResultUsesRnRm_SMULL_A1);
};

class NamedBinary4RegisterDualResultUsesRnRm_UMULL_A1
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResultUsesRnRm_UMULL_A1()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResultUsesRnRm UMULL_A1")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResultUsesRnRm_UMULL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResultUsesRnRm_UMULL_A1);
};

class NamedBinary4RegisterShiftedOp_ADC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_ADC_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp ADC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_ADC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_ADC_register_shifted_register);
};

class NamedBinary4RegisterShiftedOp_ADD_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_ADD_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp ADD_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_ADD_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_ADD_register_shifted_register);
};

class NamedBinary4RegisterShiftedOp_AND_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_AND_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp AND_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_AND_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_AND_register_shifted_register);
};

class NamedBinary4RegisterShiftedOp_BIC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_BIC_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp BIC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_BIC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_BIC_register_shifted_register);
};

class NamedBinary4RegisterShiftedOp_EOR_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_EOR_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp EOR_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_EOR_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_EOR_register_shifted_register);
};

class NamedBinary4RegisterShiftedOp_ORR_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_ORR_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp ORR_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_ORR_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_ORR_register_shifted_register);
};

class NamedBinary4RegisterShiftedOp_RSB_register_shfited_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_RSB_register_shfited_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp RSB_register_shfited_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_RSB_register_shfited_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_RSB_register_shfited_register);
};

class NamedBinary4RegisterShiftedOp_RSC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_RSC_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp RSC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_RSC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_RSC_register_shifted_register);
};

class NamedBinary4RegisterShiftedOp_SBC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_SBC_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp SBC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_SBC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_SBC_register_shifted_register);
};

class NamedBinary4RegisterShiftedOp_SUB_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterShiftedOp_SUB_register_shifted_register()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp SUB_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Binary4RegisterShiftedOp_SUB_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterShiftedOp_SUB_register_shifted_register);
};

class NamedBinaryRegisterImmediateTest_CMN_immediate
    : public NamedClassDecoder {
 public:
  NamedBinaryRegisterImmediateTest_CMN_immediate()
    : NamedClassDecoder(decoder_, "BinaryRegisterImmediateTest CMN_immediate")
  {}

 private:
  nacl_arm_dec::BinaryRegisterImmediateTest_CMN_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinaryRegisterImmediateTest_CMN_immediate);
};

class NamedBinaryRegisterImmediateTest_CMP_immediate
    : public NamedClassDecoder {
 public:
  NamedBinaryRegisterImmediateTest_CMP_immediate()
    : NamedClassDecoder(decoder_, "BinaryRegisterImmediateTest CMP_immediate")
  {}

 private:
  nacl_arm_dec::BinaryRegisterImmediateTest_CMP_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinaryRegisterImmediateTest_CMP_immediate);
};

class NamedBinaryRegisterImmediateTest_TEQ_immediate
    : public NamedClassDecoder {
 public:
  NamedBinaryRegisterImmediateTest_TEQ_immediate()
    : NamedClassDecoder(decoder_, "BinaryRegisterImmediateTest TEQ_immediate")
  {}

 private:
  nacl_arm_dec::BinaryRegisterImmediateTest_TEQ_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinaryRegisterImmediateTest_TEQ_immediate);
};

class NamedBranchImmediate24_B_Rule_16_A1_P44
    : public NamedClassDecoder {
 public:
  NamedBranchImmediate24_B_Rule_16_A1_P44()
    : NamedClassDecoder(decoder_, "BranchImmediate24 B_Rule_16_A1_P44")
  {}

 private:
  nacl_arm_dec::BranchImmediate24_B_Rule_16_A1_P44 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchImmediate24_B_Rule_16_A1_P44);
};

class NamedBranchImmediate24_Bl_Blx_Rule_23_A1_P58
    : public NamedClassDecoder {
 public:
  NamedBranchImmediate24_Bl_Blx_Rule_23_A1_P58()
    : NamedClassDecoder(decoder_, "BranchImmediate24 Bl_Blx_Rule_23_A1_P58")
  {}

 private:
  nacl_arm_dec::BranchImmediate24_Bl_Blx_Rule_23_A1_P58 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchImmediate24_Bl_Blx_Rule_23_A1_P58);
};

class NamedBranchToRegister_Blx_Rule_24_A1_P60
    : public NamedClassDecoder {
 public:
  NamedBranchToRegister_Blx_Rule_24_A1_P60()
    : NamedClassDecoder(decoder_, "BranchToRegister Blx_Rule_24_A1_P60")
  {}

 private:
  nacl_arm_dec::BranchToRegister_Blx_Rule_24_A1_P60 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchToRegister_Blx_Rule_24_A1_P60);
};

class NamedBranchToRegister_Bx_Rule_25_A1_P62
    : public NamedClassDecoder {
 public:
  NamedBranchToRegister_Bx_Rule_25_A1_P62()
    : NamedClassDecoder(decoder_, "BranchToRegister Bx_Rule_25_A1_P62")
  {}

 private:
  nacl_arm_dec::BranchToRegister_Bx_Rule_25_A1_P62 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchToRegister_Bx_Rule_25_A1_P62);
};

class NamedBreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56
    : public NamedClassDecoder {
 public:
  NamedBreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56()
    : NamedClassDecoder(decoder_, "BreakPointAndConstantPoolHead Bkpt_Rule_22_A1_P56")
  {}

 private:
  nacl_arm_dec::BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56);
};

class NamedCondDecoder_Nop_Rule_110_A1_P222
    : public NamedClassDecoder {
 public:
  NamedCondDecoder_Nop_Rule_110_A1_P222()
    : NamedClassDecoder(decoder_, "CondDecoder Nop_Rule_110_A1_P222")
  {}

 private:
  nacl_arm_dec::CondDecoder_Nop_Rule_110_A1_P222 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondDecoder_Nop_Rule_110_A1_P222);
};

class NamedCondDecoder_Yield_Rule_413_A1_P812
    : public NamedClassDecoder {
 public:
  NamedCondDecoder_Yield_Rule_413_A1_P812()
    : NamedClassDecoder(decoder_, "CondDecoder Yield_Rule_413_A1_P812")
  {}

 private:
  nacl_arm_dec::CondDecoder_Yield_Rule_413_A1_P812 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondDecoder_Yield_Rule_413_A1_P812);
};

class NamedCondVfpOp_Vabs_Rule_269_A2_P532
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vabs_Rule_269_A2_P532()
    : NamedClassDecoder(decoder_, "CondVfpOp Vabs_Rule_269_A2_P532")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vabs_Rule_269_A2_P532 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vabs_Rule_269_A2_P532);
};

class NamedCondVfpOp_Vadd_Rule_271_A2_P536
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vadd_Rule_271_A2_P536()
    : NamedClassDecoder(decoder_, "CondVfpOp Vadd_Rule_271_A2_P536")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vadd_Rule_271_A2_P536 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vadd_Rule_271_A2_P536);
};

class NamedCondVfpOp_Vcmp_Vcmpe_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vcmp_Vcmpe_Rule_A1()
    : NamedClassDecoder(decoder_, "CondVfpOp Vcmp_Vcmpe_Rule_A1")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vcmp_Vcmpe_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vcmp_Vcmpe_Rule_A1);
};

class NamedCondVfpOp_Vcmp_Vcmpe_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vcmp_Vcmpe_Rule_A2()
    : NamedClassDecoder(decoder_, "CondVfpOp Vcmp_Vcmpe_Rule_A2")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vcmp_Vcmpe_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vcmp_Vcmpe_Rule_A2);
};

class NamedCondVfpOp_Vcvt_Rule_297_A1_P582
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vcvt_Rule_297_A1_P582()
    : NamedClassDecoder(decoder_, "CondVfpOp Vcvt_Rule_297_A1_P582")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vcvt_Rule_297_A1_P582 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vcvt_Rule_297_A1_P582);
};

class NamedCondVfpOp_Vcvt_Rule_298_A1_P584
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vcvt_Rule_298_A1_P584()
    : NamedClassDecoder(decoder_, "CondVfpOp Vcvt_Rule_298_A1_P584")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vcvt_Rule_298_A1_P584 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vcvt_Rule_298_A1_P584);
};

class NamedCondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578()
    : NamedClassDecoder(decoder_, "CondVfpOp Vcvt_Vcvtr_Rule_295_A1_P578")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578);
};

class NamedCondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588()
    : NamedClassDecoder(decoder_, "CondVfpOp Vcvtb_Vcvtt_Rule_300_A1_P588")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588);
};

class NamedCondVfpOp_Vdiv_Rule_301_A1_P590
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vdiv_Rule_301_A1_P590()
    : NamedClassDecoder(decoder_, "CondVfpOp Vdiv_Rule_301_A1_P590")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vdiv_Rule_301_A1_P590 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vdiv_Rule_301_A1_P590);
};

class NamedCondVfpOp_Vfma_vfms_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vfma_vfms_Rule_A1()
    : NamedClassDecoder(decoder_, "CondVfpOp Vfma_vfms_Rule_A1")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vfma_vfms_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vfma_vfms_Rule_A1);
};

class NamedCondVfpOp_Vfnma_vfnms_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vfnma_vfnms_Rule_A1()
    : NamedClassDecoder(decoder_, "CondVfpOp Vfnma_vfnms_Rule_A1")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vfnma_vfnms_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vfnma_vfnms_Rule_A1);
};

class NamedCondVfpOp_Vmla_vmls_Rule_423_A2_P636
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vmla_vmls_Rule_423_A2_P636()
    : NamedClassDecoder(decoder_, "CondVfpOp Vmla_vmls_Rule_423_A2_P636")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vmla_vmls_Rule_423_A2_P636 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vmla_vmls_Rule_423_A2_P636);
};

class NamedCondVfpOp_Vmov_Rule_326_A2_P640
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vmov_Rule_326_A2_P640()
    : NamedClassDecoder(decoder_, "CondVfpOp Vmov_Rule_326_A2_P640")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vmov_Rule_326_A2_P640 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vmov_Rule_326_A2_P640);
};

class NamedCondVfpOp_Vmov_Rule_327_A2_P642
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vmov_Rule_327_A2_P642()
    : NamedClassDecoder(decoder_, "CondVfpOp Vmov_Rule_327_A2_P642")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vmov_Rule_327_A2_P642 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vmov_Rule_327_A2_P642);
};

class NamedCondVfpOp_Vmul_Rule_338_A2_P664
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vmul_Rule_338_A2_P664()
    : NamedClassDecoder(decoder_, "CondVfpOp Vmul_Rule_338_A2_P664")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vmul_Rule_338_A2_P664 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vmul_Rule_338_A2_P664);
};

class NamedCondVfpOp_Vneg_Rule_342_A2_P672
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vneg_Rule_342_A2_P672()
    : NamedClassDecoder(decoder_, "CondVfpOp Vneg_Rule_342_A2_P672")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vneg_Rule_342_A2_P672 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vneg_Rule_342_A2_P672);
};

class NamedCondVfpOp_Vnmla_vnmls_Rule_343_A1_P674
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vnmla_vnmls_Rule_343_A1_P674()
    : NamedClassDecoder(decoder_, "CondVfpOp Vnmla_vnmls_Rule_343_A1_P674")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vnmla_vnmls_Rule_343_A1_P674 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vnmla_vnmls_Rule_343_A1_P674);
};

class NamedCondVfpOp_Vnmul_Rule_343_A2_P674
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vnmul_Rule_343_A2_P674()
    : NamedClassDecoder(decoder_, "CondVfpOp Vnmul_Rule_343_A2_P674")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vnmul_Rule_343_A2_P674 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vnmul_Rule_343_A2_P674);
};

class NamedCondVfpOp_Vsqrt_Rule_388_A1_P762
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vsqrt_Rule_388_A1_P762()
    : NamedClassDecoder(decoder_, "CondVfpOp Vsqrt_Rule_388_A1_P762")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vsqrt_Rule_388_A1_P762 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vsqrt_Rule_388_A1_P762);
};

class NamedCondVfpOp_Vsub_Rule_402_A2_P790
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_Vsub_Rule_402_A2_P790()
    : NamedClassDecoder(decoder_, "CondVfpOp Vsub_Rule_402_A2_P790")
  {}

 private:
  nacl_arm_dec::CondVfpOp_Vsub_Rule_402_A2_P790 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_Vsub_Rule_402_A2_P790);
};

class NamedDataBarrier_Dmb_Rule_41_A1_P90
    : public NamedClassDecoder {
 public:
  NamedDataBarrier_Dmb_Rule_41_A1_P90()
    : NamedClassDecoder(decoder_, "DataBarrier Dmb_Rule_41_A1_P90")
  {}

 private:
  nacl_arm_dec::DataBarrier_Dmb_Rule_41_A1_P90 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDataBarrier_Dmb_Rule_41_A1_P90);
};

class NamedDataBarrier_Dsb_Rule_42_A1_P92
    : public NamedClassDecoder {
 public:
  NamedDataBarrier_Dsb_Rule_42_A1_P92()
    : NamedClassDecoder(decoder_, "DataBarrier Dsb_Rule_42_A1_P92")
  {}

 private:
  nacl_arm_dec::DataBarrier_Dsb_Rule_42_A1_P92 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDataBarrier_Dsb_Rule_42_A1_P92);
};

class NamedDeprecated_Swp_Swpb_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedDeprecated_Swp_Swpb_Rule_A1()
    : NamedClassDecoder(decoder_, "Deprecated Swp_Swpb_Rule_A1")
  {}

 private:
  nacl_arm_dec::Deprecated_Swp_Swpb_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDeprecated_Swp_Swpb_Rule_A1);
};

class NamedDuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594
    : public NamedClassDecoder {
 public:
  NamedDuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594()
    : NamedClassDecoder(decoder_, "DuplicateToAdvSIMDRegisters Vdup_Rule_303_A1_P594")
  {}

 private:
  nacl_arm_dec::DuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594);
};

class NamedForbidden_None
    : public NamedClassDecoder {
 public:
  NamedForbidden_None()
    : NamedClassDecoder(decoder_, "Forbidden None")
  {}

 private:
  nacl_arm_dec::Forbidden_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_None);
};

class NamedForbiddenCondDecoder_Bxj_Rule_26_A1_P64
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Bxj_Rule_26_A1_P64()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Bxj_Rule_26_A1_P64")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Bxj_Rule_26_A1_P64 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Bxj_Rule_26_A1_P64);
};

class NamedForbiddenCondDecoder_Cdp_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Cdp_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Cdp_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Cdp_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Cdp_Rule_A1);
};

class NamedForbiddenCondDecoder_Dbg_Rule_40_A1_P88
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Dbg_Rule_40_A1_P88()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Dbg_Rule_40_A1_P88")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Dbg_Rule_40_A1_P88 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Dbg_Rule_40_A1_P88);
};

class NamedForbiddenCondDecoder_Eret_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Eret_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Eret_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Eret_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Eret_Rule_A1);
};

class NamedForbiddenCondDecoder_Hvc_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Hvc_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Hvc_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Hvc_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Hvc_Rule_A1);
};

class NamedForbiddenCondDecoder_Ldc_immediate_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Ldc_immediate_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Ldc_immediate_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Ldc_immediate_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Ldc_immediate_Rule_A1);
};

class NamedForbiddenCondDecoder_Ldc_literal_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Ldc_literal_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Ldc_literal_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Ldc_literal_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Ldc_literal_Rule_A1);
};

class NamedForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Ldm_Rule_2_B6_A1_P5")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5);
};

class NamedForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Ldm_Rule_3_B6_A1_P7")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7);
};

class NamedForbiddenCondDecoder_Ldrt_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Ldrt_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Ldrt_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Ldrt_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Ldrt_Rule_A1);
};

class NamedForbiddenCondDecoder_Ldrt_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Ldrt_Rule_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Ldrt_Rule_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Ldrt_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Ldrt_Rule_A2);
};

class NamedForbiddenCondDecoder_Ldrtb_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Ldrtb_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Ldrtb_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Ldrtb_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Ldrtb_Rule_A1);
};

class NamedForbiddenCondDecoder_Ldrtb_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Ldrtb_Rule_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Ldrtb_Rule_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Ldrtb_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Ldrtb_Rule_A2);
};

class NamedForbiddenCondDecoder_Mcr_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Mcr_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Mcr_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Mcr_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Mcr_Rule_A1);
};

class NamedForbiddenCondDecoder_Mcrr_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Mcrr_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Mcrr_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Mcrr_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Mcrr_Rule_A1);
};

class NamedForbiddenCondDecoder_Mrc_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Mrc_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Mrc_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Mrc_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Mrc_Rule_A1);
};

class NamedForbiddenCondDecoder_Mrrc_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Mrrc_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Mrrc_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Mrrc_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Mrrc_Rule_A1);
};

class NamedForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Msr_Rule_B6_1_6_A1_PB6_12")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12);
};

class NamedForbiddenCondDecoder_Msr_Rule_B6_1_7_P14
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Msr_Rule_B6_1_7_P14()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Msr_Rule_B6_1_7_P14")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Msr_Rule_B6_1_7_P14);
};

class NamedForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Msr_Rule_Banked_register_A1_B9_1990")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990);
};

class NamedForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Msr_Rule_Banked_register_A1_B9_1992")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992);
};

class NamedForbiddenCondDecoder_Sev_Rule_158_A1_P316
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Sev_Rule_158_A1_P316()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Sev_Rule_158_A1_P316")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Sev_Rule_158_A1_P316 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Sev_Rule_158_A1_P316);
};

class NamedForbiddenCondDecoder_Smc_Rule_B6_1_9_P18
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Smc_Rule_B6_1_9_P18()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Smc_Rule_B6_1_9_P18")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Smc_Rule_B6_1_9_P18 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Smc_Rule_B6_1_9_P18);
};

class NamedForbiddenCondDecoder_Stc_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Stc_Rule_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Stc_Rule_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Stc_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Stc_Rule_A2);
};

class NamedForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Stm_Rule_11_B6_A1_P22")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22);
};

class NamedForbiddenCondDecoder_Strt_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Strt_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Strt_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Strt_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Strt_Rule_A1);
};

class NamedForbiddenCondDecoder_Strt_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Strt_Rule_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Strt_Rule_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Strt_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Strt_Rule_A2);
};

class NamedForbiddenCondDecoder_Strtb_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Strtb_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Strtb_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Strtb_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Strtb_Rule_A1);
};

class NamedForbiddenCondDecoder_Strtb_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Strtb_Rule_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Strtb_Rule_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Strtb_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Strtb_Rule_A2);
};

class NamedForbiddenCondDecoder_Svc_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Svc_Rule_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Svc_Rule_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Svc_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Svc_Rule_A1);
};

class NamedForbiddenCondDecoder_Wfe_Rule_411_A1_P808
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Wfe_Rule_411_A1_P808()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Wfe_Rule_411_A1_P808")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Wfe_Rule_411_A1_P808 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Wfe_Rule_411_A1_P808);
};

class NamedForbiddenCondDecoder_Wfi_Rule_412_A1_P810
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_Wfi_Rule_412_A1_P810()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder Wfi_Rule_412_A1_P810")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_Wfi_Rule_412_A1_P810 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_Wfi_Rule_412_A1_P810);
};

class NamedForbiddenCondDecoder_extra_load_store_instructions_unpriviledged
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_extra_load_store_instructions_unpriviledged()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder extra_load_store_instructions_unpriviledged")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_extra_load_store_instructions_unpriviledged);
};

class NamedForbiddenUncondDecoder_Blx_Rule_23_A2_P58
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Blx_Rule_23_A2_P58()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Blx_Rule_23_A2_P58")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Blx_Rule_23_A2_P58 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Blx_Rule_23_A2_P58);
};

class NamedForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Cdp2_Rule_28_A2_P68")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68);
};

class NamedForbiddenUncondDecoder_Clrex_Rule_30_A1_P70
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Clrex_Rule_30_A1_P70()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Clrex_Rule_30_A1_P70")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Clrex_Rule_30_A1_P70 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Clrex_Rule_30_A1_P70);
};

class NamedForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Cps_Rule_b6_1_1_A1_B6_3")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3);
};

class NamedForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Ldc2_Rule_51_A2_P106")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106);
};

class NamedForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Ldc2_Rule_52_A2_P108")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108);
};

class NamedForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Mcr2_Rule_92_A2_P186")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186);
};

class NamedForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Mcrr2_Rule_93_A2_P188")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188);
};

class NamedForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Mrc2_Rule_100_A2_P202")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202);
};

class NamedForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Mrrc2_Rule_101_A2_P204")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204);
};

class NamedForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Rfe_Rule_B6_1_10_A1_B6_16")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16);
};

class NamedForbiddenUncondDecoder_Setend_Rule_157_P314
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Setend_Rule_157_P314()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Setend_Rule_157_P314")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Setend_Rule_157_P314 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Setend_Rule_157_P314);
};

class NamedForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Srs_Rule_B6_1_10_A1_B6_20")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20);
};

class NamedForbiddenUncondDecoder_Stc2_Rule_188_A2_P372
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Stc2_Rule_188_A2_P372()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Stc2_Rule_188_A2_P372")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Stc2_Rule_188_A2_P372 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Stc2_Rule_188_A2_P372);
};

class NamedForbiddenUncondDecoder_Unallocated_hints
    : public NamedClassDecoder {
 public:
  NamedForbiddenUncondDecoder_Unallocated_hints()
    : NamedClassDecoder(decoder_, "ForbiddenUncondDecoder Unallocated_hints")
  {}

 private:
  nacl_arm_dec::ForbiddenUncondDecoder_Unallocated_hints decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenUncondDecoder_Unallocated_hints);
};

class NamedInstructionBarrier_Isb_Rule_49_A1_P102
    : public NamedClassDecoder {
 public:
  NamedInstructionBarrier_Isb_Rule_49_A1_P102()
    : NamedClassDecoder(decoder_, "InstructionBarrier Isb_Rule_49_A1_P102")
  {}

 private:
  nacl_arm_dec::InstructionBarrier_Isb_Rule_49_A1_P102 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedInstructionBarrier_Isb_Rule_49_A1_P102);
};

class NamedLdrImmediateOp_Ldr_Rule_58_A1_P120
    : public NamedClassDecoder {
 public:
  NamedLdrImmediateOp_Ldr_Rule_58_A1_P120()
    : NamedClassDecoder(decoder_, "LdrImmediateOp Ldr_Rule_58_A1_P120")
  {}

 private:
  nacl_arm_dec::LdrImmediateOp_Ldr_Rule_58_A1_P120 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediateOp_Ldr_Rule_58_A1_P120);
};

class NamedLoad2RegisterImm12Op_Ldr_Rule_59_A1_P122
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm12Op_Ldr_Rule_59_A1_P122()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op Ldr_Rule_59_A1_P122")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_Ldr_Rule_59_A1_P122 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_Ldr_Rule_59_A1_P122);
};

class NamedLoad2RegisterImm12Op_Ldrb_Rule_62_A1_P128
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm12Op_Ldrb_Rule_62_A1_P128()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op Ldrb_Rule_62_A1_P128")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_Ldrb_Rule_62_A1_P128);
};

class NamedLoad2RegisterImm12Op_Ldrb_Rule_63_A1_P130
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm12Op_Ldrb_Rule_63_A1_P130()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op Ldrb_Rule_63_A1_P130")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_Ldrb_Rule_63_A1_P130);
};

class NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8DoubleOp Ldrd_Rule_66_A1_P136")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136);
};

class NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8DoubleOp Ldrd_Rule_67_A1_P138")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138);
};

class NamedLoad2RegisterImm8Op_Ldrh_Rule_74_A1_P152
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_Ldrh_Rule_74_A1_P152()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrh_Rule_74_A1_P152")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrh_Rule_74_A1_P152);
};

class NamedLoad2RegisterImm8Op_Ldrh_Rule_75_A1_P154
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_Ldrh_Rule_75_A1_P154()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrh_Rule_75_A1_P154")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrh_Rule_75_A1_P154);
};

class NamedLoad2RegisterImm8Op_Ldrsb_Rule_78_A1_P160
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_Ldrsb_Rule_78_A1_P160()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrsb_Rule_78_A1_P160")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrsb_Rule_78_A1_P160);
};

class NamedLoad2RegisterImm8Op_Ldrsh_Rule_82_A1_P168
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_Ldrsh_Rule_82_A1_P168()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrsh_Rule_82_A1_P168")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrsh_Rule_82_A1_P168);
};

class NamedLoad2RegisterImm8Op_Ldrsh_Rule_83_A1_P170
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_Ldrsh_Rule_83_A1_P170()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op Ldrsh_Rule_83_A1_P170")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_Ldrsh_Rule_83_A1_P170);
};

class NamedLoad2RegisterImm8Op_ldrsb_Rule_79_A1_162
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_ldrsb_Rule_79_A1_162()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op ldrsb_Rule_79_A1_162")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_ldrsb_Rule_79_A1_162 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_ldrsb_Rule_79_A1_162);
};

class NamedLoad3RegisterDoubleOp_Ldrd_Rule_68_A1_P140
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterDoubleOp_Ldrd_Rule_68_A1_P140()
    : NamedClassDecoder(decoder_, "Load3RegisterDoubleOp Ldrd_Rule_68_A1_P140")
  {}

 private:
  nacl_arm_dec::Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterDoubleOp_Ldrd_Rule_68_A1_P140);
};

class NamedLoad3RegisterImm5Op_Ldr_Rule_60_A1_P124
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterImm5Op_Ldr_Rule_60_A1_P124()
    : NamedClassDecoder(decoder_, "Load3RegisterImm5Op Ldr_Rule_60_A1_P124")
  {}

 private:
  nacl_arm_dec::Load3RegisterImm5Op_Ldr_Rule_60_A1_P124 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterImm5Op_Ldr_Rule_60_A1_P124);
};

class NamedLoad3RegisterImm5Op_Ldrb_Rule_64_A1_P132
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterImm5Op_Ldrb_Rule_64_A1_P132()
    : NamedClassDecoder(decoder_, "Load3RegisterImm5Op Ldrb_Rule_64_A1_P132")
  {}

 private:
  nacl_arm_dec::Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterImm5Op_Ldrb_Rule_64_A1_P132);
};

class NamedLoad3RegisterOp_Ldrh_Rule_76_A1_P156
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterOp_Ldrh_Rule_76_A1_P156()
    : NamedClassDecoder(decoder_, "Load3RegisterOp Ldrh_Rule_76_A1_P156")
  {}

 private:
  nacl_arm_dec::Load3RegisterOp_Ldrh_Rule_76_A1_P156 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_Ldrh_Rule_76_A1_P156);
};

class NamedLoad3RegisterOp_Ldrsb_Rule_80_A1_P164
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterOp_Ldrsb_Rule_80_A1_P164()
    : NamedClassDecoder(decoder_, "Load3RegisterOp Ldrsb_Rule_80_A1_P164")
  {}

 private:
  nacl_arm_dec::Load3RegisterOp_Ldrsb_Rule_80_A1_P164 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_Ldrsb_Rule_80_A1_P164);
};

class NamedLoad3RegisterOp_Ldrsh_Rule_84_A1_P172
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterOp_Ldrsh_Rule_84_A1_P172()
    : NamedClassDecoder(decoder_, "Load3RegisterOp Ldrsh_Rule_84_A1_P172")
  {}

 private:
  nacl_arm_dec::Load3RegisterOp_Ldrsh_Rule_84_A1_P172 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_Ldrsh_Rule_84_A1_P172);
};

class NamedLoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146
    : public NamedClassDecoder {
 public:
  NamedLoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterDoubleOp Ldrexd_Rule_71_A1_P146")
  {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146);
};

class NamedLoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142
    : public NamedClassDecoder {
 public:
  NamedLoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp Ldrex_Rule_69_A1_P142")
  {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142);
};

class NamedLoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144
    : public NamedClassDecoder {
 public:
  NamedLoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp Ldrexb_Rule_70_A1_P144")
  {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144);
};

class NamedLoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148
    : public NamedClassDecoder {
 public:
  NamedLoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp Ldrexh_Rule_72_A1_P148")
  {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148);
};

class NamedLoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110()
    : NamedClassDecoder(decoder_, "LoadRegisterList Ldm_Ldmia_Ldmfd_Rule_53_A1_P110")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110);
};

class NamedLoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112()
    : NamedClassDecoder(decoder_, "LoadRegisterList Ldmda_Ldmfa_Rule_54_A1_P112")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112);
};

class NamedLoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114()
    : NamedClassDecoder(decoder_, "LoadRegisterList Ldmdb_Ldmea_Rule_55_A1_P114")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114);
};

class NamedLoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116()
    : NamedClassDecoder(decoder_, "LoadRegisterList Ldmib_Ldmed_Rule_56_A1_P116")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116);
};

class NamedLoadRegisterList_Pop_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_Pop_Rule_A1()
    : NamedClassDecoder(decoder_, "LoadRegisterList Pop_Rule_A1")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_Pop_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_Pop_Rule_A1);
};

class NamedLoadVectorRegister_Vldr_Rule_320_A1_A2_P628
    : public NamedClassDecoder {
 public:
  NamedLoadVectorRegister_Vldr_Rule_320_A1_A2_P628()
    : NamedClassDecoder(decoder_, "LoadVectorRegister Vldr_Rule_320_A1_A2_P628")
  {}

 private:
  nacl_arm_dec::LoadVectorRegister_Vldr_Rule_320_A1_A2_P628 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadVectorRegister_Vldr_Rule_320_A1_A2_P628);
};

class NamedLoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626
    : public NamedClassDecoder {
 public:
  NamedLoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626()
    : NamedClassDecoder(decoder_, "LoadVectorRegisterList Vldm_Rule_318_A1_A2_P626")
  {}

 private:
  nacl_arm_dec::LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626);
};

class NamedLoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626
    : public NamedClassDecoder {
 public:
  NamedLoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626()
    : NamedClassDecoder(decoder_, "LoadVectorRegisterList Vldm_Rule_319_A1_A2_P626")
  {}

 private:
  nacl_arm_dec::LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626);
};

class NamedLoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694
    : public NamedClassDecoder {
 public:
  NamedLoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694()
    : NamedClassDecoder(decoder_, "LoadVectorRegisterList Vpop_Rule_354_A1_A2_P694")
  {}

 private:
  nacl_arm_dec::LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694);
};

class NamedMaskedBinary2RegisterImmediateOp_BIC_immediate
    : public NamedClassDecoder {
 public:
  NamedMaskedBinary2RegisterImmediateOp_BIC_immediate()
    : NamedClassDecoder(decoder_, "MaskedBinary2RegisterImmediateOp BIC_immediate")
  {}

 private:
  nacl_arm_dec::MaskedBinary2RegisterImmediateOp_BIC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMaskedBinary2RegisterImmediateOp_BIC_immediate);
};

class NamedMaskedBinaryRegisterImmediateTest_TST_immediate
    : public NamedClassDecoder {
 public:
  NamedMaskedBinaryRegisterImmediateTest_TST_immediate()
    : NamedClassDecoder(decoder_, "MaskedBinaryRegisterImmediateTest TST_immediate")
  {}

 private:
  nacl_arm_dec::MaskedBinaryRegisterImmediateTest_TST_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMaskedBinaryRegisterImmediateTest_TST_immediate);
};

class NamedMoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedMoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1()
    : NamedClassDecoder(decoder_, "MoveDoubleVfpRegisterOp Vmov_one_D_Rule_A1")
  {}

 private:
  nacl_arm_dec::MoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1);
};

class NamedMoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedMoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1()
    : NamedClassDecoder(decoder_, "MoveDoubleVfpRegisterOp Vmov_two_S_Rule_A1")
  {}

 private:
  nacl_arm_dec::MoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1);
};

class NamedMoveImmediate12ToApsr_Msr_Rule_103_A1_P208
    : public NamedClassDecoder {
 public:
  NamedMoveImmediate12ToApsr_Msr_Rule_103_A1_P208()
    : NamedClassDecoder(decoder_, "MoveImmediate12ToApsr Msr_Rule_103_A1_P208")
  {}

 private:
  nacl_arm_dec::MoveImmediate12ToApsr_Msr_Rule_103_A1_P208 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveImmediate12ToApsr_Msr_Rule_103_A1_P208);
};

class NamedMoveVfpRegisterOp_Vmov_Rule_330_A1_P648
    : public NamedClassDecoder {
 public:
  NamedMoveVfpRegisterOp_Vmov_Rule_330_A1_P648()
    : NamedClassDecoder(decoder_, "MoveVfpRegisterOp Vmov_Rule_330_A1_P648")
  {}

 private:
  nacl_arm_dec::MoveVfpRegisterOp_Vmov_Rule_330_A1_P648 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveVfpRegisterOp_Vmov_Rule_330_A1_P648);
};

class NamedMoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644
    : public NamedClassDecoder {
 public:
  NamedMoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644()
    : NamedClassDecoder(decoder_, "MoveVfpRegisterOpWithTypeSel Vmov_Rule_328_A1_P644")
  {}

 private:
  nacl_arm_dec::MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644);
};

class NamedMoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646
    : public NamedClassDecoder {
 public:
  NamedMoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646()
    : NamedClassDecoder(decoder_, "MoveVfpRegisterOpWithTypeSel Vmov_Rule_329_A1_P646")
  {}

 private:
  nacl_arm_dec::MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646);
};

class NamedPermanentlyUndefined_Udf_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedPermanentlyUndefined_Udf_Rule_A1()
    : NamedClassDecoder(decoder_, "PermanentlyUndefined Udf_Rule_A1")
  {}

 private:
  nacl_arm_dec::PermanentlyUndefined_Udf_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPermanentlyUndefined_Udf_Rule_A1);
};

class NamedPreloadRegisterImm12Op_Pld_Rule_117_A1_P236
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterImm12Op_Pld_Rule_117_A1_P236()
    : NamedClassDecoder(decoder_, "PreloadRegisterImm12Op Pld_Rule_117_A1_P236")
  {}

 private:
  nacl_arm_dec::PreloadRegisterImm12Op_Pld_Rule_117_A1_P236 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterImm12Op_Pld_Rule_117_A1_P236);
};

class NamedPreloadRegisterImm12Op_Pld_Rule_118_A1_P238
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterImm12Op_Pld_Rule_118_A1_P238()
    : NamedClassDecoder(decoder_, "PreloadRegisterImm12Op Pld_Rule_118_A1_P238")
  {}

 private:
  nacl_arm_dec::PreloadRegisterImm12Op_Pld_Rule_118_A1_P238 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterImm12Op_Pld_Rule_118_A1_P238);
};

class NamedPreloadRegisterImm12Op_Pldw_Rule_117_A1_P236
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterImm12Op_Pldw_Rule_117_A1_P236()
    : NamedClassDecoder(decoder_, "PreloadRegisterImm12Op Pldw_Rule_117_A1_P236")
  {}

 private:
  nacl_arm_dec::PreloadRegisterImm12Op_Pldw_Rule_117_A1_P236 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterImm12Op_Pldw_Rule_117_A1_P236);
};

class NamedPreloadRegisterImm12Op_Pli_Rule_120_A1_P242
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterImm12Op_Pli_Rule_120_A1_P242()
    : NamedClassDecoder(decoder_, "PreloadRegisterImm12Op Pli_Rule_120_A1_P242")
  {}

 private:
  nacl_arm_dec::PreloadRegisterImm12Op_Pli_Rule_120_A1_P242 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterImm12Op_Pli_Rule_120_A1_P242);
};

class NamedPreloadRegisterPairOp_Pld_Rule_119_A1_P240
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterPairOp_Pld_Rule_119_A1_P240()
    : NamedClassDecoder(decoder_, "PreloadRegisterPairOp Pld_Rule_119_A1_P240")
  {}

 private:
  nacl_arm_dec::PreloadRegisterPairOp_Pld_Rule_119_A1_P240 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterPairOp_Pld_Rule_119_A1_P240);
};

class NamedPreloadRegisterPairOp_Pldw_Rule_119_A1_P240
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterPairOp_Pldw_Rule_119_A1_P240()
    : NamedClassDecoder(decoder_, "PreloadRegisterPairOp Pldw_Rule_119_A1_P240")
  {}

 private:
  nacl_arm_dec::PreloadRegisterPairOp_Pldw_Rule_119_A1_P240 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterPairOp_Pldw_Rule_119_A1_P240);
};

class NamedPreloadRegisterPairOp_Pli_Rule_121_A1_P244
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterPairOp_Pli_Rule_121_A1_P244()
    : NamedClassDecoder(decoder_, "PreloadRegisterPairOp Pli_Rule_121_A1_P244")
  {}

 private:
  nacl_arm_dec::PreloadRegisterPairOp_Pli_Rule_121_A1_P244 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterPairOp_Pli_Rule_121_A1_P244);
};

class NamedStore2RegisterImm12Op_Str_Rule_194_A1_P384
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm12Op_Str_Rule_194_A1_P384()
    : NamedClassDecoder(decoder_, "Store2RegisterImm12Op Str_Rule_194_A1_P384")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm12Op_Str_Rule_194_A1_P384 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm12Op_Str_Rule_194_A1_P384);
};

class NamedStore2RegisterImm12Op_Strb_Rule_197_A1_P390
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm12Op_Strb_Rule_197_A1_P390()
    : NamedClassDecoder(decoder_, "Store2RegisterImm12Op Strb_Rule_197_A1_P390")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm12Op_Strb_Rule_197_A1_P390 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm12Op_Strb_Rule_197_A1_P390);
};

class NamedStore2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248()
    : NamedClassDecoder(decoder_, "Store2RegisterImm12OpRnNotRtOnWriteback Push_Rule_123_A2_P248")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248);
};

class NamedStore2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396()
    : NamedClassDecoder(decoder_, "Store2RegisterImm8DoubleOp Strd_Rule_200_A1_P396")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396);
};

class NamedStore2RegisterImm8Op_Strh_Rule_207_A1_P410
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm8Op_Strh_Rule_207_A1_P410()
    : NamedClassDecoder(decoder_, "Store2RegisterImm8Op Strh_Rule_207_A1_P410")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm8Op_Strh_Rule_207_A1_P410 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm8Op_Strh_Rule_207_A1_P410);
};

class NamedStore3RegisterDoubleOp_Strd_Rule_201_A1_P398
    : public NamedClassDecoder {
 public:
  NamedStore3RegisterDoubleOp_Strd_Rule_201_A1_P398()
    : NamedClassDecoder(decoder_, "Store3RegisterDoubleOp Strd_Rule_201_A1_P398")
  {}

 private:
  nacl_arm_dec::Store3RegisterDoubleOp_Strd_Rule_201_A1_P398 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterDoubleOp_Strd_Rule_201_A1_P398);
};

class NamedStore3RegisterImm5Op_Str_Rule_195_A1_P386
    : public NamedClassDecoder {
 public:
  NamedStore3RegisterImm5Op_Str_Rule_195_A1_P386()
    : NamedClassDecoder(decoder_, "Store3RegisterImm5Op Str_Rule_195_A1_P386")
  {}

 private:
  nacl_arm_dec::Store3RegisterImm5Op_Str_Rule_195_A1_P386 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterImm5Op_Str_Rule_195_A1_P386);
};

class NamedStore3RegisterImm5Op_Strb_Rule_198_A1_P392
    : public NamedClassDecoder {
 public:
  NamedStore3RegisterImm5Op_Strb_Rule_198_A1_P392()
    : NamedClassDecoder(decoder_, "Store3RegisterImm5Op Strb_Rule_198_A1_P392")
  {}

 private:
  nacl_arm_dec::Store3RegisterImm5Op_Strb_Rule_198_A1_P392 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterImm5Op_Strb_Rule_198_A1_P392);
};

class NamedStore3RegisterOp_Strh_Rule_208_A1_P412
    : public NamedClassDecoder {
 public:
  NamedStore3RegisterOp_Strh_Rule_208_A1_P412()
    : NamedClassDecoder(decoder_, "Store3RegisterOp Strh_Rule_208_A1_P412")
  {}

 private:
  nacl_arm_dec::Store3RegisterOp_Strh_Rule_208_A1_P412 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterOp_Strh_Rule_208_A1_P412);
};

class NamedStoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404
    : public NamedClassDecoder {
 public:
  NamedStoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterDoubleOp Strexd_Rule_204_A1_P404")
  {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404);
};

class NamedStoreExclusive3RegisterOp_Strex_Rule_202_A1_P400
    : public NamedClassDecoder {
 public:
  NamedStoreExclusive3RegisterOp_Strex_Rule_202_A1_P400()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp Strex_Rule_202_A1_P400")
  {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_Strex_Rule_202_A1_P400);
};

class NamedStoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402
    : public NamedClassDecoder {
 public:
  NamedStoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp Strexb_Rule_203_A1_P402")
  {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402);
};

class NamedStoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406
    : public NamedClassDecoder {
 public:
  NamedStoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp Strexh_Rule_205_A1_P406")
  {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406);
};

class NamedStoreRegisterList_Push_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_Push_Rule_A1()
    : NamedClassDecoder(decoder_, "StoreRegisterList Push_Rule_A1")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_Push_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_Push_Rule_A1);
};

class NamedStoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374()
    : NamedClassDecoder(decoder_, "StoreRegisterList Stm_Stmia_Stmea_Rule_189_A1_P374")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374);
};

class NamedStoreRegisterList_Stmda_Stmed_Rule_190_A1_P376
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_Stmda_Stmed_Rule_190_A1_P376()
    : NamedClassDecoder(decoder_, "StoreRegisterList Stmda_Stmed_Rule_190_A1_P376")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_Stmda_Stmed_Rule_190_A1_P376 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_Stmda_Stmed_Rule_190_A1_P376);
};

class NamedStoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378()
    : NamedClassDecoder(decoder_, "StoreRegisterList Stmdb_Stmfd_Rule_191_A1_P378")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378);
};

class NamedStoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380()
    : NamedClassDecoder(decoder_, "StoreRegisterList Stmib_Stmfa_Rule_192_A1_P380")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380);
};

class NamedStoreVectorRegister_Vstr_Rule_400_A1_A2_P786
    : public NamedClassDecoder {
 public:
  NamedStoreVectorRegister_Vstr_Rule_400_A1_A2_P786()
    : NamedClassDecoder(decoder_, "StoreVectorRegister Vstr_Rule_400_A1_A2_P786")
  {}

 private:
  nacl_arm_dec::StoreVectorRegister_Vstr_Rule_400_A1_A2_P786 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreVectorRegister_Vstr_Rule_400_A1_A2_P786);
};

class NamedStoreVectorRegisterList_Vpush_355_A1_A2_P696
    : public NamedClassDecoder {
 public:
  NamedStoreVectorRegisterList_Vpush_355_A1_A2_P696()
    : NamedClassDecoder(decoder_, "StoreVectorRegisterList Vpush_355_A1_A2_P696")
  {}

 private:
  nacl_arm_dec::StoreVectorRegisterList_Vpush_355_A1_A2_P696 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreVectorRegisterList_Vpush_355_A1_A2_P696);
};

class NamedStoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784
    : public NamedClassDecoder {
 public:
  NamedStoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784()
    : NamedClassDecoder(decoder_, "StoreVectorRegisterList Vstm_Rule_399_A1_A2_P784")
  {}

 private:
  nacl_arm_dec::StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784);
};

class NamedUnary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46()
    : NamedClassDecoder(decoder_, "Unary1RegisterBitRangeMsbGeLsb Bfc_17_A1_P46")
  {}

 private:
  nacl_arm_dec::Unary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46);
};

class NamedUnary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOp12DynCodeReplace MOV_immediate_A1")
  {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1);
};

class NamedUnary1RegisterImmediateOp12DynCodeReplace_MVN_immediate
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterImmediateOp12DynCodeReplace_MVN_immediate()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOp12DynCodeReplace MVN_immediate")
  {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOp12DynCodeReplace_MVN_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOp12DynCodeReplace_MVN_immediate);
};

class NamedUnary1RegisterImmediateOpDynCodeReplace_Movt_Rule_99_A1_P200
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterImmediateOpDynCodeReplace_Movt_Rule_99_A1_P200()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOpDynCodeReplace Movt_Rule_99_A1_P200")
  {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOpDynCodeReplace_Movt_Rule_99_A1_P200 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOpDynCodeReplace_Movt_Rule_99_A1_P200);
};

class NamedUnary1RegisterImmediateOpDynCodeReplace_Movw_Rule_96_A2_P194
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterImmediateOpDynCodeReplace_Movw_Rule_96_A2_P194()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOpDynCodeReplace Movw_Rule_96_A2_P194")
  {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOpDynCodeReplace_Movw_Rule_96_A2_P194 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOpDynCodeReplace_Movw_Rule_96_A2_P194);
};

class NamedUnary1RegisterImmediateOpPc_ADR_A1
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterImmediateOpPc_ADR_A1()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOpPc ADR_A1")
  {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOpPc_ADR_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOpPc_ADR_A1);
};

class NamedUnary1RegisterImmediateOpPc_ADR_A2
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterImmediateOpPc_ADR_A2()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOpPc ADR_A2")
  {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOpPc_ADR_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOpPc_ADR_A2);
};

class NamedUnary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10()
    : NamedClassDecoder(decoder_, "Unary1RegisterSet Mrs_Rule_102_A1_P206_Or_B6_10")
  {}

 private:
  nacl_arm_dec::Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10);
};

class NamedUnary1RegisterUse_Msr_Rule_104_A1_P210
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterUse_Msr_Rule_104_A1_P210()
    : NamedClassDecoder(decoder_, "Unary1RegisterUse Msr_Rule_104_A1_P210")
  {}

 private:
  nacl_arm_dec::Unary1RegisterUse_Msr_Rule_104_A1_P210 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterUse_Msr_Rule_104_A1_P210);
};

class NamedUnary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOpRegsNotPc Sxtb16_Rule_224_A1_P442")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442);
};

class NamedUnary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOpRegsNotPc Sxtb_Rule_223_A1_P440")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440);
};

class NamedUnary2RegisterOp_MOV_register
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOp_MOV_register()
    : NamedClassDecoder(decoder_, "Unary2RegisterOp MOV_register")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOp_MOV_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOp_MOV_register);
};

class NamedUnary2RegisterOp_RRX
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOp_RRX()
    : NamedClassDecoder(decoder_, "Unary2RegisterOp RRX")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOp_RRX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOp_RRX);
};

class NamedUnary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPc Clz_Rule_31_A1_P72")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72);
};

class NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPcNoCondUpdates Rbit_Rule_134_A1_P270")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270);
};

class NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPcNoCondUpdates Rev16_Rule_136_A1_P274")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274);
};

class NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPcNoCondUpdates Rev_Rule_135_A1_P272")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272);
};

class NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPcNoCondUpdates Revsh_Rule_137_A1_P276")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276);
};

class NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPcNoCondUpdates Sxth_Rule_225_A1_P444")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444);
};

class NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPcNoCondUpdates Uxtb16_Rule_264_A1_P522")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522);
};

class NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPcNoCondUpdates Uxtb_Rule_263_A1_P520")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520);
};

class NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPcNoCondUpdates Uxth_Rule_265_A1_P524")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524);
};

class NamedUnary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364()
    : NamedClassDecoder(decoder_, "Unary2RegisterSatImmedShiftedOp Ssat16_Rule_184_A1_P364")
  {}

 private:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364);
};

class NamedUnary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362()
    : NamedClassDecoder(decoder_, "Unary2RegisterSatImmedShiftedOp Ssat_Rule_183_A1_P362")
  {}

 private:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362);
};

class NamedUnary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506()
    : NamedClassDecoder(decoder_, "Unary2RegisterSatImmedShiftedOp Usat16_Rule_256_A1_P506")
  {}

 private:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506);
};

class NamedUnary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504()
    : NamedClassDecoder(decoder_, "Unary2RegisterSatImmedShiftedOp Usat_Rule_255_A1_P504")
  {}

 private:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504);
};

class NamedUnary2RegisterShiftedOp_ASR_immediate
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterShiftedOp_ASR_immediate()
    : NamedClassDecoder(decoder_, "Unary2RegisterShiftedOp ASR_immediate")
  {}

 private:
  nacl_arm_dec::Unary2RegisterShiftedOp_ASR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterShiftedOp_ASR_immediate);
};

class NamedUnary2RegisterShiftedOp_LSR_immediate
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterShiftedOp_LSR_immediate()
    : NamedClassDecoder(decoder_, "Unary2RegisterShiftedOp LSR_immediate")
  {}

 private:
  nacl_arm_dec::Unary2RegisterShiftedOp_LSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterShiftedOp_LSR_immediate);
};

class NamedUnary2RegisterShiftedOp_MVN_register
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterShiftedOp_MVN_register()
    : NamedClassDecoder(decoder_, "Unary2RegisterShiftedOp MVN_register")
  {}

 private:
  nacl_arm_dec::Unary2RegisterShiftedOp_MVN_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterShiftedOp_MVN_register);
};

class NamedUnary2RegisterShiftedOpImmNotZero_LSL_immediate
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterShiftedOpImmNotZero_LSL_immediate()
    : NamedClassDecoder(decoder_, "Unary2RegisterShiftedOpImmNotZero LSL_immediate")
  {}

 private:
  nacl_arm_dec::Unary2RegisterShiftedOpImmNotZero_LSL_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterShiftedOpImmNotZero_LSL_immediate);
};

class NamedUnary2RegisterShiftedOpImmNotZero_ROR_immediate
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterShiftedOpImmNotZero_ROR_immediate()
    : NamedClassDecoder(decoder_, "Unary2RegisterShiftedOpImmNotZero ROR_immediate")
  {}

 private:
  nacl_arm_dec::Unary2RegisterShiftedOpImmNotZero_ROR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterShiftedOpImmNotZero_ROR_immediate);
};

class NamedUnary3RegisterShiftedOp_MVN_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedUnary3RegisterShiftedOp_MVN_register_shifted_register()
    : NamedClassDecoder(decoder_, "Unary3RegisterShiftedOp MVN_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Unary3RegisterShiftedOp_MVN_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary3RegisterShiftedOp_MVN_register_shifted_register);
};

class NamedUndefined_None
    : public NamedClassDecoder {
 public:
  NamedUndefined_None()
    : NamedClassDecoder(decoder_, "Undefined None")
  {}

 private:
  nacl_arm_dec::Undefined_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUndefined_None);
};

class NamedUndefinedCondDecoder_Undefined_A5_6
    : public NamedClassDecoder {
 public:
  NamedUndefinedCondDecoder_Undefined_A5_6()
    : NamedClassDecoder(decoder_, "UndefinedCondDecoder Undefined_A5_6")
  {}

 private:
  nacl_arm_dec::UndefinedCondDecoder_Undefined_A5_6 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUndefinedCondDecoder_Undefined_A5_6);
};

class NamedUndefinedCondDecoder_None
    : public NamedClassDecoder {
 public:
  NamedUndefinedCondDecoder_None()
    : NamedClassDecoder(decoder_, "UndefinedCondDecoder None")
  {}

 private:
  nacl_arm_dec::UndefinedCondDecoder_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUndefinedCondDecoder_None);
};

class NamedUnpredictableUncondDecoder_Unpredictable
    : public NamedClassDecoder {
 public:
  NamedUnpredictableUncondDecoder_Unpredictable()
    : NamedClassDecoder(decoder_, "UnpredictableUncondDecoder Unpredictable")
  {}

 private:
  nacl_arm_dec::UnpredictableUncondDecoder_Unpredictable decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnpredictableUncondDecoder_Unpredictable);
};

class NamedVector1RegisterImmediate_BIT_VBIC_immediate
    : public NamedClassDecoder {
 public:
  NamedVector1RegisterImmediate_BIT_VBIC_immediate()
    : NamedClassDecoder(decoder_, "Vector1RegisterImmediate_BIT VBIC_immediate")
  {}

 private:
  nacl_arm_dec::Vector1RegisterImmediate_BIT_VBIC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector1RegisterImmediate_BIT_VBIC_immediate);
};

class NamedVector1RegisterImmediate_BIT_VORR_immediate
    : public NamedClassDecoder {
 public:
  NamedVector1RegisterImmediate_BIT_VORR_immediate()
    : NamedClassDecoder(decoder_, "Vector1RegisterImmediate_BIT VORR_immediate")
  {}

 private:
  nacl_arm_dec::Vector1RegisterImmediate_BIT_VORR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector1RegisterImmediate_BIT_VORR_immediate);
};

class NamedVector1RegisterImmediate_MOV_VMOV_immediate_A1
    : public NamedClassDecoder {
 public:
  NamedVector1RegisterImmediate_MOV_VMOV_immediate_A1()
    : NamedClassDecoder(decoder_, "Vector1RegisterImmediate_MOV VMOV_immediate_A1")
  {}

 private:
  nacl_arm_dec::Vector1RegisterImmediate_MOV_VMOV_immediate_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector1RegisterImmediate_MOV_VMOV_immediate_A1);
};

class NamedVector1RegisterImmediate_MVN_VMVN_immediate
    : public NamedClassDecoder {
 public:
  NamedVector1RegisterImmediate_MVN_VMVN_immediate()
    : NamedClassDecoder(decoder_, "Vector1RegisterImmediate_MVN VMVN_immediate")
  {}

 private:
  nacl_arm_dec::Vector1RegisterImmediate_MVN_VMVN_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector1RegisterImmediate_MVN_VMVN_immediate);
};

class NamedVector2RegisterMiscellaneous_CVT_F2I_VCVT
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_CVT_F2I_VCVT()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_CVT_F2I VCVT")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_CVT_F2I_VCVT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_CVT_F2I_VCVT);
};

class NamedVector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_CVT_H2S CVT_between_half_precision_and_single_precision")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision);
};

class NamedVector2RegisterMiscellaneous_F32_VABS_A1
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VABS_A1()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VABS_A1")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VABS_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VABS_A1);
};

class NamedVector2RegisterMiscellaneous_F32_VCEQ_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VCEQ_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VCEQ_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VCEQ_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VCEQ_immediate_0);
};

class NamedVector2RegisterMiscellaneous_F32_VCGE_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VCGE_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VCGE_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VCGE_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VCGE_immediate_0);
};

class NamedVector2RegisterMiscellaneous_F32_VCGT_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VCGT_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VCGT_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VCGT_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VCGT_immediate_0);
};

class NamedVector2RegisterMiscellaneous_F32_VCLE_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VCLE_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VCLE_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VCLE_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VCLE_immediate_0);
};

class NamedVector2RegisterMiscellaneous_F32_VCLT_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VCLT_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VCLT_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VCLT_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VCLT_immediate_0);
};

class NamedVector2RegisterMiscellaneous_F32_VNEG
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VNEG()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VNEG")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VNEG decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VNEG);
};

class NamedVector2RegisterMiscellaneous_F32_VRECPE
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VRECPE()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VRECPE")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VRECPE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VRECPE);
};

class NamedVector2RegisterMiscellaneous_F32_VRSQRTE
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_F32_VRSQRTE()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_F32 VRSQRTE")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_F32_VRSQRTE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_F32_VRSQRTE);
};

class NamedVector2RegisterMiscellaneous_I16_32_64N_VQMOVN
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_I16_32_64N_VQMOVN()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_I16_32_64N VQMOVN")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_I16_32_64N_VQMOVN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_I16_32_64N_VQMOVN);
};

class NamedVector2RegisterMiscellaneous_I16_32_64N_VQMOVUN
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_I16_32_64N_VQMOVUN()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_I16_32_64N VQMOVUN")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_I16_32_64N_VQMOVUN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_I16_32_64N_VQMOVUN);
};

class NamedVector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_I8_16_32L VSHLL_A2")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2);
};

class NamedVector2RegisterMiscellaneous_RG_VREV16
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_RG_VREV16()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_RG VREV16")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_RG_VREV16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_RG_VREV16);
};

class NamedVector2RegisterMiscellaneous_RG_VREV32
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_RG_VREV32()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_RG VREV32")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_RG_VREV32 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_RG_VREV32);
};

class NamedVector2RegisterMiscellaneous_RG_VREV64
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_RG_VREV64()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_RG VREV64")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_RG_VREV64 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_RG_VREV64);
};

class NamedVector2RegisterMiscellaneous_V16_32_64N_VMOVN
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V16_32_64N_VMOVN()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V16_32_64N VMOVN")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V16_32_64N_VMOVN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V16_32_64N_VMOVN);
};

class NamedVector2RegisterMiscellaneous_V8_VCNT
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_VCNT()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8 VCNT")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_VCNT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_VCNT);
};

class NamedVector2RegisterMiscellaneous_V8_VMVN_register
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_VMVN_register()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8 VMVN_register")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_VMVN_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_VMVN_register);
};

class NamedVector2RegisterMiscellaneous_V8S_VSWP
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8S_VSWP()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8S VSWP")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8S_VSWP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8S_VSWP);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VABS_A1
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VABS_A1()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VABS_A1")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VABS_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VABS_A1);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VCEQ_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VCGE_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VCGT_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VCLE_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VCLS
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VCLS()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VCLS")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VCLS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VCLS);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VCLT_immediate_0")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VCLZ
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VCLZ()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VCLZ")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VCLZ decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VCLZ);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VNEG
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VNEG()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VNEG")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VNEG decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VNEG);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VPADAL
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VPADAL()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VPADAL")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VPADAL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VPADAL);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VPADDL
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VPADDL()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VPADDL")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VPADDL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VPADDL);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VQABS
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VQABS()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VQABS")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VQABS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VQABS);
};

class NamedVector2RegisterMiscellaneous_V8_16_32_VQNEG
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32_VQNEG()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32 VQNEG")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32_VQNEG decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32_VQNEG);
};

class NamedVector2RegisterMiscellaneous_V8_16_32I_VUZP
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32I_VUZP()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32I VUZP")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32I_VUZP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32I_VUZP);
};

class NamedVector2RegisterMiscellaneous_V8_16_32I_VZIP
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32I_VZIP()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32I VZIP")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32I_VZIP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32I_VZIP);
};

class NamedVector2RegisterMiscellaneous_V8_16_32T_VTRN
    : public NamedClassDecoder {
 public:
  NamedVector2RegisterMiscellaneous_V8_16_32T_VTRN()
    : NamedClassDecoder(decoder_, "Vector2RegisterMiscellaneous_V8_16_32T VTRN")
  {}

 private:
  nacl_arm_dec::Vector2RegisterMiscellaneous_V8_16_32T_VTRN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVector2RegisterMiscellaneous_V8_16_32T_VTRN);
};

class NamedVectorBinary2RegisterScalar_F32_VMLA_by_scalar_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_F32_VMLA_by_scalar_A1()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_F32 VMLA_by_scalar_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_F32_VMLA_by_scalar_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_F32_VMLA_by_scalar_A1);
};

class NamedVectorBinary2RegisterScalar_F32_VMLS_by_scalar_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_F32_VMLS_by_scalar_A1()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_F32 VMLS_by_scalar_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_F32_VMLS_by_scalar_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_F32_VMLS_by_scalar_A1);
};

class NamedVectorBinary2RegisterScalar_F32_VMUL_by_scalar_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_F32_VMUL_by_scalar_A1()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_F32 VMUL_by_scalar_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_F32_VMUL_by_scalar_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_F32_VMUL_by_scalar_A1);
};

class NamedVectorBinary2RegisterScalar_I16_32_VMLA_by_scalar_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32_VMLA_by_scalar_A1()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32 VMLA_by_scalar_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32_VMLA_by_scalar_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32_VMLA_by_scalar_A1);
};

class NamedVectorBinary2RegisterScalar_I16_32_VMLS_by_scalar_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32_VMLS_by_scalar_A1()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32 VMLS_by_scalar_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32_VMLS_by_scalar_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32_VMLS_by_scalar_A1);
};

class NamedVectorBinary2RegisterScalar_I16_32_VMUL_by_scalar_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32_VMUL_by_scalar_A1()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32 VMUL_by_scalar_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32_VMUL_by_scalar_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32_VMUL_by_scalar_A1);
};

class NamedVectorBinary2RegisterScalar_I16_32_VQDMULH_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32_VQDMULH_A2()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32 VQDMULH_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32_VQDMULH_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32_VQDMULH_A2);
};

class NamedVectorBinary2RegisterScalar_I16_32_VQRDMULH
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32_VQRDMULH()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32 VQRDMULH")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32_VQRDMULH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32_VQRDMULH);
};

class NamedVectorBinary2RegisterScalar_I16_32L_VMLAL_by_scalar_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32L_VMLAL_by_scalar_A2()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32L VMLAL_by_scalar_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32L_VMLAL_by_scalar_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32L_VMLAL_by_scalar_A2);
};

class NamedVectorBinary2RegisterScalar_I16_32L_VMLSL_by_scalar_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32L_VMLSL_by_scalar_A2()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32L VMLSL_by_scalar_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32L_VMLSL_by_scalar_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32L_VMLSL_by_scalar_A2);
};

class NamedVectorBinary2RegisterScalar_I16_32L_VMULL_by_scalar_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32L_VMULL_by_scalar_A2()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32L VMULL_by_scalar_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32L_VMULL_by_scalar_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32L_VMULL_by_scalar_A2);
};

class NamedVectorBinary2RegisterScalar_I16_32L_VQDMLAL_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32L_VQDMLAL_A1()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32L VQDMLAL_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32L_VQDMLAL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32L_VQDMLAL_A1);
};

class NamedVectorBinary2RegisterScalar_I16_32L_VQDMLSL_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32L_VQDMLSL_A1()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32L VQDMLSL_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32L_VQDMLSL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32L_VQDMLSL_A1);
};

class NamedVectorBinary2RegisterScalar_I16_32L_VQDMULL_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterScalar_I16_32L_VQDMULL_A2()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterScalar_I16_32L VQDMULL_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterScalar_I16_32L_VQDMULL_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterScalar_I16_32L_VQDMULL_A2);
};

class NamedVectorBinary2RegisterShiftAmount_CVT_VCVT_between_floating_point_and_fixed_point
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_CVT_VCVT_between_floating_point_and_fixed_point()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_CVT VCVT_between_floating_point_and_fixed_point")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_CVT_VCVT_between_floating_point_and_fixed_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_CVT_VCVT_between_floating_point_and_fixed_point);
};

class NamedVectorBinary2RegisterShiftAmount_E8_16_32L_VSHLL_A1_or_VMOVL
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_E8_16_32L_VSHLL_A1_or_VMOVL()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_E8_16_32L VSHLL_A1_or_VMOVL")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_E8_16_32L_VSHLL_A1_or_VMOVL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_E8_16_32L_VSHLL_A1_or_VMOVL);
};

class NamedVectorBinary2RegisterShiftAmount_I_VRSHR
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_I_VRSHR()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_I VRSHR")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_I_VRSHR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_I_VRSHR);
};

class NamedVectorBinary2RegisterShiftAmount_I_VRSRA
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_I_VRSRA()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_I VRSRA")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_I_VRSRA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_I_VRSRA);
};

class NamedVectorBinary2RegisterShiftAmount_I_VSHL_immediate
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_I_VSHL_immediate()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_I VSHL_immediate")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_I_VSHL_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_I_VSHL_immediate);
};

class NamedVectorBinary2RegisterShiftAmount_I_VSHR
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_I_VSHR()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_I VSHR")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_I_VSHR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_I_VSHR);
};

class NamedVectorBinary2RegisterShiftAmount_I_VSLI
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_I_VSLI()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_I VSLI")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_I_VSLI decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_I_VSLI);
};

class NamedVectorBinary2RegisterShiftAmount_I_VSRA
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_I_VSRA()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_I VSRA")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_I_VSRA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_I_VSRA);
};

class NamedVectorBinary2RegisterShiftAmount_I_VSRI
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_I_VSRI()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_I VSRI")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_I_VSRI decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_I_VSRI);
};

class NamedVectorBinary2RegisterShiftAmount_ILS_VQSHL_VQSHLU_immediate
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_ILS_VQSHL_VQSHLU_immediate()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_ILS VQSHL_VQSHLU_immediate")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_ILS_VQSHL_VQSHLU_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_ILS_VQSHL_VQSHLU_immediate);
};

class NamedVectorBinary2RegisterShiftAmount_N16_32_64R_VRSHRN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_N16_32_64R_VRSHRN()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_N16_32_64R VRSHRN")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_N16_32_64R_VRSHRN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_N16_32_64R_VRSHRN);
};

class NamedVectorBinary2RegisterShiftAmount_N16_32_64R_VSHRN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_N16_32_64R_VSHRN()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_N16_32_64R VSHRN")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_N16_32_64R_VSHRN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_N16_32_64R_VSHRN);
};

class NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_N16_32_64RS VQRSHRN")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN);
};

class NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_N16_32_64RS VQRSHRUN")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN);
};

class NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRN()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_N16_32_64RS VQSHRN")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRN);
};

class NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRUN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRUN()
    : NamedClassDecoder(decoder_, "VectorBinary2RegisterShiftAmount_N16_32_64RS VQSHRUN")
  {}

 private:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRUN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRUN);
};

class NamedVectorBinary3RegisterDifferentLength_I16_32L_VQDMLAL_VQDMLSL_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I16_32L_VQDMLAL_VQDMLSL_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I16_32L VQDMLAL_VQDMLSL_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I16_32L_VQDMLAL_VQDMLSL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I16_32L_VQDMLAL_VQDMLSL_A1);
};

class NamedVectorBinary3RegisterDifferentLength_I16_32L_VQDMULL_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I16_32L_VQDMULL_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I16_32L VQDMULL_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I16_32L_VQDMULL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I16_32L_VQDMULL_A1);
};

class NamedVectorBinary3RegisterDifferentLength_I16_32_64_VADDHN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I16_32_64_VADDHN()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I16_32_64 VADDHN")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I16_32_64_VADDHN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I16_32_64_VADDHN);
};

class NamedVectorBinary3RegisterDifferentLength_I16_32_64_VRADDHN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I16_32_64_VRADDHN()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I16_32_64 VRADDHN")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I16_32_64_VRADDHN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I16_32_64_VRADDHN);
};

class NamedVectorBinary3RegisterDifferentLength_I16_32_64_VRSUBHN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I16_32_64_VRSUBHN()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I16_32_64 VRSUBHN")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I16_32_64_VRSUBHN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I16_32_64_VRSUBHN);
};

class NamedVectorBinary3RegisterDifferentLength_I16_32_64_VSUBHN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I16_32_64_VSUBHN()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I16_32_64 VSUBHN")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I16_32_64_VSUBHN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I16_32_64_VSUBHN);
};

class NamedVectorBinary3RegisterDifferentLength_I8_16_32_VADDL_VADDW
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I8_16_32_VADDL_VADDW()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I8_16_32 VADDL_VADDW")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I8_16_32_VADDL_VADDW decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I8_16_32_VADDL_VADDW);
};

class NamedVectorBinary3RegisterDifferentLength_I8_16_32_VSUBL_VSUBW
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I8_16_32_VSUBL_VSUBW()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I8_16_32 VSUBL_VSUBW")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I8_16_32_VSUBL_VSUBW decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I8_16_32_VSUBL_VSUBW);
};

class NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VABAL_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VABAL_A2()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I8_16_32L VABAL_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I8_16_32L_VABAL_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VABAL_A2);
};

class NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VABDL_integer_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VABDL_integer_A2()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I8_16_32L VABDL_integer_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I8_16_32L_VABDL_integer_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VABDL_integer_A2);
};

class NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VMLAL_VMLSL_integer_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VMLAL_VMLSL_integer_A2()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I8_16_32L VMLAL_VMLSL_integer_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I8_16_32L_VMLAL_VMLSL_integer_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VMLAL_VMLSL_integer_A2);
};

class NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VMULL_integer_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VMULL_integer_A2()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_I8_16_32L VMULL_integer_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_I8_16_32L_VMULL_integer_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_I8_16_32L_VMULL_integer_A2);
};

class NamedVectorBinary3RegisterDifferentLength_P8_VMULL_polynomial_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterDifferentLength_P8_VMULL_polynomial_A2()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterDifferentLength_P8 VMULL_polynomial_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength_P8_VMULL_polynomial_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterDifferentLength_P8_VMULL_polynomial_A2);
};

class NamedVectorBinary3RegisterImmOp_Vext_Rule_305_A1_P598
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterImmOp_Vext_Rule_305_A1_P598()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterImmOp Vext_Rule_305_A1_P598")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterImmOp_Vext_Rule_305_A1_P598 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterImmOp_Vext_Rule_305_A1_P598);
};

class NamedVectorBinary3RegisterLookupOp_Vtbl_Vtbx_Rule_406_A1_P798
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterLookupOp_Vtbl_Vtbx_Rule_406_A1_P798()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterLookupOp Vtbl_Vtbx_Rule_406_A1_P798")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterLookupOp_Vtbl_Vtbx_Rule_406_A1_P798 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterLookupOp_Vtbl_Vtbx_Rule_406_A1_P798);
};

class NamedVectorBinary3RegisterSameLength32P_VPADD_floating_point
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32P_VPADD_floating_point()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32P VPADD_floating_point")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32P_VPADD_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32P_VPADD_floating_point);
};

class NamedVectorBinary3RegisterSameLength32P_VPMAX
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32P_VPMAX()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32P VPMAX")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32P_VPMAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32P_VPMAX);
};

class NamedVectorBinary3RegisterSameLength32P_VPMIN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32P_VPMIN()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32P VPMIN")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32P_VPMIN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32P_VPMIN);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VABD_floating_point
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VABD_floating_point()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VABD_floating_point")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VABD_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VABD_floating_point);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VACGE
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VACGE()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VACGE")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VACGE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VACGE);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VACGT
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VACGT()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VACGT")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VACGT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VACGT);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VADD_floating_point_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VCEQ_register_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VCGE_register_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VCGE_register_A2()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VCGE_register_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VCGE_register_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VCGE_register_A2);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VCGT_register_A2
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VCGT_register_A2()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VCGT_register_A2")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VCGT_register_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VCGT_register_A2);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VFMA_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VFMA_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VFMA_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VFMA_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VFMA_A1);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VFMS_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VFMS_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VFMS_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VFMS_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VFMS_A1);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VMAX_floating_point
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VMAX_floating_point()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VMAX_floating_point")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VMAX_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VMAX_floating_point);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VMIN_floating_point
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VMIN_floating_point()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VMIN_floating_point")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VMIN_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VMIN_floating_point);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VMLA_floating_point_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VMLS_floating_point_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VMUL_floating_point_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VRECPS
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VRECPS()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VRECPS")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VRECPS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VRECPS);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VRSQRTS
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VRSQRTS()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VRSQRTS")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VRSQRTS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VRSQRTS);
};

class NamedVectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLength32_DQ VSUB_floating_point_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1);
};

class NamedVectorBinary3RegisterSameLengthDI_VPADD_integer
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDI_VPADD_integer()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDI VPADD_integer")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDI_VPADD_integer decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDI_VPADD_integer);
};

class NamedVectorBinary3RegisterSameLengthDI_VPMAX
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDI_VPMAX()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDI VPMAX")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDI_VPMAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDI_VPMAX);
};

class NamedVectorBinary3RegisterSameLengthDI_VPMIN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDI_VPMIN()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDI VPMIN")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDI_VPMIN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDI_VPMIN);
};

class NamedVectorBinary3RegisterSameLengthDQ_VADD_integer
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VADD_integer()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VADD_integer")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VADD_integer decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VADD_integer);
};

class NamedVectorBinary3RegisterSameLengthDQ_VAND_register
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VAND_register()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VAND_register")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VAND_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VAND_register);
};

class NamedVectorBinary3RegisterSameLengthDQ_VBIC_register
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VBIC_register()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VBIC_register")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VBIC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VBIC_register);
};

class NamedVectorBinary3RegisterSameLengthDQ_VBIF
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VBIF()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VBIF")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VBIF decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VBIF);
};

class NamedVectorBinary3RegisterSameLengthDQ_VBIT
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VBIT()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VBIT")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VBIT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VBIT);
};

class NamedVectorBinary3RegisterSameLengthDQ_VBSL
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VBSL()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VBSL")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VBSL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VBSL);
};

class NamedVectorBinary3RegisterSameLengthDQ_VEOR
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VEOR()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VEOR")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VEOR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VEOR);
};

class NamedVectorBinary3RegisterSameLengthDQ_VORN_register
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VORN_register()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VORN_register")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VORN_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VORN_register);
};

class NamedVectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VORR_register_or_VMOV_register_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1);
};

class NamedVectorBinary3RegisterSameLengthDQ_VQADD
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VQADD()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VQADD")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VQADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VQADD);
};

class NamedVectorBinary3RegisterSameLengthDQ_VQRSHL
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VQRSHL()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VQRSHL")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VQRSHL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VQRSHL);
};

class NamedVectorBinary3RegisterSameLengthDQ_VQSHL_register
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VQSHL_register()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VQSHL_register")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VQSHL_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VQSHL_register);
};

class NamedVectorBinary3RegisterSameLengthDQ_VQSUB
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VQSUB()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VQSUB")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VQSUB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VQSUB);
};

class NamedVectorBinary3RegisterSameLengthDQ_VRSHL
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VRSHL()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VRSHL")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VRSHL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VRSHL);
};

class NamedVectorBinary3RegisterSameLengthDQ_VSHL_register
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VSHL_register()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VSHL_register")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VSHL_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VSHL_register);
};

class NamedVectorBinary3RegisterSameLengthDQ_VSUB_integer
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQ_VSUB_integer()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQ VSUB_integer")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQ_VSUB_integer decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQ_VSUB_integer);
};

class NamedVectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI16_32 VQDMULH_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI16_32 VQRDMULH_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8P VMUL_polynomial_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VABA
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VABA()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VABA")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VABA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VABA);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VABD
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VABD()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VABD")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VABD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VABD);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VCEQ_register_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VCGE_register_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VCGT_register_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VHADD
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VHADD()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VHADD")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VHADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VHADD);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VHSUB
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VHSUB()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VHSUB")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VHSUB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VHSUB);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMAX
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMAX()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VMAX")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VMAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMAX);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMIN
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMIN()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VMIN")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VMIN decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMIN);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VMLA_integer_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VMLS_integer_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VMUL_integer_A1")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VRHADD
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VRHADD()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VRHADD")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VRHADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VRHADD);
};

class NamedVectorBinary3RegisterSameLengthDQI8_16_32_VTST
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterSameLengthDQI8_16_32_VTST()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterSameLengthDQI8_16_32 VTST")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterSameLengthDQI8_16_32_VTST decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterSameLengthDQI8_16_32_VTST);
};

class NamedVectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes
    : public NamedClassDecoder {
 public:
  NamedVectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes()
    : NamedClassDecoder(decoder_, "VectorLoadSingle1AllLanes VLD1_single_element_to_all_lanes")
  {}

 private:
  nacl_arm_dec::VectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes);
};

class NamedVectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes
    : public NamedClassDecoder {
 public:
  NamedVectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes()
    : NamedClassDecoder(decoder_, "VectorLoadSingle2AllLanes VLD2_single_2_element_structure_to_all_lanes")
  {}

 private:
  nacl_arm_dec::VectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes);
};

class NamedVectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes
    : public NamedClassDecoder {
 public:
  NamedVectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes()
    : NamedClassDecoder(decoder_, "VectorLoadSingle3AllLanes VLD3_single_3_element_structure_to_all_lanes")
  {}

 private:
  nacl_arm_dec::VectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes);
};

class NamedVectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes
    : public NamedClassDecoder {
 public:
  NamedVectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes()
    : NamedClassDecoder(decoder_, "VectorLoadSingle4AllLanes VLD4_single_4_element_structure_to_all_lanes")
  {}

 private:
  nacl_arm_dec::VectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes);
};

class NamedVectorLoadStoreMultiple1_VLD1_multiple_single_elements
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreMultiple1_VLD1_multiple_single_elements()
    : NamedClassDecoder(decoder_, "VectorLoadStoreMultiple1 VLD1_multiple_single_elements")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreMultiple1_VLD1_multiple_single_elements decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreMultiple1_VLD1_multiple_single_elements);
};

class NamedVectorLoadStoreMultiple1_VST1_multiple_single_elements
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreMultiple1_VST1_multiple_single_elements()
    : NamedClassDecoder(decoder_, "VectorLoadStoreMultiple1 VST1_multiple_single_elements")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreMultiple1_VST1_multiple_single_elements decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreMultiple1_VST1_multiple_single_elements);
};

class NamedVectorLoadStoreMultiple2_VLD2_multiple_2_element_structures
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreMultiple2_VLD2_multiple_2_element_structures()
    : NamedClassDecoder(decoder_, "VectorLoadStoreMultiple2 VLD2_multiple_2_element_structures")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreMultiple2_VLD2_multiple_2_element_structures);
};

class NamedVectorLoadStoreMultiple2_VST2_multiple_2_element_structures
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreMultiple2_VST2_multiple_2_element_structures()
    : NamedClassDecoder(decoder_, "VectorLoadStoreMultiple2 VST2_multiple_2_element_structures")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreMultiple2_VST2_multiple_2_element_structures decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreMultiple2_VST2_multiple_2_element_structures);
};

class NamedVectorLoadStoreMultiple3_VLD3_multiple_3_element_structures
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreMultiple3_VLD3_multiple_3_element_structures()
    : NamedClassDecoder(decoder_, "VectorLoadStoreMultiple3 VLD3_multiple_3_element_structures")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreMultiple3_VLD3_multiple_3_element_structures decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreMultiple3_VLD3_multiple_3_element_structures);
};

class NamedVectorLoadStoreMultiple3_VST3_multiple_3_element_structures
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreMultiple3_VST3_multiple_3_element_structures()
    : NamedClassDecoder(decoder_, "VectorLoadStoreMultiple3 VST3_multiple_3_element_structures")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreMultiple3_VST3_multiple_3_element_structures decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreMultiple3_VST3_multiple_3_element_structures);
};

class NamedVectorLoadStoreMultiple4_VLD4_multiple_4_element_structures
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreMultiple4_VLD4_multiple_4_element_structures()
    : NamedClassDecoder(decoder_, "VectorLoadStoreMultiple4 VLD4_multiple_4_element_structures")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreMultiple4_VLD4_multiple_4_element_structures decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreMultiple4_VLD4_multiple_4_element_structures);
};

class NamedVectorLoadStoreMultiple4_VST4_multiple_4_element_structures
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreMultiple4_VST4_multiple_4_element_structures()
    : NamedClassDecoder(decoder_, "VectorLoadStoreMultiple4 VST4_multiple_4_element_structures")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreMultiple4_VST4_multiple_4_element_structures decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreMultiple4_VST4_multiple_4_element_structures);
};

class NamedVectorLoadStoreSingle1_VLD1_single_element_to_one_lane
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreSingle1_VLD1_single_element_to_one_lane()
    : NamedClassDecoder(decoder_, "VectorLoadStoreSingle1 VLD1_single_element_to_one_lane")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreSingle1_VLD1_single_element_to_one_lane decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreSingle1_VLD1_single_element_to_one_lane);
};

class NamedVectorLoadStoreSingle1_VST1_single_element_from_one_lane
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreSingle1_VST1_single_element_from_one_lane()
    : NamedClassDecoder(decoder_, "VectorLoadStoreSingle1 VST1_single_element_from_one_lane")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreSingle1_VST1_single_element_from_one_lane decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreSingle1_VST1_single_element_from_one_lane);
};

class NamedVectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane()
    : NamedClassDecoder(decoder_, "VectorLoadStoreSingle2 VLD2_single_2_element_structure_to_one_lane")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane);
};

class NamedVectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane()
    : NamedClassDecoder(decoder_, "VectorLoadStoreSingle2 VST2_single_2_element_structure_from_one_lane")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane);
};

class NamedVectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane()
    : NamedClassDecoder(decoder_, "VectorLoadStoreSingle3 VLD3_single_3_element_structure_to_one_lane")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane);
};

class NamedVectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane()
    : NamedClassDecoder(decoder_, "VectorLoadStoreSingle3 VST3_single_3_element_structure_from_one_lane")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane);
};

class NamedVectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane()
    : NamedClassDecoder(decoder_, "VectorLoadStoreSingle4 VLD4_single_4_element_structure_to_one_lane")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane);
};

class NamedVectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane
    : public NamedClassDecoder {
 public:
  NamedVectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane()
    : NamedClassDecoder(decoder_, "VectorLoadStoreSingle4 VST4_single_4_element_structure_form_one_lane")
  {}

 private:
  nacl_arm_dec::VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane);
};

class NamedVectorUnary2RegisterDup_Vdup_Rule_302_A1_P592
    : public NamedClassDecoder {
 public:
  NamedVectorUnary2RegisterDup_Vdup_Rule_302_A1_P592()
    : NamedClassDecoder(decoder_, "VectorUnary2RegisterDup Vdup_Rule_302_A1_P592")
  {}

 private:
  nacl_arm_dec::VectorUnary2RegisterDup_Vdup_Rule_302_A1_P592 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorUnary2RegisterDup_Vdup_Rule_302_A1_P592);
};

class NamedVfpMrsOp_Vmrs_Rule_335_A1_P658
    : public NamedClassDecoder {
 public:
  NamedVfpMrsOp_Vmrs_Rule_335_A1_P658()
    : NamedClassDecoder(decoder_, "VfpMrsOp Vmrs_Rule_335_A1_P658")
  {}

 private:
  nacl_arm_dec::VfpMrsOp_Vmrs_Rule_335_A1_P658 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpMrsOp_Vmrs_Rule_335_A1_P658);
};

class NamedVfpUsesRegOp_Vmsr_Rule_336_A1_P660
    : public NamedClassDecoder {
 public:
  NamedVfpUsesRegOp_Vmsr_Rule_336_A1_P660()
    : NamedClassDecoder(decoder_, "VfpUsesRegOp Vmsr_Rule_336_A1_P660")
  {}

 private:
  nacl_arm_dec::VfpUsesRegOp_Vmsr_Rule_336_A1_P660 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpUsesRegOp_Vmsr_Rule_336_A1_P660);
};

class NamedBranch_B_Rule_16_A1_P44
    : public NamedClassDecoder {
 public:
  NamedBranch_B_Rule_16_A1_P44()
    : NamedClassDecoder(decoder_, "Branch B_Rule_16_A1_P44")
  {}

 private:
  nacl_arm_dec::Branch_B_Rule_16_A1_P44 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranch_B_Rule_16_A1_P44);
};

class NamedBranch_Bl_Blx_Rule_23_A1_P58
    : public NamedClassDecoder {
 public:
  NamedBranch_Bl_Blx_Rule_23_A1_P58()
    : NamedClassDecoder(decoder_, "Branch Bl_Blx_Rule_23_A1_P58")
  {}

 private:
  nacl_arm_dec::Branch_Bl_Blx_Rule_23_A1_P58 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranch_Bl_Blx_Rule_23_A1_P58);
};

class NamedBreakpoint_Bkpt_Rule_22_A1_P56
    : public NamedClassDecoder {
 public:
  NamedBreakpoint_Bkpt_Rule_22_A1_P56()
    : NamedClassDecoder(decoder_, "Breakpoint Bkpt_Rule_22_A1_P56")
  {}

 private:
  nacl_arm_dec::Breakpoint_Bkpt_Rule_22_A1_P56 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBreakpoint_Bkpt_Rule_22_A1_P56);
};

class NamedBxBlx_Blx_Rule_24_A1_P60
    : public NamedClassDecoder {
 public:
  NamedBxBlx_Blx_Rule_24_A1_P60()
    : NamedClassDecoder(decoder_, "BxBlx Blx_Rule_24_A1_P60")
  {}

 private:
  nacl_arm_dec::BxBlx_Blx_Rule_24_A1_P60 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBxBlx_Blx_Rule_24_A1_P60);
};

class NamedBxBlx_Bx_Rule_25_A1_P62
    : public NamedClassDecoder {
 public:
  NamedBxBlx_Bx_Rule_25_A1_P62()
    : NamedClassDecoder(decoder_, "BxBlx Bx_Rule_25_A1_P62")
  {}

 private:
  nacl_arm_dec::BxBlx_Bx_Rule_25_A1_P62 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBxBlx_Bx_Rule_25_A1_P62);
};

class NamedDefs12To15CondsDontCareMsbGeLsb_Bfi_Rule_18_A1_P48
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareMsbGeLsb_Bfi_Rule_18_A1_P48()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareMsbGeLsb Bfi_Rule_18_A1_P48")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareMsbGeLsb_Bfi_Rule_18_A1_P48 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareMsbGeLsb_Bfi_Rule_18_A1_P48);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Rbit_Rule_134_A1_P270")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Rev16_Rule_136_A1_P274")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Rev_Rule_135_A1_P272")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Revsh_Rule_137_A1_P276")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Ssat16_Rule_184_A1_P364")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Ssat_Rule_183_A1_P362")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Sxtb16_Rule_224_A1_P442")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Sxtb_Rule_223_A1_P440")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Sxth_Rule_225_A1_P444")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Usat16_Rule_256_A1_P506")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Usat_Rule_255_A1_P504")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Uxtb16_Rule_264_A1_P522")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Uxtb_Rule_263_A1_P520")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520);
};

class NamedDefs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPc Uxth_Rule_265_A1_P524")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524);
};

class NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Sbfx_Rule_154_A1_P308
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Sbfx_Rule_154_A1_P308()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPcBitfieldExtract Sbfx_Rule_154_A1_P308")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPcBitfieldExtract_Sbfx_Rule_154_A1_P308 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Sbfx_Rule_154_A1_P308);
};

class NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Ubfx_Rule_236_A1_P466
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Ubfx_Rule_236_A1_P466()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRdRnNotPcBitfieldExtract Ubfx_Rule_236_A1_P466")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRdRnNotPcBitfieldExtract_Ubfx_Rule_236_A1_P466 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Ubfx_Rule_236_A1_P466);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Pkh_Rule_116_A1_P234")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qadd16_Rule_125_A1_P252")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qadd8_Rule_126_A1_P254")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qasx_Rule_127_A1_P256")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qsax_Rule_130_A1_P262")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qsub16_Rule_132_A1_P266")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Qsub8_Rule_133_A1_P268")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sadd16_Rule_148_A1_P296")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd8_Rule_149_A1_P298
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd8_Rule_149_A1_P298()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sadd8_Rule_149_A1_P298")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sadd8_Rule_149_A1_P298 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd8_Rule_149_A1_P298);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sasx_Rule_150_A1_P300")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sel_Rule_156_A1_P312")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shadd16_Rule_159_A1_P318")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shadd8_Rule_160_A1_P320")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shasx_Rule_161_A1_P322")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shsax_Rule_162_A1_P324")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shsub16_Rule_163_A1_P326")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Shsub8_Rule_164_A1_P328")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Ssax_Rule_185_A1_P366")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Ssub16_Rule_186_A1_P368")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Ssub8_Rule_187_A1_P370")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab16_Rule_221_A1_P436
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab16_Rule_221_A1_P436()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sxtab16_Rule_221_A1_P436")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sxtab16_Rule_221_A1_P436 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab16_Rule_221_A1_P436);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab_Rule_220_A1_P434
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab_Rule_220_A1_P434()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sxtab_Rule_220_A1_P434")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sxtab_Rule_220_A1_P434 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab_Rule_220_A1_P434);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtah_Rule_222_A1_P438
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtah_Rule_222_A1_P438()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Sxtah_Rule_222_A1_P438")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Sxtah_Rule_222_A1_P438 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtah_Rule_222_A1_P438);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uadd16_Rule_233_A1_P460")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uadd8_Rule_234_A1_P462")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uasx_Rule_235_A1_P464")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhadd16_Rule_238_A1_P470")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhadd8_Rule_239_A1_P472")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhasx_Rule_240_A1_P474")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhsax_Rule_241_A1_P476")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhsub16_Rule_242_A1_P478")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uhsub8_Rule_243_A1_P480")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqadd16_Rule_247_A1_P488")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqadd8_Rule_248_A1_P490")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqasx_Rule_249_A1_P492")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqsax_Rule_250_A1_P494")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqsub16_Rule_251_A1_P496")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uqsub8_Rule_252_A1_P498")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Usax_Rule_257_A1_P508")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Usub16_Rule_258_A1_P510")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Usub8_Rule_259_A1_P512")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab16_Rule_262_A1_P516
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab16_Rule_262_A1_P516()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uxtab16_Rule_262_A1_P516")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uxtab16_Rule_262_A1_P516 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab16_Rule_262_A1_P516);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab_Rule_260_A1_P514
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab_Rule_260_A1_P514()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uxtab_Rule_260_A1_P514")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uxtab_Rule_260_A1_P514 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab_Rule_260_A1_P514);
};

class NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtah_Rule_262_A1_P518
    : public NamedClassDecoder {
 public:
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtah_Rule_262_A1_P518()
    : NamedClassDecoder(decoder_, "Defs12To15CondsDontCareRnRdRmNotPc Uxtah_Rule_262_A1_P518")
  {}

 private:
  nacl_arm_dec::Defs12To15CondsDontCareRnRdRmNotPc_Uxtah_Rule_262_A1_P518 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtah_Rule_262_A1_P518);
};

class NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72
    : public NamedClassDecoder {
 public:
  NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72()
    : NamedClassDecoder(decoder_, "Defs12To15RdRnNotPc Clz_Rule_31_A1_P72")
  {}

 private:
  nacl_arm_dec::Defs12To15RdRnNotPc_Clz_Rule_31_A1_P72 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336
    : public NamedClassDecoder {
 public:
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Smlald_Rule_170_P336")
  {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336
    : public NamedClassDecoder {
 public:
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Smlalxx_Rule_169_A1_P336")
  {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336);
};

class NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344
    : public NamedClassDecoder {
 public:
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344()
    : NamedClassDecoder(decoder_, "Defs12To19CondsDontCareRdRmRnNotPc Smlsld_Rule_173_P344")
  {}

 private:
  nacl_arm_dec::Defs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smlad_Rule_167_P332")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smlawx_Rule_171_A1_340")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smlaxx_Rule_166_A1_P330")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smlsd_Rule_172_P342")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smmla_Rule_174_P346")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Smmls_Rule_175_P348")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348);
};

class NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRaRmRnNotPc Usada8_Rule_254_A1_P502")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Sdiv_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Sdiv_Rule_A1()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Sdiv_Rule_A1")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Sdiv_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Sdiv_Rule_A1);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smmul_Rule_176_P350")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smuad_Rule_177_P352")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smulwx_Rule_180_A1_P358")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smulxx_Rule_178_P354")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Smusd_Rule_181_P360")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Udiv_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Udiv_Rule_A1()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Udiv_Rule_A1")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Udiv_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Udiv_Rule_A1);
};

class NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500
    : public NamedClassDecoder {
 public:
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500()
    : NamedClassDecoder(decoder_, "Defs16To19CondsDontCareRdRmRnNotPc Usad8_Rule_253_A1_P500")
  {}

 private:
  nacl_arm_dec::Defs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500);
};

class NamedDontCareInst_Msr_Rule_103_A1_P208
    : public NamedClassDecoder {
 public:
  NamedDontCareInst_Msr_Rule_103_A1_P208()
    : NamedClassDecoder(decoder_, "DontCareInst Msr_Rule_103_A1_P208")
  {}

 private:
  nacl_arm_dec::DontCareInst_Msr_Rule_103_A1_P208 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Msr_Rule_103_A1_P208);
};

class NamedDontCareInst_Nop_Rule_110_A1_P222
    : public NamedClassDecoder {
 public:
  NamedDontCareInst_Nop_Rule_110_A1_P222()
    : NamedClassDecoder(decoder_, "DontCareInst Nop_Rule_110_A1_P222")
  {}

 private:
  nacl_arm_dec::DontCareInst_Nop_Rule_110_A1_P222 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Nop_Rule_110_A1_P222);
};

class NamedDontCareInst_Yield_Rule_413_A1_P812
    : public NamedClassDecoder {
 public:
  NamedDontCareInst_Yield_Rule_413_A1_P812()
    : NamedClassDecoder(decoder_, "DontCareInst Yield_Rule_413_A1_P812")
  {}

 private:
  nacl_arm_dec::DontCareInst_Yield_Rule_413_A1_P812 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInst_Yield_Rule_413_A1_P812);
};

class NamedDontCareInstRdNotPc_Vmsr_Rule_336_A1_P660
    : public NamedClassDecoder {
 public:
  NamedDontCareInstRdNotPc_Vmsr_Rule_336_A1_P660()
    : NamedClassDecoder(decoder_, "DontCareInstRdNotPc Vmsr_Rule_336_A1_P660")
  {}

 private:
  nacl_arm_dec::DontCareInstRdNotPc_Vmsr_Rule_336_A1_P660 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDontCareInstRdNotPc_Vmsr_Rule_336_A1_P660);
};

class NamedForbidden_Blx_Rule_23_A2_P58
    : public NamedClassDecoder {
 public:
  NamedForbidden_Blx_Rule_23_A2_P58()
    : NamedClassDecoder(decoder_, "Forbidden Blx_Rule_23_A2_P58")
  {}

 private:
  nacl_arm_dec::Forbidden_Blx_Rule_23_A2_P58 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Blx_Rule_23_A2_P58);
};

class NamedForbidden_Bxj_Rule_26_A1_P64
    : public NamedClassDecoder {
 public:
  NamedForbidden_Bxj_Rule_26_A1_P64()
    : NamedClassDecoder(decoder_, "Forbidden Bxj_Rule_26_A1_P64")
  {}

 private:
  nacl_arm_dec::Forbidden_Bxj_Rule_26_A1_P64 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Bxj_Rule_26_A1_P64);
};

class NamedForbidden_Cdp2_Rule_28_A2_P68
    : public NamedClassDecoder {
 public:
  NamedForbidden_Cdp2_Rule_28_A2_P68()
    : NamedClassDecoder(decoder_, "Forbidden Cdp2_Rule_28_A2_P68")
  {}

 private:
  nacl_arm_dec::Forbidden_Cdp2_Rule_28_A2_P68 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Cdp2_Rule_28_A2_P68);
};

class NamedForbidden_Cdp_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Cdp_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Cdp_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Cdp_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Cdp_Rule_A1);
};

class NamedForbidden_Clrex_Rule_30_A1_P70
    : public NamedClassDecoder {
 public:
  NamedForbidden_Clrex_Rule_30_A1_P70()
    : NamedClassDecoder(decoder_, "Forbidden Clrex_Rule_30_A1_P70")
  {}

 private:
  nacl_arm_dec::Forbidden_Clrex_Rule_30_A1_P70 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Clrex_Rule_30_A1_P70);
};

class NamedForbidden_Cps_Rule_b6_1_1_A1_B6_3
    : public NamedClassDecoder {
 public:
  NamedForbidden_Cps_Rule_b6_1_1_A1_B6_3()
    : NamedClassDecoder(decoder_, "Forbidden Cps_Rule_b6_1_1_A1_B6_3")
  {}

 private:
  nacl_arm_dec::Forbidden_Cps_Rule_b6_1_1_A1_B6_3 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Cps_Rule_b6_1_1_A1_B6_3);
};

class NamedForbidden_Dbg_Rule_40_A1_P88
    : public NamedClassDecoder {
 public:
  NamedForbidden_Dbg_Rule_40_A1_P88()
    : NamedClassDecoder(decoder_, "Forbidden Dbg_Rule_40_A1_P88")
  {}

 private:
  nacl_arm_dec::Forbidden_Dbg_Rule_40_A1_P88 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Dbg_Rule_40_A1_P88);
};

class NamedForbidden_Eret_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Eret_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Eret_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Eret_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Eret_Rule_A1);
};

class NamedForbidden_Hvc_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Hvc_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Hvc_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Hvc_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Hvc_Rule_A1);
};

class NamedForbidden_Ldc2_Rule_51_A2_P106
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldc2_Rule_51_A2_P106()
    : NamedClassDecoder(decoder_, "Forbidden Ldc2_Rule_51_A2_P106")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldc2_Rule_51_A2_P106 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldc2_Rule_51_A2_P106);
};

class NamedForbidden_Ldc2_Rule_52_A2_P108
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldc2_Rule_52_A2_P108()
    : NamedClassDecoder(decoder_, "Forbidden Ldc2_Rule_52_A2_P108")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldc2_Rule_52_A2_P108 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldc2_Rule_52_A2_P108);
};

class NamedForbidden_Ldc_immediate_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldc_immediate_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Ldc_immediate_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldc_immediate_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldc_immediate_Rule_A1);
};

class NamedForbidden_Ldc_literal_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldc_literal_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Ldc_literal_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldc_literal_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldc_literal_Rule_A1);
};

class NamedForbidden_Ldm_Rule_2_B6_A1_P5
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldm_Rule_2_B6_A1_P5()
    : NamedClassDecoder(decoder_, "Forbidden Ldm_Rule_2_B6_A1_P5")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldm_Rule_2_B6_A1_P5 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldm_Rule_2_B6_A1_P5);
};

class NamedForbidden_Ldm_Rule_3_B6_A1_P7
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldm_Rule_3_B6_A1_P7()
    : NamedClassDecoder(decoder_, "Forbidden Ldm_Rule_3_B6_A1_P7")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldm_Rule_3_B6_A1_P7 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldm_Rule_3_B6_A1_P7);
};

class NamedForbidden_Ldrt_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldrt_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Ldrt_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldrt_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldrt_Rule_A1);
};

class NamedForbidden_Ldrt_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldrt_Rule_A2()
    : NamedClassDecoder(decoder_, "Forbidden Ldrt_Rule_A2")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldrt_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldrt_Rule_A2);
};

class NamedForbidden_Ldrtb_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldrtb_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Ldrtb_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldrtb_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldrtb_Rule_A1);
};

class NamedForbidden_Ldrtb_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbidden_Ldrtb_Rule_A2()
    : NamedClassDecoder(decoder_, "Forbidden Ldrtb_Rule_A2")
  {}

 private:
  nacl_arm_dec::Forbidden_Ldrtb_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Ldrtb_Rule_A2);
};

class NamedForbidden_Mcr2_Rule_92_A2_P186
    : public NamedClassDecoder {
 public:
  NamedForbidden_Mcr2_Rule_92_A2_P186()
    : NamedClassDecoder(decoder_, "Forbidden Mcr2_Rule_92_A2_P186")
  {}

 private:
  nacl_arm_dec::Forbidden_Mcr2_Rule_92_A2_P186 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Mcr2_Rule_92_A2_P186);
};

class NamedForbidden_Mcr_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Mcr_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Mcr_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Mcr_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Mcr_Rule_A1);
};

class NamedForbidden_Mcrr2_Rule_93_A2_P188
    : public NamedClassDecoder {
 public:
  NamedForbidden_Mcrr2_Rule_93_A2_P188()
    : NamedClassDecoder(decoder_, "Forbidden Mcrr2_Rule_93_A2_P188")
  {}

 private:
  nacl_arm_dec::Forbidden_Mcrr2_Rule_93_A2_P188 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Mcrr2_Rule_93_A2_P188);
};

class NamedForbidden_Mcrr_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Mcrr_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Mcrr_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Mcrr_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Mcrr_Rule_A1);
};

class NamedForbidden_Mrc2_Rule_100_A2_P202
    : public NamedClassDecoder {
 public:
  NamedForbidden_Mrc2_Rule_100_A2_P202()
    : NamedClassDecoder(decoder_, "Forbidden Mrc2_Rule_100_A2_P202")
  {}

 private:
  nacl_arm_dec::Forbidden_Mrc2_Rule_100_A2_P202 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Mrc2_Rule_100_A2_P202);
};

class NamedForbidden_Mrc_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Mrc_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Mrc_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Mrc_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Mrc_Rule_A1);
};

class NamedForbidden_Mrrc2_Rule_101_A2_P204
    : public NamedClassDecoder {
 public:
  NamedForbidden_Mrrc2_Rule_101_A2_P204()
    : NamedClassDecoder(decoder_, "Forbidden Mrrc2_Rule_101_A2_P204")
  {}

 private:
  nacl_arm_dec::Forbidden_Mrrc2_Rule_101_A2_P204 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Mrrc2_Rule_101_A2_P204);
};

class NamedForbidden_Mrrc_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Mrrc_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Mrrc_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Mrrc_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Mrrc_Rule_A1);
};

class NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12
    : public NamedClassDecoder {
 public:
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12()
    : NamedClassDecoder(decoder_, "Forbidden Msr_Rule_B6_1_6_A1_PB6_12")
  {}

 private:
  nacl_arm_dec::Forbidden_Msr_Rule_B6_1_6_A1_PB6_12 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12);
};

class NamedForbidden_Msr_Rule_B6_1_7_P14
    : public NamedClassDecoder {
 public:
  NamedForbidden_Msr_Rule_B6_1_7_P14()
    : NamedClassDecoder(decoder_, "Forbidden Msr_Rule_B6_1_7_P14")
  {}

 private:
  nacl_arm_dec::Forbidden_Msr_Rule_B6_1_7_P14 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Msr_Rule_B6_1_7_P14);
};

class NamedForbidden_Msr_Rule_Banked_register_A1_B9_1990
    : public NamedClassDecoder {
 public:
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1990()
    : NamedClassDecoder(decoder_, "Forbidden Msr_Rule_Banked_register_A1_B9_1990")
  {}

 private:
  nacl_arm_dec::Forbidden_Msr_Rule_Banked_register_A1_B9_1990 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Msr_Rule_Banked_register_A1_B9_1990);
};

class NamedForbidden_Msr_Rule_Banked_register_A1_B9_1992
    : public NamedClassDecoder {
 public:
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1992()
    : NamedClassDecoder(decoder_, "Forbidden Msr_Rule_Banked_register_A1_B9_1992")
  {}

 private:
  nacl_arm_dec::Forbidden_Msr_Rule_Banked_register_A1_B9_1992 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Msr_Rule_Banked_register_A1_B9_1992);
};

class NamedForbidden_Rfe_Rule_B6_1_10_A1_B6_16
    : public NamedClassDecoder {
 public:
  NamedForbidden_Rfe_Rule_B6_1_10_A1_B6_16()
    : NamedClassDecoder(decoder_, "Forbidden Rfe_Rule_B6_1_10_A1_B6_16")
  {}

 private:
  nacl_arm_dec::Forbidden_Rfe_Rule_B6_1_10_A1_B6_16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Rfe_Rule_B6_1_10_A1_B6_16);
};

class NamedForbidden_Setend_Rule_157_P314
    : public NamedClassDecoder {
 public:
  NamedForbidden_Setend_Rule_157_P314()
    : NamedClassDecoder(decoder_, "Forbidden Setend_Rule_157_P314")
  {}

 private:
  nacl_arm_dec::Forbidden_Setend_Rule_157_P314 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Setend_Rule_157_P314);
};

class NamedForbidden_Sev_Rule_158_A1_P316
    : public NamedClassDecoder {
 public:
  NamedForbidden_Sev_Rule_158_A1_P316()
    : NamedClassDecoder(decoder_, "Forbidden Sev_Rule_158_A1_P316")
  {}

 private:
  nacl_arm_dec::Forbidden_Sev_Rule_158_A1_P316 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Sev_Rule_158_A1_P316);
};

class NamedForbidden_Smc_Rule_B6_1_9_P18
    : public NamedClassDecoder {
 public:
  NamedForbidden_Smc_Rule_B6_1_9_P18()
    : NamedClassDecoder(decoder_, "Forbidden Smc_Rule_B6_1_9_P18")
  {}

 private:
  nacl_arm_dec::Forbidden_Smc_Rule_B6_1_9_P18 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Smc_Rule_B6_1_9_P18);
};

class NamedForbidden_Srs_Rule_B6_1_10_A1_B6_20
    : public NamedClassDecoder {
 public:
  NamedForbidden_Srs_Rule_B6_1_10_A1_B6_20()
    : NamedClassDecoder(decoder_, "Forbidden Srs_Rule_B6_1_10_A1_B6_20")
  {}

 private:
  nacl_arm_dec::Forbidden_Srs_Rule_B6_1_10_A1_B6_20 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Srs_Rule_B6_1_10_A1_B6_20);
};

class NamedForbidden_Stc2_Rule_188_A2_P372
    : public NamedClassDecoder {
 public:
  NamedForbidden_Stc2_Rule_188_A2_P372()
    : NamedClassDecoder(decoder_, "Forbidden Stc2_Rule_188_A2_P372")
  {}

 private:
  nacl_arm_dec::Forbidden_Stc2_Rule_188_A2_P372 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Stc2_Rule_188_A2_P372);
};

class NamedForbidden_Stc_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbidden_Stc_Rule_A2()
    : NamedClassDecoder(decoder_, "Forbidden Stc_Rule_A2")
  {}

 private:
  nacl_arm_dec::Forbidden_Stc_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Stc_Rule_A2);
};

class NamedForbidden_Stm_Rule_11_B6_A1_P22
    : public NamedClassDecoder {
 public:
  NamedForbidden_Stm_Rule_11_B6_A1_P22()
    : NamedClassDecoder(decoder_, "Forbidden Stm_Rule_11_B6_A1_P22")
  {}

 private:
  nacl_arm_dec::Forbidden_Stm_Rule_11_B6_A1_P22 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Stm_Rule_11_B6_A1_P22);
};

class NamedForbidden_Strt_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Strt_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Strt_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Strt_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Strt_Rule_A1);
};

class NamedForbidden_Strt_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbidden_Strt_Rule_A2()
    : NamedClassDecoder(decoder_, "Forbidden Strt_Rule_A2")
  {}

 private:
  nacl_arm_dec::Forbidden_Strt_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Strt_Rule_A2);
};

class NamedForbidden_Strtb_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Strtb_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Strtb_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Strtb_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Strtb_Rule_A1);
};

class NamedForbidden_Strtb_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedForbidden_Strtb_Rule_A2()
    : NamedClassDecoder(decoder_, "Forbidden Strtb_Rule_A2")
  {}

 private:
  nacl_arm_dec::Forbidden_Strtb_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Strtb_Rule_A2);
};

class NamedForbidden_Svc_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedForbidden_Svc_Rule_A1()
    : NamedClassDecoder(decoder_, "Forbidden Svc_Rule_A1")
  {}

 private:
  nacl_arm_dec::Forbidden_Svc_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Svc_Rule_A1);
};

class NamedForbidden_Unallocated_hints
    : public NamedClassDecoder {
 public:
  NamedForbidden_Unallocated_hints()
    : NamedClassDecoder(decoder_, "Forbidden Unallocated_hints")
  {}

 private:
  nacl_arm_dec::Forbidden_Unallocated_hints decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Unallocated_hints);
};

class NamedForbidden_Wfe_Rule_411_A1_P808
    : public NamedClassDecoder {
 public:
  NamedForbidden_Wfe_Rule_411_A1_P808()
    : NamedClassDecoder(decoder_, "Forbidden Wfe_Rule_411_A1_P808")
  {}

 private:
  nacl_arm_dec::Forbidden_Wfe_Rule_411_A1_P808 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Wfe_Rule_411_A1_P808);
};

class NamedForbidden_Wfi_Rule_412_A1_P810
    : public NamedClassDecoder {
 public:
  NamedForbidden_Wfi_Rule_412_A1_P810()
    : NamedClassDecoder(decoder_, "Forbidden Wfi_Rule_412_A1_P810")
  {}

 private:
  nacl_arm_dec::Forbidden_Wfi_Rule_412_A1_P810 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_Wfi_Rule_412_A1_P810);
};

class NamedForbidden_extra_load_store_instructions_unpriviledged
    : public NamedClassDecoder {
 public:
  NamedForbidden_extra_load_store_instructions_unpriviledged()
    : NamedClassDecoder(decoder_, "Forbidden extra_load_store_instructions_unpriviledged")
  {}

 private:
  nacl_arm_dec::Forbidden_extra_load_store_instructions_unpriviledged decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_extra_load_store_instructions_unpriviledged);
};

class NamedLoadBasedImmedMemory_Ldr_Rule_59_A1_P122
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_Ldr_Rule_59_A1_P122()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory Ldr_Rule_59_A1_P122")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_Ldr_Rule_59_A1_P122 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_Ldr_Rule_59_A1_P122);
};

class NamedLoadBasedImmedMemory_Ldrb_Rule_62_A1_P128
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_Ldrb_Rule_62_A1_P128()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory Ldrb_Rule_62_A1_P128")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_Ldrb_Rule_62_A1_P128 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_Ldrb_Rule_62_A1_P128);
};

class NamedLoadBasedImmedMemory_Ldrb_Rule_63_A1_P130
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_Ldrb_Rule_63_A1_P130()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory Ldrb_Rule_63_A1_P130")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_Ldrb_Rule_63_A1_P130 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_Ldrb_Rule_63_A1_P130);
};

class NamedLoadBasedImmedMemory_Ldrh_Rule_74_A1_P152
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_Ldrh_Rule_74_A1_P152()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory Ldrh_Rule_74_A1_P152")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_Ldrh_Rule_74_A1_P152 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_Ldrh_Rule_74_A1_P152);
};

class NamedLoadBasedImmedMemory_Ldrh_Rule_75_A1_P154
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_Ldrh_Rule_75_A1_P154()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory Ldrh_Rule_75_A1_P154")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_Ldrh_Rule_75_A1_P154 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_Ldrh_Rule_75_A1_P154);
};

class NamedLoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory Ldrsb_Rule_78_A1_P160")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160);
};

class NamedLoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory Ldrsh_Rule_82_A1_P168")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168);
};

class NamedLoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory Ldrsh_Rule_83_A1_P170")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170);
};

class NamedLoadBasedImmedMemory_ldrsb_Rule_79_A1_162
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemory_ldrsb_Rule_79_A1_162()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemory ldrsb_Rule_79_A1_162")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemory_ldrsb_Rule_79_A1_162 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemory_ldrsb_Rule_79_A1_162);
};

class NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemoryDouble Ldrd_Rule_66_A1_P136")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136);
};

class NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138
    : public NamedClassDecoder {
 public:
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138()
    : NamedClassDecoder(decoder_, "LoadBasedImmedMemoryDouble Ldrd_Rule_67_A1_P138")
  {}

 private:
  nacl_arm_dec::LoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138);
};

class NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142
    : public NamedClassDecoder {
 public:
  NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142()
    : NamedClassDecoder(decoder_, "LoadBasedMemory Ldrex_Rule_69_A1_P142")
  {}

 private:
  nacl_arm_dec::LoadBasedMemory_Ldrex_Rule_69_A1_P142 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142);
};

class NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144
    : public NamedClassDecoder {
 public:
  NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144()
    : NamedClassDecoder(decoder_, "LoadBasedMemory Ldrexb_Rule_70_A1_P144")
  {}

 private:
  nacl_arm_dec::LoadBasedMemory_Ldrexb_Rule_70_A1_P144 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144);
};

class NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148
    : public NamedClassDecoder {
 public:
  NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148()
    : NamedClassDecoder(decoder_, "LoadBasedMemory Ldrexh_Rule_72_A1_P148")
  {}

 private:
  nacl_arm_dec::LoadBasedMemory_Ldrexh_Rule_72_A1_P148 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148);
};

class NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146
    : public NamedClassDecoder {
 public:
  NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146()
    : NamedClassDecoder(decoder_, "LoadBasedMemoryDouble Ldrexd_Rule_71_A1_P146")
  {}

 private:
  nacl_arm_dec::LoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146);
};

class NamedLoadBasedOffsetMemory_Ldr_Rule_60_A1_P124
    : public NamedClassDecoder {
 public:
  NamedLoadBasedOffsetMemory_Ldr_Rule_60_A1_P124()
    : NamedClassDecoder(decoder_, "LoadBasedOffsetMemory Ldr_Rule_60_A1_P124")
  {}

 private:
  nacl_arm_dec::LoadBasedOffsetMemory_Ldr_Rule_60_A1_P124 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedOffsetMemory_Ldr_Rule_60_A1_P124);
};

class NamedLoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132
    : public NamedClassDecoder {
 public:
  NamedLoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132()
    : NamedClassDecoder(decoder_, "LoadBasedOffsetMemory Ldrb_Rule_64_A1_P132")
  {}

 private:
  nacl_arm_dec::LoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132);
};

class NamedLoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156
    : public NamedClassDecoder {
 public:
  NamedLoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156()
    : NamedClassDecoder(decoder_, "LoadBasedOffsetMemory Ldrh_Rule_76_A1_P156")
  {}

 private:
  nacl_arm_dec::LoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156);
};

class NamedLoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164
    : public NamedClassDecoder {
 public:
  NamedLoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164()
    : NamedClassDecoder(decoder_, "LoadBasedOffsetMemory Ldrsb_Rule_80_A1_P164")
  {}

 private:
  nacl_arm_dec::LoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164);
};

class NamedLoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172
    : public NamedClassDecoder {
 public:
  NamedLoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172()
    : NamedClassDecoder(decoder_, "LoadBasedOffsetMemory Ldrsh_Rule_84_A1_P172")
  {}

 private:
  nacl_arm_dec::LoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172);
};

class NamedLoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140
    : public NamedClassDecoder {
 public:
  NamedLoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140()
    : NamedClassDecoder(decoder_, "LoadBasedOffsetMemoryDouble Ldrd_Rule_68_A1_P140")
  {}

 private:
  nacl_arm_dec::LoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140);
};

class NamedLoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110
    : public NamedClassDecoder {
 public:
  NamedLoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110()
    : NamedClassDecoder(decoder_, "LoadMultiple Ldm_Ldmia_Ldmfd_Rule_53_A1_P110")
  {}

 private:
  nacl_arm_dec::LoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110);
};

class NamedLoadMultiple_Ldmda_Ldmfa_Rule_54_A1_P112
    : public NamedClassDecoder {
 public:
  NamedLoadMultiple_Ldmda_Ldmfa_Rule_54_A1_P112()
    : NamedClassDecoder(decoder_, "LoadMultiple Ldmda_Ldmfa_Rule_54_A1_P112")
  {}

 private:
  nacl_arm_dec::LoadMultiple_Ldmda_Ldmfa_Rule_54_A1_P112 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadMultiple_Ldmda_Ldmfa_Rule_54_A1_P112);
};

class NamedLoadMultiple_Ldmdb_Ldmea_Rule_55_A1_P114
    : public NamedClassDecoder {
 public:
  NamedLoadMultiple_Ldmdb_Ldmea_Rule_55_A1_P114()
    : NamedClassDecoder(decoder_, "LoadMultiple Ldmdb_Ldmea_Rule_55_A1_P114")
  {}

 private:
  nacl_arm_dec::LoadMultiple_Ldmdb_Ldmea_Rule_55_A1_P114 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadMultiple_Ldmdb_Ldmea_Rule_55_A1_P114);
};

class NamedLoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116
    : public NamedClassDecoder {
 public:
  NamedLoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116()
    : NamedClassDecoder(decoder_, "LoadMultiple Ldmib_Ldmed_Rule_56_A1_P116")
  {}

 private:
  nacl_arm_dec::LoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116);
};

class NamedLoadMultiple_Pop_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedLoadMultiple_Pop_Rule_A1()
    : NamedClassDecoder(decoder_, "LoadMultiple Pop_Rule_A1")
  {}

 private:
  nacl_arm_dec::LoadMultiple_Pop_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadMultiple_Pop_Rule_A1);
};

class NamedStoreBasedImmedMemory_Str_Rule_194_A1_P384
    : public NamedClassDecoder {
 public:
  NamedStoreBasedImmedMemory_Str_Rule_194_A1_P384()
    : NamedClassDecoder(decoder_, "StoreBasedImmedMemory Str_Rule_194_A1_P384")
  {}

 private:
  nacl_arm_dec::StoreBasedImmedMemory_Str_Rule_194_A1_P384 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedImmedMemory_Str_Rule_194_A1_P384);
};

class NamedStoreBasedImmedMemory_Strb_Rule_197_A1_P390
    : public NamedClassDecoder {
 public:
  NamedStoreBasedImmedMemory_Strb_Rule_197_A1_P390()
    : NamedClassDecoder(decoder_, "StoreBasedImmedMemory Strb_Rule_197_A1_P390")
  {}

 private:
  nacl_arm_dec::StoreBasedImmedMemory_Strb_Rule_197_A1_P390 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedImmedMemory_Strb_Rule_197_A1_P390);
};

class NamedStoreBasedImmedMemory_Strh_Rule_207_A1_P410
    : public NamedClassDecoder {
 public:
  NamedStoreBasedImmedMemory_Strh_Rule_207_A1_P410()
    : NamedClassDecoder(decoder_, "StoreBasedImmedMemory Strh_Rule_207_A1_P410")
  {}

 private:
  nacl_arm_dec::StoreBasedImmedMemory_Strh_Rule_207_A1_P410 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedImmedMemory_Strh_Rule_207_A1_P410);
};

class NamedStoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396
    : public NamedClassDecoder {
 public:
  NamedStoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396()
    : NamedClassDecoder(decoder_, "StoreBasedImmedMemoryDouble Strd_Rule_200_A1_P396")
  {}

 private:
  nacl_arm_dec::StoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396);
};

class NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404
    : public NamedClassDecoder {
 public:
  NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404()
    : NamedClassDecoder(decoder_, "StoreBasedMemoryDoubleRtBits0To3 Strexd_Rule_204_A1_P404")
  {}

 private:
  nacl_arm_dec::StoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404);
};

class NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400
    : public NamedClassDecoder {
 public:
  NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400()
    : NamedClassDecoder(decoder_, "StoreBasedMemoryRtBits0To3 Strex_Rule_202_A1_P400")
  {}

 private:
  nacl_arm_dec::StoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400);
};

class NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402
    : public NamedClassDecoder {
 public:
  NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402()
    : NamedClassDecoder(decoder_, "StoreBasedMemoryRtBits0To3 Strexb_Rule_203_A1_P402")
  {}

 private:
  nacl_arm_dec::StoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402);
};

class NamedStoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406
    : public NamedClassDecoder {
 public:
  NamedStoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406()
    : NamedClassDecoder(decoder_, "StoreBasedMemoryRtBits0To3 Strexh_Rule_205_A1_P406")
  {}

 private:
  nacl_arm_dec::StoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406);
};

class NamedStoreBasedOffsetMemory_Str_Rule_195_A1_P386
    : public NamedClassDecoder {
 public:
  NamedStoreBasedOffsetMemory_Str_Rule_195_A1_P386()
    : NamedClassDecoder(decoder_, "StoreBasedOffsetMemory Str_Rule_195_A1_P386")
  {}

 private:
  nacl_arm_dec::StoreBasedOffsetMemory_Str_Rule_195_A1_P386 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedOffsetMemory_Str_Rule_195_A1_P386);
};

class NamedStoreBasedOffsetMemory_Strb_Rule_198_A1_P392
    : public NamedClassDecoder {
 public:
  NamedStoreBasedOffsetMemory_Strb_Rule_198_A1_P392()
    : NamedClassDecoder(decoder_, "StoreBasedOffsetMemory Strb_Rule_198_A1_P392")
  {}

 private:
  nacl_arm_dec::StoreBasedOffsetMemory_Strb_Rule_198_A1_P392 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedOffsetMemory_Strb_Rule_198_A1_P392);
};

class NamedStoreBasedOffsetMemory_Strh_Rule_208_A1_P412
    : public NamedClassDecoder {
 public:
  NamedStoreBasedOffsetMemory_Strh_Rule_208_A1_P412()
    : NamedClassDecoder(decoder_, "StoreBasedOffsetMemory Strh_Rule_208_A1_P412")
  {}

 private:
  nacl_arm_dec::StoreBasedOffsetMemory_Strh_Rule_208_A1_P412 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedOffsetMemory_Strh_Rule_208_A1_P412);
};

class NamedStoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398
    : public NamedClassDecoder {
 public:
  NamedStoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398()
    : NamedClassDecoder(decoder_, "StoreBasedOffsetMemoryDouble Strd_Rule_201_A1_P398")
  {}

 private:
  nacl_arm_dec::StoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398);
};

class NamedUndefined_Undefined_A5_6
    : public NamedClassDecoder {
 public:
  NamedUndefined_Undefined_A5_6()
    : NamedClassDecoder(decoder_, "Undefined Undefined_A5_6")
  {}

 private:
  nacl_arm_dec::Undefined_Undefined_A5_6 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUndefined_Undefined_A5_6);
};

class NamedUnpredictable_Unpredictable
    : public NamedClassDecoder {
 public:
  NamedUnpredictable_Unpredictable()
    : NamedClassDecoder(decoder_, "Unpredictable Unpredictable")
  {}

 private:
  nacl_arm_dec::Unpredictable_Unpredictable decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnpredictable_Unpredictable);
};

class NamedVfpOp_Vabs_Rule_269_A2_P532
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vabs_Rule_269_A2_P532()
    : NamedClassDecoder(decoder_, "VfpOp Vabs_Rule_269_A2_P532")
  {}

 private:
  nacl_arm_dec::VfpOp_Vabs_Rule_269_A2_P532 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vabs_Rule_269_A2_P532);
};

class NamedVfpOp_Vadd_Rule_271_A2_P536
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vadd_Rule_271_A2_P536()
    : NamedClassDecoder(decoder_, "VfpOp Vadd_Rule_271_A2_P536")
  {}

 private:
  nacl_arm_dec::VfpOp_Vadd_Rule_271_A2_P536 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vadd_Rule_271_A2_P536);
};

class NamedVfpOp_Vcmp_Vcmpe_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vcmp_Vcmpe_Rule_A1()
    : NamedClassDecoder(decoder_, "VfpOp Vcmp_Vcmpe_Rule_A1")
  {}

 private:
  nacl_arm_dec::VfpOp_Vcmp_Vcmpe_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vcmp_Vcmpe_Rule_A1);
};

class NamedVfpOp_Vcmp_Vcmpe_Rule_A2
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vcmp_Vcmpe_Rule_A2()
    : NamedClassDecoder(decoder_, "VfpOp Vcmp_Vcmpe_Rule_A2")
  {}

 private:
  nacl_arm_dec::VfpOp_Vcmp_Vcmpe_Rule_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vcmp_Vcmpe_Rule_A2);
};

class NamedVfpOp_Vcvt_Rule_297_A1_P582
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vcvt_Rule_297_A1_P582()
    : NamedClassDecoder(decoder_, "VfpOp Vcvt_Rule_297_A1_P582")
  {}

 private:
  nacl_arm_dec::VfpOp_Vcvt_Rule_297_A1_P582 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vcvt_Rule_297_A1_P582);
};

class NamedVfpOp_Vcvt_Rule_298_A1_P584
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vcvt_Rule_298_A1_P584()
    : NamedClassDecoder(decoder_, "VfpOp Vcvt_Rule_298_A1_P584")
  {}

 private:
  nacl_arm_dec::VfpOp_Vcvt_Rule_298_A1_P584 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vcvt_Rule_298_A1_P584);
};

class NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578()
    : NamedClassDecoder(decoder_, "VfpOp Vcvt_Vcvtr_Rule_295_A1_P578")
  {}

 private:
  nacl_arm_dec::VfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578);
};

class NamedVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588()
    : NamedClassDecoder(decoder_, "VfpOp Vcvtb_Vcvtt_Rule_300_A1_P588")
  {}

 private:
  nacl_arm_dec::VfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588);
};

class NamedVfpOp_Vdiv_Rule_301_A1_P590
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vdiv_Rule_301_A1_P590()
    : NamedClassDecoder(decoder_, "VfpOp Vdiv_Rule_301_A1_P590")
  {}

 private:
  nacl_arm_dec::VfpOp_Vdiv_Rule_301_A1_P590 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vdiv_Rule_301_A1_P590);
};

class NamedVfpOp_Vfma_vfms_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vfma_vfms_Rule_A1()
    : NamedClassDecoder(decoder_, "VfpOp Vfma_vfms_Rule_A1")
  {}

 private:
  nacl_arm_dec::VfpOp_Vfma_vfms_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vfma_vfms_Rule_A1);
};

class NamedVfpOp_Vfnma_vfnms_Rule_A1
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vfnma_vfnms_Rule_A1()
    : NamedClassDecoder(decoder_, "VfpOp Vfnma_vfnms_Rule_A1")
  {}

 private:
  nacl_arm_dec::VfpOp_Vfnma_vfnms_Rule_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vfnma_vfnms_Rule_A1);
};

class NamedVfpOp_Vmla_vmls_Rule_423_A2_P636
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vmla_vmls_Rule_423_A2_P636()
    : NamedClassDecoder(decoder_, "VfpOp Vmla_vmls_Rule_423_A2_P636")
  {}

 private:
  nacl_arm_dec::VfpOp_Vmla_vmls_Rule_423_A2_P636 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vmla_vmls_Rule_423_A2_P636);
};

class NamedVfpOp_Vmov_Rule_326_A2_P640
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vmov_Rule_326_A2_P640()
    : NamedClassDecoder(decoder_, "VfpOp Vmov_Rule_326_A2_P640")
  {}

 private:
  nacl_arm_dec::VfpOp_Vmov_Rule_326_A2_P640 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vmov_Rule_326_A2_P640);
};

class NamedVfpOp_Vmov_Rule_327_A2_P642
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vmov_Rule_327_A2_P642()
    : NamedClassDecoder(decoder_, "VfpOp Vmov_Rule_327_A2_P642")
  {}

 private:
  nacl_arm_dec::VfpOp_Vmov_Rule_327_A2_P642 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vmov_Rule_327_A2_P642);
};

class NamedVfpOp_Vmul_Rule_338_A2_P664
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vmul_Rule_338_A2_P664()
    : NamedClassDecoder(decoder_, "VfpOp Vmul_Rule_338_A2_P664")
  {}

 private:
  nacl_arm_dec::VfpOp_Vmul_Rule_338_A2_P664 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vmul_Rule_338_A2_P664);
};

class NamedVfpOp_Vneg_Rule_342_A2_P672
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vneg_Rule_342_A2_P672()
    : NamedClassDecoder(decoder_, "VfpOp Vneg_Rule_342_A2_P672")
  {}

 private:
  nacl_arm_dec::VfpOp_Vneg_Rule_342_A2_P672 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vneg_Rule_342_A2_P672);
};

class NamedVfpOp_Vnmla_vnmls_Rule_343_A1_P674
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vnmla_vnmls_Rule_343_A1_P674()
    : NamedClassDecoder(decoder_, "VfpOp Vnmla_vnmls_Rule_343_A1_P674")
  {}

 private:
  nacl_arm_dec::VfpOp_Vnmla_vnmls_Rule_343_A1_P674 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vnmla_vnmls_Rule_343_A1_P674);
};

class NamedVfpOp_Vnmul_Rule_343_A2_P674
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vnmul_Rule_343_A2_P674()
    : NamedClassDecoder(decoder_, "VfpOp Vnmul_Rule_343_A2_P674")
  {}

 private:
  nacl_arm_dec::VfpOp_Vnmul_Rule_343_A2_P674 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vnmul_Rule_343_A2_P674);
};

class NamedVfpOp_Vsqrt_Rule_388_A1_P762
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vsqrt_Rule_388_A1_P762()
    : NamedClassDecoder(decoder_, "VfpOp Vsqrt_Rule_388_A1_P762")
  {}

 private:
  nacl_arm_dec::VfpOp_Vsqrt_Rule_388_A1_P762 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vsqrt_Rule_388_A1_P762);
};

class NamedVfpOp_Vsub_Rule_402_A2_P790
    : public NamedClassDecoder {
 public:
  NamedVfpOp_Vsub_Rule_402_A2_P790()
    : NamedClassDecoder(decoder_, "VfpOp Vsub_Rule_402_A2_P790")
  {}

 private:
  nacl_arm_dec::VfpOp_Vsub_Rule_402_A2_P790 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpOp_Vsub_Rule_402_A2_P790);
};


// Defines the default parse action if the table doesn't define
// an action.
class NotImplementedNamed : public NamedClassDecoder {
 public:
  NotImplementedNamed()
    : NamedClassDecoder(decoder_, "not implemented")
  {}

 private:
  nacl_arm_dec::NotImplemented decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NotImplementedNamed);
};

} // namespace nacl_arm_test
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_CLASSES_H_
