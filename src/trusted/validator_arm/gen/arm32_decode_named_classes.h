/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
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

class Binary2RegisterBitRangeMsbGeLsb_BFI
    : public Binary2RegisterBitRangeMsbGeLsb {
};

class Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_SBFX
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {
};

class Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_UBFX
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

class Binary3RegisterOpAltA_SMULBB_SMULBT_SMULTB_SMULTT
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_SMULWB_SMULWT
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltA_USAD8
    : public Binary3RegisterOpAltA {
};

class Binary3RegisterOpAltANoCondsUpdate_SDIV
    : public Binary3RegisterOpAltANoCondsUpdate {
};

class Binary3RegisterOpAltANoCondsUpdate_SMMUL
    : public Binary3RegisterOpAltANoCondsUpdate {
};

class Binary3RegisterOpAltANoCondsUpdate_SMUAD
    : public Binary3RegisterOpAltANoCondsUpdate {
};

class Binary3RegisterOpAltANoCondsUpdate_SMUSD
    : public Binary3RegisterOpAltANoCondsUpdate {
};

class Binary3RegisterOpAltANoCondsUpdate_UDIV
    : public Binary3RegisterOpAltANoCondsUpdate {
};

class Binary3RegisterOpAltBNoCondUpdates_PKH
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QADD
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QADD16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QADD8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QASX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QDADD
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QDSUB
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QSAX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QSUB
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QSUB16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_QSUB8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SADD16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SADD8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SASX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SEL
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SHADD16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SHADD8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SHASX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SHSAX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SHSUB16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SHSUB8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SSAX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SSSUB16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SSUB8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SXTAB
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SXTAB16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_SXTAH
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UADD16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UADD8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UASX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UHADD16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UHADD8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UHASX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UHSAX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UHSUB16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UHSUB8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UQADD16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UQADD8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UQASX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UQSAX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UQSUB16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UQSUB8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_USAX
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_USUB16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_USUB8
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UXTAB
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UXTAB16
    : public Binary3RegisterOpAltBNoCondUpdates {
};

class Binary3RegisterOpAltBNoCondUpdates_UXTAH
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

class Binary4RegisterDualOp_SMLABB_SMLABT_SMLATB_SMLATT
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_SMLAWB_SMLAWT
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOp_USADA8
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOpLtV6RdNotRn_MLA_A1
    : public Binary4RegisterDualOpLtV6RdNotRn {
};

class Binary4RegisterDualOpNoCondsUpdate_SMLAD
    : public Binary4RegisterDualOpNoCondsUpdate {
};

class Binary4RegisterDualOpNoCondsUpdate_SMLSD
    : public Binary4RegisterDualOpNoCondsUpdate {
};

class Binary4RegisterDualOpNoCondsUpdate_SMMLA
    : public Binary4RegisterDualOpNoCondsUpdate {
};

class Binary4RegisterDualOpNoCondsUpdate_SMMLS
    : public Binary4RegisterDualOpNoCondsUpdate {
};

class Binary4RegisterDualResult_SMLALBB_SMLALBT_SMLALTB_SMLALTT
    : public Binary4RegisterDualResult {
};

class Binary4RegisterDualResult_UMAAL_A1
    : public Binary4RegisterDualResult {
};

class Binary4RegisterDualResultLtV6RdHiLoNotRn_SMLAL_A1
    : public Binary4RegisterDualResultLtV6RdHiLoNotRn {
};

class Binary4RegisterDualResultLtV6RdHiLoNotRn_UMLAL_A1
    : public Binary4RegisterDualResultLtV6RdHiLoNotRn {
};

class Binary4RegisterDualResultNoCondsUpdate_SMLALD
    : public Binary4RegisterDualResultNoCondsUpdate {
};

class Binary4RegisterDualResultNoCondsUpdate_SMLSLD
    : public Binary4RegisterDualResultNoCondsUpdate {
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

class BranchImmediate24_B
    : public BranchImmediate24 {
};

class BranchImmediate24_BL_BLX_immediate
    : public BranchImmediate24 {
};

class BranchToRegister_BLX_register
    : public BranchToRegister {
};

class BranchToRegister_Bx
    : public BranchToRegister {
};

class BreakPointAndConstantPoolHead_BKPT
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

class Deprecated_SWP_SWPB
    : public Deprecated {
};

class DuplicateToAdvSIMDRegisters_VDUP_arm_core_register
    : public DuplicateToAdvSIMDRegisters {
};

class Forbidden_None
    : public Forbidden {
};

class ForbiddenCondDecoder_BXJ
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Cdp_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Dbg_Rule_40_A1_P88
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_ERET
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_HVC
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_LDM_User_registers
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_LDM_exception_return
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_LDRBT_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_LDRBT_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_LDRT_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_LDRT_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldc_immediate_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Ldc_literal_Rule_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_MRS_Banked_register
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_MSR_Banked_register
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_MSR_register
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

class ForbiddenCondDecoder_SMC
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_STM_User_registers
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_STRBT_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_STRBT_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_STRT_A1
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_STRT_A2
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Sev_Rule_158_A1_P316
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_Stc_Rule_A2
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

class LdrImmediateOp_LDR_immediate
    : public LdrImmediateOp {
};

class Load2RegisterImm12Op_LDRB_immediate
    : public Load2RegisterImm12Op {
};

class Load2RegisterImm12Op_LDRB_literal
    : public Load2RegisterImm12Op {
};

class Load2RegisterImm12Op_LDR_literal
    : public Load2RegisterImm12Op {
};

class Load2RegisterImm8DoubleOp_LDRD_immediate
    : public Load2RegisterImm8DoubleOp {
};

class Load2RegisterImm8Op_LDRH_immediate
    : public Load2RegisterImm8Op {
};

class Load2RegisterImm8Op_LDRSB_immediate
    : public Load2RegisterImm8Op {
};

class Load2RegisterImm8Op_LDRSH_immediate
    : public Load2RegisterImm8Op {
};

class Load3RegisterDoubleOp_LDRD_register
    : public Load3RegisterDoubleOp {
};

class Load3RegisterImm5Op_LDRB_register
    : public Load3RegisterImm5Op {
};

class Load3RegisterImm5Op_LDR_register
    : public Load3RegisterImm5Op {
};

class Load3RegisterOp_LDRH_register
    : public Load3RegisterOp {
};

class Load3RegisterOp_LDRSB_register
    : public Load3RegisterOp {
};

class Load3RegisterOp_LDRSH_register
    : public Load3RegisterOp {
};

class LoadExclusive2RegisterDoubleOp_LDREXD
    : public LoadExclusive2RegisterDoubleOp {
};

class LoadExclusive2RegisterOp_LDREX
    : public LoadExclusive2RegisterOp {
};

class LoadExclusive2RegisterOp_LDREXB
    : public LoadExclusive2RegisterOp {
};

class LoadExclusive2RegisterOp_STREXH
    : public LoadExclusive2RegisterOp {
};

class LoadRegisterImm8DoubleOp_LDRD_literal
    : public LoadRegisterImm8DoubleOp {
};

class LoadRegisterImm8Op_LDRH_literal
    : public LoadRegisterImm8Op {
};

class LoadRegisterImm8Op_LDRSB_literal
    : public LoadRegisterImm8Op {
};

class LoadRegisterList_LDMDA_LDMFA
    : public LoadRegisterList {
};

class LoadRegisterList_LDMDB_LDMEA
    : public LoadRegisterList {
};

class LoadRegisterList_LDMIB_LDMED
    : public LoadRegisterList {
};

class LoadRegisterList_LDM_LDMIA_LDMFD
    : public LoadRegisterList {
};

class LoadVectorRegister_VLDR
    : public LoadVectorRegister {
};

class LoadVectorRegisterList_VLDM
    : public LoadVectorRegisterList {
};

class LoadVectorRegisterList_VPOP
    : public LoadVectorRegisterList {
};

class MaskedBinary2RegisterImmediateOp_BIC_immediate
    : public MaskedBinary2RegisterImmediateOp {
};

class MaskedBinaryRegisterImmediateTest_TST_immediate
    : public MaskedBinaryRegisterImmediateTest {
};

class MoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register
    : public MoveDoubleVfpRegisterOp {
};

class MoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers
    : public MoveDoubleVfpRegisterOp {
};

class MoveImmediate12ToApsr_Msr_Rule_103_A1_P208
    : public MoveImmediate12ToApsr {
};

class MoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register
    : public MoveVfpRegisterOp {
};

class MoveVfpRegisterOpWithTypeSel_MOVE_scalar_to_ARM_core_register
    : public MoveVfpRegisterOpWithTypeSel {
};

class MoveVfpRegisterOpWithTypeSel_VMOV_ARM_core_register_to_scalar
    : public MoveVfpRegisterOpWithTypeSel {
};

class PermanentlyUndefined_UDF
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

class Store2RegisterImm12Op_STRB_immediate
    : public Store2RegisterImm12Op {
};

class Store2RegisterImm12Op_STR_immediate
    : public Store2RegisterImm12Op {
};

class Store2RegisterImm8DoubleOp_STRD_immediate
    : public Store2RegisterImm8DoubleOp {
};

class Store2RegisterImm8Op_STRH_immediate
    : public Store2RegisterImm8Op {
};

class Store3RegisterDoubleOp_STRD_register
    : public Store3RegisterDoubleOp {
};

class Store3RegisterImm5Op_STRB_register
    : public Store3RegisterImm5Op {
};

class Store3RegisterImm5Op_STR_register
    : public Store3RegisterImm5Op {
};

class Store3RegisterOp_STRH_register
    : public Store3RegisterOp {
};

class StoreExclusive3RegisterDoubleOp_STREXD
    : public StoreExclusive3RegisterDoubleOp {
};

class StoreExclusive3RegisterOp_STREX
    : public StoreExclusive3RegisterOp {
};

class StoreExclusive3RegisterOp_STREXB
    : public StoreExclusive3RegisterOp {
};

class StoreExclusive3RegisterOp_STREXH
    : public StoreExclusive3RegisterOp {
};

class StoreRegisterList_STMDA_STMED
    : public StoreRegisterList {
};

class StoreRegisterList_STMDB_STMFD
    : public StoreRegisterList {
};

class StoreRegisterList_STMIB_STMFA
    : public StoreRegisterList {
};

class StoreRegisterList_STM_STMIA_STMEA
    : public StoreRegisterList {
};

class StoreVectorRegister_VSTR
    : public StoreVectorRegister {
};

class StoreVectorRegisterList_VPUSH
    : public StoreVectorRegisterList {
};

class StoreVectorRegisterList_VSTM
    : public StoreVectorRegisterList {
};

class Unary1RegisterBitRangeMsbGeLsb_BFC
    : public Unary1RegisterBitRangeMsbGeLsb {
};

class Unary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1
    : public Unary1RegisterImmediateOp12DynCodeReplace {
};

class Unary1RegisterImmediateOp12DynCodeReplace_MVN_immediate
    : public Unary1RegisterImmediateOp12DynCodeReplace {
};

class Unary1RegisterImmediateOpDynCodeReplace_MOVT
    : public Unary1RegisterImmediateOpDynCodeReplace {
};

class Unary1RegisterImmediateOpDynCodeReplace_MOVW
    : public Unary1RegisterImmediateOpDynCodeReplace {
};

class Unary1RegisterImmediateOpPc_ADR_A1
    : public Unary1RegisterImmediateOpPc {
};

class Unary1RegisterImmediateOpPc_ADR_A2
    : public Unary1RegisterImmediateOpPc {
};

class Unary1RegisterSet_MRS
    : public Unary1RegisterSet {
};

class Unary1RegisterUse_MSR_register
    : public Unary1RegisterUse {
};

class Unary2RegisterImmedShiftedOp_RBIT
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_REV
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_REV16
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_REVSH
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_SXTB
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_SXTB16
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_SXTH
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_UXTB
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_UXTB16
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterImmedShiftedOp_UXTH
    : public Unary2RegisterImmedShiftedOp {
};

class Unary2RegisterOp_MOV_register
    : public Unary2RegisterOp {
};

class Unary2RegisterOp_RRX
    : public Unary2RegisterOp {
};

class Unary2RegisterOpNotRmIsPc_CLZ
    : public Unary2RegisterOpNotRmIsPc {
};

class Unary2RegisterSatImmedShiftedOp_SSAT
    : public Unary2RegisterSatImmedShiftedOp {
};

class Unary2RegisterSatImmedShiftedOp_SSAT16
    : public Unary2RegisterSatImmedShiftedOp {
};

class Unary2RegisterSatImmedShiftedOp_USAT
    : public Unary2RegisterSatImmedShiftedOp {
};

class Unary2RegisterSatImmedShiftedOp_USAT16
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

class VfpMrsOp_VMRS
    : public VfpMrsOp {
};

class VfpUsesRegOp_VMSR
    : public VfpUsesRegOp {
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

class Forbidden_BXJ
    : public Forbidden {
};

class Forbidden_Blx_Rule_23_A2_P58
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

class Forbidden_ERET
    : public Forbidden {
};

class Forbidden_HVC
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

class Forbidden_MRS_Banked_register
    : public Forbidden {
};

class Forbidden_MSR_Banked_register
    : public Forbidden {
};

class Forbidden_MSR_register
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

class Forbidden_Rfe_Rule_B6_1_10_A1_B6_16
    : public Forbidden {
};

class Forbidden_SMC
    : public Forbidden {
};

class Forbidden_Setend_Rule_157_P314
    : public Forbidden {
};

class Forbidden_Sev_Rule_158_A1_P316
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

class NamedBinary2RegisterBitRangeMsbGeLsb_BFI
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterBitRangeMsbGeLsb_BFI()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRangeMsbGeLsb BFI")
  {}

 private:
  nacl_arm_dec::Binary2RegisterBitRangeMsbGeLsb_BFI decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRangeMsbGeLsb_BFI);
};

class NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_SBFX
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_SBFX()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRangeNotRnIsPcBitfieldExtract SBFX")
  {}

 private:
  nacl_arm_dec::Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_SBFX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_SBFX);
};

class NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_UBFX
    : public NamedClassDecoder {
 public:
  NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_UBFX()
    : NamedClassDecoder(decoder_, "Binary2RegisterBitRangeNotRnIsPcBitfieldExtract UBFX")
  {}

 private:
  nacl_arm_dec::Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_UBFX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary2RegisterBitRangeNotRnIsPcBitfieldExtract_UBFX);
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

class NamedBinary3RegisterOpAltA_SMULBB_SMULBT_SMULTB_SMULTT
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_SMULBB_SMULBT_SMULTB_SMULTT()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA SMULBB_SMULBT_SMULTB_SMULTT")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_SMULBB_SMULBT_SMULTB_SMULTT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_SMULBB_SMULBT_SMULTB_SMULTT);
};

class NamedBinary3RegisterOpAltA_SMULWB_SMULWT
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_SMULWB_SMULWT()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA SMULWB_SMULWT")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_SMULWB_SMULWT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_SMULWB_SMULWT);
};

class NamedBinary3RegisterOpAltA_USAD8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltA_USAD8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltA USAD8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltA_USAD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltA_USAD8);
};

class NamedBinary3RegisterOpAltANoCondsUpdate_SDIV
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltANoCondsUpdate_SDIV()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltANoCondsUpdate SDIV")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltANoCondsUpdate_SDIV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltANoCondsUpdate_SDIV);
};

class NamedBinary3RegisterOpAltANoCondsUpdate_SMMUL
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltANoCondsUpdate_SMMUL()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltANoCondsUpdate SMMUL")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltANoCondsUpdate_SMMUL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltANoCondsUpdate_SMMUL);
};

class NamedBinary3RegisterOpAltANoCondsUpdate_SMUAD
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltANoCondsUpdate_SMUAD()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltANoCondsUpdate SMUAD")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltANoCondsUpdate_SMUAD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltANoCondsUpdate_SMUAD);
};

class NamedBinary3RegisterOpAltANoCondsUpdate_SMUSD
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltANoCondsUpdate_SMUSD()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltANoCondsUpdate SMUSD")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltANoCondsUpdate_SMUSD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltANoCondsUpdate_SMUSD);
};

class NamedBinary3RegisterOpAltANoCondsUpdate_UDIV
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltANoCondsUpdate_UDIV()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltANoCondsUpdate UDIV")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltANoCondsUpdate_UDIV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltANoCondsUpdate_UDIV);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_PKH
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_PKH()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates PKH")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_PKH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_PKH);
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

class NamedBinary3RegisterOpAltBNoCondUpdates_QADD16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QADD16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QADD16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QADD16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_QADD8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QADD8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QADD8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QADD8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_QASX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QASX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QASX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QASX);
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

class NamedBinary3RegisterOpAltBNoCondUpdates_QSAX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QSAX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QSAX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QSAX);
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

class NamedBinary3RegisterOpAltBNoCondUpdates_QSUB16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QSUB16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QSUB16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QSUB16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_QSUB8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_QSUB8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates QSUB8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_QSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_QSUB8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SADD16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SADD16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SADD16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SADD16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SADD8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SADD8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SADD8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SADD8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SASX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SASX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SASX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SASX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SEL
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SEL()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SEL")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SEL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SEL);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SHADD16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SHADD16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SHADD16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SHADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SHADD16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SHADD8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SHADD8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SHADD8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SHADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SHADD8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SHASX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SHASX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SHASX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SHASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SHASX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SHSAX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SHSAX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SHSAX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SHSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SHSAX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SHSUB16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SHSUB16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SHSUB16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SHSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SHSUB16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SHSUB8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SHSUB8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SHSUB8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SHSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SHSUB8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SSAX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SSAX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SSAX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SSAX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SSSUB16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SSSUB16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SSSUB16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SSSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SSSUB16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SSUB8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SSUB8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SSUB8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SSUB8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SXTAB
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SXTAB()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SXTAB")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SXTAB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SXTAB);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SXTAB16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SXTAB16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SXTAB16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SXTAB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SXTAB16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_SXTAH
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_SXTAH()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates SXTAH")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_SXTAH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_SXTAH);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UADD16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UADD16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UADD16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UADD16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UADD8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UADD8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UADD8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UADD8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UASX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UASX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UASX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UASX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UHADD16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UHADD16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UHADD16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UHADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UHADD16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UHADD8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UHADD8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UHADD8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UHADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UHADD8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UHASX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UHASX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UHASX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UHASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UHASX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UHSAX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UHSAX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UHSAX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UHSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UHSAX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UHSUB16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UHSUB16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UHSUB16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UHSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UHSUB16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UHSUB8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UHSUB8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UHSUB8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UHSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UHSUB8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UQADD16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UQADD16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UQADD16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UQADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UQADD16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UQADD8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UQADD8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UQADD8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UQADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UQADD8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UQASX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UQASX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UQASX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UQASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UQASX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UQSAX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UQSAX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UQSAX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UQSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UQSAX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UQSUB16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UQSUB16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UQSUB16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UQSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UQSUB16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UQSUB8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UQSUB8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UQSUB8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UQSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UQSUB8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_USAX
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_USAX()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates USAX")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_USAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_USAX);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_USUB16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_USUB16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates USUB16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_USUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_USUB16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_USUB8
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_USUB8()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates USUB8")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_USUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_USUB8);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UXTAB
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UXTAB()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UXTAB")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UXTAB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UXTAB);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UXTAB16
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UXTAB16()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UXTAB16")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UXTAB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UXTAB16);
};

class NamedBinary3RegisterOpAltBNoCondUpdates_UXTAH
    : public NamedClassDecoder {
 public:
  NamedBinary3RegisterOpAltBNoCondUpdates_UXTAH()
    : NamedClassDecoder(decoder_, "Binary3RegisterOpAltBNoCondUpdates UXTAH")
  {}

 private:
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates_UXTAH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary3RegisterOpAltBNoCondUpdates_UXTAH);
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

class NamedBinary4RegisterDualOp_SMLABB_SMLABT_SMLATB_SMLATT
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_SMLABB_SMLABT_SMLATB_SMLATT()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp SMLABB_SMLABT_SMLATB_SMLATT")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_SMLABB_SMLABT_SMLATB_SMLATT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_SMLABB_SMLABT_SMLATB_SMLATT);
};

class NamedBinary4RegisterDualOp_SMLAWB_SMLAWT
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_SMLAWB_SMLAWT()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp SMLAWB_SMLAWT")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_SMLAWB_SMLAWT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_SMLAWB_SMLAWT);
};

class NamedBinary4RegisterDualOp_USADA8
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOp_USADA8()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOp USADA8")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOp_USADA8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOp_USADA8);
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

class NamedBinary4RegisterDualOpNoCondsUpdate_SMLAD
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOpNoCondsUpdate_SMLAD()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOpNoCondsUpdate SMLAD")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOpNoCondsUpdate_SMLAD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOpNoCondsUpdate_SMLAD);
};

class NamedBinary4RegisterDualOpNoCondsUpdate_SMLSD
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOpNoCondsUpdate_SMLSD()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOpNoCondsUpdate SMLSD")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOpNoCondsUpdate_SMLSD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOpNoCondsUpdate_SMLSD);
};

class NamedBinary4RegisterDualOpNoCondsUpdate_SMMLA
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOpNoCondsUpdate_SMMLA()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOpNoCondsUpdate SMMLA")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOpNoCondsUpdate_SMMLA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOpNoCondsUpdate_SMMLA);
};

class NamedBinary4RegisterDualOpNoCondsUpdate_SMMLS
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualOpNoCondsUpdate_SMMLS()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualOpNoCondsUpdate SMMLS")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualOpNoCondsUpdate_SMMLS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualOpNoCondsUpdate_SMMLS);
};

class NamedBinary4RegisterDualResult_SMLALBB_SMLALBT_SMLALTB_SMLALTT
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResult_SMLALBB_SMLALBT_SMLALTB_SMLALTT()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResult SMLALBB_SMLALBT_SMLALTB_SMLALTT")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResult_SMLALBB_SMLALBT_SMLALTB_SMLALTT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResult_SMLALBB_SMLALBT_SMLALTB_SMLALTT);
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

class NamedBinary4RegisterDualResultNoCondsUpdate_SMLALD
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResultNoCondsUpdate_SMLALD()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResultNoCondsUpdate SMLALD")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResultNoCondsUpdate_SMLALD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResultNoCondsUpdate_SMLALD);
};

class NamedBinary4RegisterDualResultNoCondsUpdate_SMLSLD
    : public NamedClassDecoder {
 public:
  NamedBinary4RegisterDualResultNoCondsUpdate_SMLSLD()
    : NamedClassDecoder(decoder_, "Binary4RegisterDualResultNoCondsUpdate SMLSLD")
  {}

 private:
  nacl_arm_dec::Binary4RegisterDualResultNoCondsUpdate_SMLSLD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBinary4RegisterDualResultNoCondsUpdate_SMLSLD);
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

class NamedBranchImmediate24_B
    : public NamedClassDecoder {
 public:
  NamedBranchImmediate24_B()
    : NamedClassDecoder(decoder_, "BranchImmediate24 B")
  {}

 private:
  nacl_arm_dec::BranchImmediate24_B decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchImmediate24_B);
};

class NamedBranchImmediate24_BL_BLX_immediate
    : public NamedClassDecoder {
 public:
  NamedBranchImmediate24_BL_BLX_immediate()
    : NamedClassDecoder(decoder_, "BranchImmediate24 BL_BLX_immediate")
  {}

 private:
  nacl_arm_dec::BranchImmediate24_BL_BLX_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchImmediate24_BL_BLX_immediate);
};

class NamedBranchToRegister_BLX_register
    : public NamedClassDecoder {
 public:
  NamedBranchToRegister_BLX_register()
    : NamedClassDecoder(decoder_, "BranchToRegister BLX_register")
  {}

 private:
  nacl_arm_dec::BranchToRegister_BLX_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchToRegister_BLX_register);
};

class NamedBranchToRegister_Bx
    : public NamedClassDecoder {
 public:
  NamedBranchToRegister_Bx()
    : NamedClassDecoder(decoder_, "BranchToRegister Bx")
  {}

 private:
  nacl_arm_dec::BranchToRegister_Bx decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBranchToRegister_Bx);
};

class NamedBreakPointAndConstantPoolHead_BKPT
    : public NamedClassDecoder {
 public:
  NamedBreakPointAndConstantPoolHead_BKPT()
    : NamedClassDecoder(decoder_, "BreakPointAndConstantPoolHead BKPT")
  {}

 private:
  nacl_arm_dec::BreakPointAndConstantPoolHead_BKPT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBreakPointAndConstantPoolHead_BKPT);
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

class NamedDeprecated_SWP_SWPB
    : public NamedClassDecoder {
 public:
  NamedDeprecated_SWP_SWPB()
    : NamedClassDecoder(decoder_, "Deprecated SWP_SWPB")
  {}

 private:
  nacl_arm_dec::Deprecated_SWP_SWPB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDeprecated_SWP_SWPB);
};

class NamedDuplicateToAdvSIMDRegisters_VDUP_arm_core_register
    : public NamedClassDecoder {
 public:
  NamedDuplicateToAdvSIMDRegisters_VDUP_arm_core_register()
    : NamedClassDecoder(decoder_, "DuplicateToAdvSIMDRegisters VDUP_arm_core_register")
  {}

 private:
  nacl_arm_dec::DuplicateToAdvSIMDRegisters_VDUP_arm_core_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDuplicateToAdvSIMDRegisters_VDUP_arm_core_register);
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

class NamedForbiddenCondDecoder_BXJ
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_BXJ()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder BXJ")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_BXJ decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_BXJ);
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

class NamedForbiddenCondDecoder_ERET
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_ERET()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder ERET")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_ERET decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_ERET);
};

class NamedForbiddenCondDecoder_HVC
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_HVC()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder HVC")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_HVC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_HVC);
};

class NamedForbiddenCondDecoder_LDM_User_registers
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_LDM_User_registers()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder LDM_User_registers")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_LDM_User_registers decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_LDM_User_registers);
};

class NamedForbiddenCondDecoder_LDM_exception_return
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_LDM_exception_return()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder LDM_exception_return")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_LDM_exception_return decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_LDM_exception_return);
};

class NamedForbiddenCondDecoder_LDRBT_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_LDRBT_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder LDRBT_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_LDRBT_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_LDRBT_A1);
};

class NamedForbiddenCondDecoder_LDRBT_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_LDRBT_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder LDRBT_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_LDRBT_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_LDRBT_A2);
};

class NamedForbiddenCondDecoder_LDRT_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_LDRT_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder LDRT_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_LDRT_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_LDRT_A1);
};

class NamedForbiddenCondDecoder_LDRT_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_LDRT_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder LDRT_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_LDRT_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_LDRT_A2);
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

class NamedForbiddenCondDecoder_MRS_Banked_register
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_MRS_Banked_register()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder MRS_Banked_register")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_MRS_Banked_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_MRS_Banked_register);
};

class NamedForbiddenCondDecoder_MSR_Banked_register
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_MSR_Banked_register()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder MSR_Banked_register")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_MSR_Banked_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_MSR_Banked_register);
};

class NamedForbiddenCondDecoder_MSR_register
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_MSR_register()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder MSR_register")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_MSR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_MSR_register);
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

class NamedForbiddenCondDecoder_SMC
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_SMC()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder SMC")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_SMC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_SMC);
};

class NamedForbiddenCondDecoder_STM_User_registers
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_STM_User_registers()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder STM_User_registers")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_STM_User_registers decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_STM_User_registers);
};

class NamedForbiddenCondDecoder_STRBT_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_STRBT_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder STRBT_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_STRBT_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_STRBT_A1);
};

class NamedForbiddenCondDecoder_STRBT_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_STRBT_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder STRBT_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_STRBT_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_STRBT_A2);
};

class NamedForbiddenCondDecoder_STRT_A1
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_STRT_A1()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder STRT_A1")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_STRT_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_STRT_A1);
};

class NamedForbiddenCondDecoder_STRT_A2
    : public NamedClassDecoder {
 public:
  NamedForbiddenCondDecoder_STRT_A2()
    : NamedClassDecoder(decoder_, "ForbiddenCondDecoder STRT_A2")
  {}

 private:
  nacl_arm_dec::ForbiddenCondDecoder_STRT_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbiddenCondDecoder_STRT_A2);
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

class NamedLdrImmediateOp_LDR_immediate
    : public NamedClassDecoder {
 public:
  NamedLdrImmediateOp_LDR_immediate()
    : NamedClassDecoder(decoder_, "LdrImmediateOp LDR_immediate")
  {}

 private:
  nacl_arm_dec::LdrImmediateOp_LDR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLdrImmediateOp_LDR_immediate);
};

class NamedLoad2RegisterImm12Op_LDRB_immediate
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm12Op_LDRB_immediate()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op LDRB_immediate")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_LDRB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_LDRB_immediate);
};

class NamedLoad2RegisterImm12Op_LDRB_literal
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm12Op_LDRB_literal()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op LDRB_literal")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_LDRB_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_LDRB_literal);
};

class NamedLoad2RegisterImm12Op_LDR_literal
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm12Op_LDR_literal()
    : NamedClassDecoder(decoder_, "Load2RegisterImm12Op LDR_literal")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm12Op_LDR_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm12Op_LDR_literal);
};

class NamedLoad2RegisterImm8DoubleOp_LDRD_immediate
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8DoubleOp_LDRD_immediate()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8DoubleOp LDRD_immediate")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8DoubleOp_LDRD_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8DoubleOp_LDRD_immediate);
};

class NamedLoad2RegisterImm8Op_LDRH_immediate
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_LDRH_immediate()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op LDRH_immediate")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_LDRH_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_LDRH_immediate);
};

class NamedLoad2RegisterImm8Op_LDRSB_immediate
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_LDRSB_immediate()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op LDRSB_immediate")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_LDRSB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_LDRSB_immediate);
};

class NamedLoad2RegisterImm8Op_LDRSH_immediate
    : public NamedClassDecoder {
 public:
  NamedLoad2RegisterImm8Op_LDRSH_immediate()
    : NamedClassDecoder(decoder_, "Load2RegisterImm8Op LDRSH_immediate")
  {}

 private:
  nacl_arm_dec::Load2RegisterImm8Op_LDRSH_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad2RegisterImm8Op_LDRSH_immediate);
};

class NamedLoad3RegisterDoubleOp_LDRD_register
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterDoubleOp_LDRD_register()
    : NamedClassDecoder(decoder_, "Load3RegisterDoubleOp LDRD_register")
  {}

 private:
  nacl_arm_dec::Load3RegisterDoubleOp_LDRD_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterDoubleOp_LDRD_register);
};

class NamedLoad3RegisterImm5Op_LDRB_register
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterImm5Op_LDRB_register()
    : NamedClassDecoder(decoder_, "Load3RegisterImm5Op LDRB_register")
  {}

 private:
  nacl_arm_dec::Load3RegisterImm5Op_LDRB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterImm5Op_LDRB_register);
};

class NamedLoad3RegisterImm5Op_LDR_register
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterImm5Op_LDR_register()
    : NamedClassDecoder(decoder_, "Load3RegisterImm5Op LDR_register")
  {}

 private:
  nacl_arm_dec::Load3RegisterImm5Op_LDR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterImm5Op_LDR_register);
};

class NamedLoad3RegisterOp_LDRH_register
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterOp_LDRH_register()
    : NamedClassDecoder(decoder_, "Load3RegisterOp LDRH_register")
  {}

 private:
  nacl_arm_dec::Load3RegisterOp_LDRH_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_LDRH_register);
};

class NamedLoad3RegisterOp_LDRSB_register
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterOp_LDRSB_register()
    : NamedClassDecoder(decoder_, "Load3RegisterOp LDRSB_register")
  {}

 private:
  nacl_arm_dec::Load3RegisterOp_LDRSB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_LDRSB_register);
};

class NamedLoad3RegisterOp_LDRSH_register
    : public NamedClassDecoder {
 public:
  NamedLoad3RegisterOp_LDRSH_register()
    : NamedClassDecoder(decoder_, "Load3RegisterOp LDRSH_register")
  {}

 private:
  nacl_arm_dec::Load3RegisterOp_LDRSH_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoad3RegisterOp_LDRSH_register);
};

class NamedLoadExclusive2RegisterDoubleOp_LDREXD
    : public NamedClassDecoder {
 public:
  NamedLoadExclusive2RegisterDoubleOp_LDREXD()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterDoubleOp LDREXD")
  {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterDoubleOp_LDREXD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterDoubleOp_LDREXD);
};

class NamedLoadExclusive2RegisterOp_LDREX
    : public NamedClassDecoder {
 public:
  NamedLoadExclusive2RegisterOp_LDREX()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp LDREX")
  {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_LDREX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_LDREX);
};

class NamedLoadExclusive2RegisterOp_LDREXB
    : public NamedClassDecoder {
 public:
  NamedLoadExclusive2RegisterOp_LDREXB()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp LDREXB")
  {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_LDREXB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_LDREXB);
};

class NamedLoadExclusive2RegisterOp_STREXH
    : public NamedClassDecoder {
 public:
  NamedLoadExclusive2RegisterOp_STREXH()
    : NamedClassDecoder(decoder_, "LoadExclusive2RegisterOp STREXH")
  {}

 private:
  nacl_arm_dec::LoadExclusive2RegisterOp_STREXH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadExclusive2RegisterOp_STREXH);
};

class NamedLoadRegisterImm8DoubleOp_LDRD_literal
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterImm8DoubleOp_LDRD_literal()
    : NamedClassDecoder(decoder_, "LoadRegisterImm8DoubleOp LDRD_literal")
  {}

 private:
  nacl_arm_dec::LoadRegisterImm8DoubleOp_LDRD_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterImm8DoubleOp_LDRD_literal);
};

class NamedLoadRegisterImm8Op_LDRH_literal
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterImm8Op_LDRH_literal()
    : NamedClassDecoder(decoder_, "LoadRegisterImm8Op LDRH_literal")
  {}

 private:
  nacl_arm_dec::LoadRegisterImm8Op_LDRH_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterImm8Op_LDRH_literal);
};

class NamedLoadRegisterImm8Op_LDRSB_literal
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterImm8Op_LDRSB_literal()
    : NamedClassDecoder(decoder_, "LoadRegisterImm8Op LDRSB_literal")
  {}

 private:
  nacl_arm_dec::LoadRegisterImm8Op_LDRSB_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterImm8Op_LDRSB_literal);
};

class NamedLoadRegisterList_LDMDA_LDMFA
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_LDMDA_LDMFA()
    : NamedClassDecoder(decoder_, "LoadRegisterList LDMDA_LDMFA")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_LDMDA_LDMFA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_LDMDA_LDMFA);
};

class NamedLoadRegisterList_LDMDB_LDMEA
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_LDMDB_LDMEA()
    : NamedClassDecoder(decoder_, "LoadRegisterList LDMDB_LDMEA")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_LDMDB_LDMEA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_LDMDB_LDMEA);
};

class NamedLoadRegisterList_LDMIB_LDMED
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_LDMIB_LDMED()
    : NamedClassDecoder(decoder_, "LoadRegisterList LDMIB_LDMED")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_LDMIB_LDMED decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_LDMIB_LDMED);
};

class NamedLoadRegisterList_LDM_LDMIA_LDMFD
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterList_LDM_LDMIA_LDMFD()
    : NamedClassDecoder(decoder_, "LoadRegisterList LDM_LDMIA_LDMFD")
  {}

 private:
  nacl_arm_dec::LoadRegisterList_LDM_LDMIA_LDMFD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterList_LDM_LDMIA_LDMFD);
};

class NamedLoadVectorRegister_VLDR
    : public NamedClassDecoder {
 public:
  NamedLoadVectorRegister_VLDR()
    : NamedClassDecoder(decoder_, "LoadVectorRegister VLDR")
  {}

 private:
  nacl_arm_dec::LoadVectorRegister_VLDR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadVectorRegister_VLDR);
};

class NamedLoadVectorRegisterList_VLDM
    : public NamedClassDecoder {
 public:
  NamedLoadVectorRegisterList_VLDM()
    : NamedClassDecoder(decoder_, "LoadVectorRegisterList VLDM")
  {}

 private:
  nacl_arm_dec::LoadVectorRegisterList_VLDM decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadVectorRegisterList_VLDM);
};

class NamedLoadVectorRegisterList_VPOP
    : public NamedClassDecoder {
 public:
  NamedLoadVectorRegisterList_VPOP()
    : NamedClassDecoder(decoder_, "LoadVectorRegisterList VPOP")
  {}

 private:
  nacl_arm_dec::LoadVectorRegisterList_VPOP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadVectorRegisterList_VPOP);
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

class NamedMoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register
    : public NamedClassDecoder {
 public:
  NamedMoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register()
    : NamedClassDecoder(decoder_, "MoveDoubleVfpRegisterOp VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register")
  {}

 private:
  nacl_arm_dec::MoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register);
};

class NamedMoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers
    : public NamedClassDecoder {
 public:
  NamedMoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers()
    : NamedClassDecoder(decoder_, "MoveDoubleVfpRegisterOp VMOV_between_two_ARM_core_registers_and_two_single_precision_registers")
  {}

 private:
  nacl_arm_dec::MoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers);
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

class NamedMoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register
    : public NamedClassDecoder {
 public:
  NamedMoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register()
    : NamedClassDecoder(decoder_, "MoveVfpRegisterOp VMOV_between_ARM_core_register_and_single_precision_register")
  {}

 private:
  nacl_arm_dec::MoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register);
};

class NamedMoveVfpRegisterOpWithTypeSel_MOVE_scalar_to_ARM_core_register
    : public NamedClassDecoder {
 public:
  NamedMoveVfpRegisterOpWithTypeSel_MOVE_scalar_to_ARM_core_register()
    : NamedClassDecoder(decoder_, "MoveVfpRegisterOpWithTypeSel MOVE_scalar_to_ARM_core_register")
  {}

 private:
  nacl_arm_dec::MoveVfpRegisterOpWithTypeSel_MOVE_scalar_to_ARM_core_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveVfpRegisterOpWithTypeSel_MOVE_scalar_to_ARM_core_register);
};

class NamedMoveVfpRegisterOpWithTypeSel_VMOV_ARM_core_register_to_scalar
    : public NamedClassDecoder {
 public:
  NamedMoveVfpRegisterOpWithTypeSel_VMOV_ARM_core_register_to_scalar()
    : NamedClassDecoder(decoder_, "MoveVfpRegisterOpWithTypeSel VMOV_ARM_core_register_to_scalar")
  {}

 private:
  nacl_arm_dec::MoveVfpRegisterOpWithTypeSel_VMOV_ARM_core_register_to_scalar decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveVfpRegisterOpWithTypeSel_VMOV_ARM_core_register_to_scalar);
};

class NamedPermanentlyUndefined_UDF
    : public NamedClassDecoder {
 public:
  NamedPermanentlyUndefined_UDF()
    : NamedClassDecoder(decoder_, "PermanentlyUndefined UDF")
  {}

 private:
  nacl_arm_dec::PermanentlyUndefined_UDF decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPermanentlyUndefined_UDF);
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

class NamedStore2RegisterImm12Op_STRB_immediate
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm12Op_STRB_immediate()
    : NamedClassDecoder(decoder_, "Store2RegisterImm12Op STRB_immediate")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm12Op_STRB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm12Op_STRB_immediate);
};

class NamedStore2RegisterImm12Op_STR_immediate
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm12Op_STR_immediate()
    : NamedClassDecoder(decoder_, "Store2RegisterImm12Op STR_immediate")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm12Op_STR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm12Op_STR_immediate);
};

class NamedStore2RegisterImm8DoubleOp_STRD_immediate
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm8DoubleOp_STRD_immediate()
    : NamedClassDecoder(decoder_, "Store2RegisterImm8DoubleOp STRD_immediate")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm8DoubleOp_STRD_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm8DoubleOp_STRD_immediate);
};

class NamedStore2RegisterImm8Op_STRH_immediate
    : public NamedClassDecoder {
 public:
  NamedStore2RegisterImm8Op_STRH_immediate()
    : NamedClassDecoder(decoder_, "Store2RegisterImm8Op STRH_immediate")
  {}

 private:
  nacl_arm_dec::Store2RegisterImm8Op_STRH_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore2RegisterImm8Op_STRH_immediate);
};

class NamedStore3RegisterDoubleOp_STRD_register
    : public NamedClassDecoder {
 public:
  NamedStore3RegisterDoubleOp_STRD_register()
    : NamedClassDecoder(decoder_, "Store3RegisterDoubleOp STRD_register")
  {}

 private:
  nacl_arm_dec::Store3RegisterDoubleOp_STRD_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterDoubleOp_STRD_register);
};

class NamedStore3RegisterImm5Op_STRB_register
    : public NamedClassDecoder {
 public:
  NamedStore3RegisterImm5Op_STRB_register()
    : NamedClassDecoder(decoder_, "Store3RegisterImm5Op STRB_register")
  {}

 private:
  nacl_arm_dec::Store3RegisterImm5Op_STRB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterImm5Op_STRB_register);
};

class NamedStore3RegisterImm5Op_STR_register
    : public NamedClassDecoder {
 public:
  NamedStore3RegisterImm5Op_STR_register()
    : NamedClassDecoder(decoder_, "Store3RegisterImm5Op STR_register")
  {}

 private:
  nacl_arm_dec::Store3RegisterImm5Op_STR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterImm5Op_STR_register);
};

class NamedStore3RegisterOp_STRH_register
    : public NamedClassDecoder {
 public:
  NamedStore3RegisterOp_STRH_register()
    : NamedClassDecoder(decoder_, "Store3RegisterOp STRH_register")
  {}

 private:
  nacl_arm_dec::Store3RegisterOp_STRH_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStore3RegisterOp_STRH_register);
};

class NamedStoreExclusive3RegisterDoubleOp_STREXD
    : public NamedClassDecoder {
 public:
  NamedStoreExclusive3RegisterDoubleOp_STREXD()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterDoubleOp STREXD")
  {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterDoubleOp_STREXD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterDoubleOp_STREXD);
};

class NamedStoreExclusive3RegisterOp_STREX
    : public NamedClassDecoder {
 public:
  NamedStoreExclusive3RegisterOp_STREX()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp STREX")
  {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_STREX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_STREX);
};

class NamedStoreExclusive3RegisterOp_STREXB
    : public NamedClassDecoder {
 public:
  NamedStoreExclusive3RegisterOp_STREXB()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp STREXB")
  {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_STREXB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_STREXB);
};

class NamedStoreExclusive3RegisterOp_STREXH
    : public NamedClassDecoder {
 public:
  NamedStoreExclusive3RegisterOp_STREXH()
    : NamedClassDecoder(decoder_, "StoreExclusive3RegisterOp STREXH")
  {}

 private:
  nacl_arm_dec::StoreExclusive3RegisterOp_STREXH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreExclusive3RegisterOp_STREXH);
};

class NamedStoreRegisterList_STMDA_STMED
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_STMDA_STMED()
    : NamedClassDecoder(decoder_, "StoreRegisterList STMDA_STMED")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_STMDA_STMED decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_STMDA_STMED);
};

class NamedStoreRegisterList_STMDB_STMFD
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_STMDB_STMFD()
    : NamedClassDecoder(decoder_, "StoreRegisterList STMDB_STMFD")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_STMDB_STMFD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_STMDB_STMFD);
};

class NamedStoreRegisterList_STMIB_STMFA
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_STMIB_STMFA()
    : NamedClassDecoder(decoder_, "StoreRegisterList STMIB_STMFA")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_STMIB_STMFA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_STMIB_STMFA);
};

class NamedStoreRegisterList_STM_STMIA_STMEA
    : public NamedClassDecoder {
 public:
  NamedStoreRegisterList_STM_STMIA_STMEA()
    : NamedClassDecoder(decoder_, "StoreRegisterList STM_STMIA_STMEA")
  {}

 private:
  nacl_arm_dec::StoreRegisterList_STM_STMIA_STMEA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreRegisterList_STM_STMIA_STMEA);
};

class NamedStoreVectorRegister_VSTR
    : public NamedClassDecoder {
 public:
  NamedStoreVectorRegister_VSTR()
    : NamedClassDecoder(decoder_, "StoreVectorRegister VSTR")
  {}

 private:
  nacl_arm_dec::StoreVectorRegister_VSTR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreVectorRegister_VSTR);
};

class NamedStoreVectorRegisterList_VPUSH
    : public NamedClassDecoder {
 public:
  NamedStoreVectorRegisterList_VPUSH()
    : NamedClassDecoder(decoder_, "StoreVectorRegisterList VPUSH")
  {}

 private:
  nacl_arm_dec::StoreVectorRegisterList_VPUSH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreVectorRegisterList_VPUSH);
};

class NamedStoreVectorRegisterList_VSTM
    : public NamedClassDecoder {
 public:
  NamedStoreVectorRegisterList_VSTM()
    : NamedClassDecoder(decoder_, "StoreVectorRegisterList VSTM")
  {}

 private:
  nacl_arm_dec::StoreVectorRegisterList_VSTM decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedStoreVectorRegisterList_VSTM);
};

class NamedUnary1RegisterBitRangeMsbGeLsb_BFC
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterBitRangeMsbGeLsb_BFC()
    : NamedClassDecoder(decoder_, "Unary1RegisterBitRangeMsbGeLsb BFC")
  {}

 private:
  nacl_arm_dec::Unary1RegisterBitRangeMsbGeLsb_BFC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterBitRangeMsbGeLsb_BFC);
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

class NamedUnary1RegisterImmediateOpDynCodeReplace_MOVT
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterImmediateOpDynCodeReplace_MOVT()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOpDynCodeReplace MOVT")
  {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOpDynCodeReplace_MOVT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOpDynCodeReplace_MOVT);
};

class NamedUnary1RegisterImmediateOpDynCodeReplace_MOVW
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterImmediateOpDynCodeReplace_MOVW()
    : NamedClassDecoder(decoder_, "Unary1RegisterImmediateOpDynCodeReplace MOVW")
  {}

 private:
  nacl_arm_dec::Unary1RegisterImmediateOpDynCodeReplace_MOVW decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterImmediateOpDynCodeReplace_MOVW);
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

class NamedUnary1RegisterSet_MRS
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterSet_MRS()
    : NamedClassDecoder(decoder_, "Unary1RegisterSet MRS")
  {}

 private:
  nacl_arm_dec::Unary1RegisterSet_MRS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterSet_MRS);
};

class NamedUnary1RegisterUse_MSR_register
    : public NamedClassDecoder {
 public:
  NamedUnary1RegisterUse_MSR_register()
    : NamedClassDecoder(decoder_, "Unary1RegisterUse MSR_register")
  {}

 private:
  nacl_arm_dec::Unary1RegisterUse_MSR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary1RegisterUse_MSR_register);
};

class NamedUnary2RegisterImmedShiftedOp_RBIT
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_RBIT()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp RBIT")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_RBIT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_RBIT);
};

class NamedUnary2RegisterImmedShiftedOp_REV
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_REV()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp REV")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_REV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_REV);
};

class NamedUnary2RegisterImmedShiftedOp_REV16
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_REV16()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp REV16")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_REV16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_REV16);
};

class NamedUnary2RegisterImmedShiftedOp_REVSH
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_REVSH()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp REVSH")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_REVSH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_REVSH);
};

class NamedUnary2RegisterImmedShiftedOp_SXTB
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_SXTB()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp SXTB")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_SXTB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_SXTB);
};

class NamedUnary2RegisterImmedShiftedOp_SXTB16
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_SXTB16()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp SXTB16")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_SXTB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_SXTB16);
};

class NamedUnary2RegisterImmedShiftedOp_SXTH
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_SXTH()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp SXTH")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_SXTH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_SXTH);
};

class NamedUnary2RegisterImmedShiftedOp_UXTB
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_UXTB()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp UXTB")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_UXTB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_UXTB);
};

class NamedUnary2RegisterImmedShiftedOp_UXTB16
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_UXTB16()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp UXTB16")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_UXTB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_UXTB16);
};

class NamedUnary2RegisterImmedShiftedOp_UXTH
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterImmedShiftedOp_UXTH()
    : NamedClassDecoder(decoder_, "Unary2RegisterImmedShiftedOp UXTH")
  {}

 private:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp_UXTH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterImmedShiftedOp_UXTH);
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

class NamedUnary2RegisterOpNotRmIsPc_CLZ
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterOpNotRmIsPc_CLZ()
    : NamedClassDecoder(decoder_, "Unary2RegisterOpNotRmIsPc CLZ")
  {}

 private:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPc_CLZ decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterOpNotRmIsPc_CLZ);
};

class NamedUnary2RegisterSatImmedShiftedOp_SSAT
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterSatImmedShiftedOp_SSAT()
    : NamedClassDecoder(decoder_, "Unary2RegisterSatImmedShiftedOp SSAT")
  {}

 private:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp_SSAT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterSatImmedShiftedOp_SSAT);
};

class NamedUnary2RegisterSatImmedShiftedOp_SSAT16
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterSatImmedShiftedOp_SSAT16()
    : NamedClassDecoder(decoder_, "Unary2RegisterSatImmedShiftedOp SSAT16")
  {}

 private:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp_SSAT16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterSatImmedShiftedOp_SSAT16);
};

class NamedUnary2RegisterSatImmedShiftedOp_USAT
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterSatImmedShiftedOp_USAT()
    : NamedClassDecoder(decoder_, "Unary2RegisterSatImmedShiftedOp USAT")
  {}

 private:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp_USAT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterSatImmedShiftedOp_USAT);
};

class NamedUnary2RegisterSatImmedShiftedOp_USAT16
    : public NamedClassDecoder {
 public:
  NamedUnary2RegisterSatImmedShiftedOp_USAT16()
    : NamedClassDecoder(decoder_, "Unary2RegisterSatImmedShiftedOp USAT16")
  {}

 private:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp_USAT16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnary2RegisterSatImmedShiftedOp_USAT16);
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

class NamedVfpMrsOp_VMRS
    : public NamedClassDecoder {
 public:
  NamedVfpMrsOp_VMRS()
    : NamedClassDecoder(decoder_, "VfpMrsOp VMRS")
  {}

 private:
  nacl_arm_dec::VfpMrsOp_VMRS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpMrsOp_VMRS);
};

class NamedVfpUsesRegOp_VMSR
    : public NamedClassDecoder {
 public:
  NamedVfpUsesRegOp_VMSR()
    : NamedClassDecoder(decoder_, "VfpUsesRegOp VMSR")
  {}

 private:
  nacl_arm_dec::VfpUsesRegOp_VMSR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVfpUsesRegOp_VMSR);
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

class NamedForbidden_BXJ
    : public NamedClassDecoder {
 public:
  NamedForbidden_BXJ()
    : NamedClassDecoder(decoder_, "Forbidden BXJ")
  {}

 private:
  nacl_arm_dec::Forbidden_BXJ decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_BXJ);
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

class NamedForbidden_ERET
    : public NamedClassDecoder {
 public:
  NamedForbidden_ERET()
    : NamedClassDecoder(decoder_, "Forbidden ERET")
  {}

 private:
  nacl_arm_dec::Forbidden_ERET decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_ERET);
};

class NamedForbidden_HVC
    : public NamedClassDecoder {
 public:
  NamedForbidden_HVC()
    : NamedClassDecoder(decoder_, "Forbidden HVC")
  {}

 private:
  nacl_arm_dec::Forbidden_HVC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_HVC);
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

class NamedForbidden_MRS_Banked_register
    : public NamedClassDecoder {
 public:
  NamedForbidden_MRS_Banked_register()
    : NamedClassDecoder(decoder_, "Forbidden MRS_Banked_register")
  {}

 private:
  nacl_arm_dec::Forbidden_MRS_Banked_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MRS_Banked_register);
};

class NamedForbidden_MSR_Banked_register
    : public NamedClassDecoder {
 public:
  NamedForbidden_MSR_Banked_register()
    : NamedClassDecoder(decoder_, "Forbidden MSR_Banked_register")
  {}

 private:
  nacl_arm_dec::Forbidden_MSR_Banked_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MSR_Banked_register);
};

class NamedForbidden_MSR_register
    : public NamedClassDecoder {
 public:
  NamedForbidden_MSR_register()
    : NamedClassDecoder(decoder_, "Forbidden MSR_register")
  {}

 private:
  nacl_arm_dec::Forbidden_MSR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MSR_register);
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

class NamedForbidden_SMC
    : public NamedClassDecoder {
 public:
  NamedForbidden_SMC()
    : NamedClassDecoder(decoder_, "Forbidden SMC")
  {}

 private:
  nacl_arm_dec::Forbidden_SMC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_SMC);
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
