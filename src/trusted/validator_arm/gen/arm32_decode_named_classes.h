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
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_actuals.h"

#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_bases.h"

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

class CondDecoder_NOP
    : public CondDecoder {
};

class CondDecoder_YIELD
    : public CondDecoder {
};

class CondVfpOp_VABS
    : public CondVfpOp {
};

class CondVfpOp_VADD_floating_point
    : public CondVfpOp {
};

class CondVfpOp_VCMP_VCMPE
    : public CondVfpOp {
};

class CondVfpOp_VCVTB_VCVTT
    : public CondVfpOp {
};

class CondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point
    : public CondVfpOp {
};

class CondVfpOp_VCVT_between_double_precision_and_single_precision
    : public CondVfpOp {
};

class CondVfpOp_VDIV
    : public CondVfpOp {
};

class CondVfpOp_VFMA_VFMS
    : public CondVfpOp {
};

class CondVfpOp_VFNMA_VFNMS
    : public CondVfpOp {
};

class CondVfpOp_VMLA_VMLS_floating_point
    : public CondVfpOp {
};

class CondVfpOp_VMOV_immediate
    : public CondVfpOp {
};

class CondVfpOp_VMOV_register
    : public CondVfpOp {
};

class CondVfpOp_VMUL_floating_point
    : public CondVfpOp {
};

class CondVfpOp_VNEG
    : public CondVfpOp {
};

class CondVfpOp_VNMLA_VNMLS
    : public CondVfpOp {
};

class CondVfpOp_VNMUL
    : public CondVfpOp {
};

class CondVfpOp_VSQRT
    : public CondVfpOp {
};

class CondVfpOp_VSUB_floating_point
    : public CondVfpOp {
};

class DataBarrier_DMB
    : public DataBarrier {
};

class DataBarrier_DSB
    : public DataBarrier {
};

class Deprecated_SWP_SWPB
    : public Deprecated {
};

class DuplicateToAdvSIMDRegisters_VDUP_arm_core_register
    : public DuplicateToAdvSIMDRegisters {
};

class Forbidden_DBG
    : public Forbidden {
};

class Forbidden_MSR_immediate
    : public Forbidden {
};

class Forbidden_SEV
    : public Forbidden {
};

class Forbidden_WFE
    : public Forbidden {
};

class Forbidden_WFI
    : public Forbidden {
};

class Forbidden_BLX_immediate
    : public Forbidden {
};

class Forbidden_CDP
    : public Forbidden {
};

class Forbidden_CDP2
    : public Forbidden {
};

class Forbidden_CLREX
    : public Forbidden {
};

class Forbidden_CPS
    : public Forbidden {
};

class Forbidden_LDC2_immediate
    : public Forbidden {
};

class Forbidden_LDC2_literal
    : public Forbidden {
};

class Forbidden_LDC_immediate
    : public Forbidden {
};

class Forbidden_LDC_literal
    : public Forbidden {
};

class Forbidden_MCR
    : public Forbidden {
};

class Forbidden_MCR2
    : public Forbidden {
};

class Forbidden_MCRR
    : public Forbidden {
};

class Forbidden_MCRR2
    : public Forbidden {
};

class Forbidden_MRC
    : public Forbidden {
};

class Forbidden_MRC2
    : public Forbidden {
};

class Forbidden_MRRC
    : public Forbidden {
};

class Forbidden_MRRC2
    : public Forbidden {
};

class Forbidden_RFE
    : public Forbidden {
};

class Forbidden_SETEND
    : public Forbidden {
};

class Forbidden_SRS
    : public Forbidden {
};

class Forbidden_STC
    : public Forbidden {
};

class Forbidden_STC2
    : public Forbidden {
};

class Forbidden_SVC
    : public Forbidden {
};

class Forbidden_None
    : public Forbidden {
};

class ForbiddenCondDecoder_BXJ
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

class ForbiddenCondDecoder_MRS_Banked_register
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_MSR_Banked_register
    : public ForbiddenCondDecoder {
};

class ForbiddenCondDecoder_MSR_register
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

class ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged
    : public ForbiddenCondDecoder {
};

class InstructionBarrier_ISB
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

class MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_0_MOVT
    : public MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_0 {
};

class MOVW_cccc00110000iiiiddddiiiiiiiiiiii_case_0_MOVW
    : public MOVW_cccc00110000iiiiddddiiiiiiiiiiii_case_0 {
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

class MoveImmediate12ToApsr_MSR_immediate
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

class PreloadRegisterImm12Op_PLD_PLDW_immediate
    : public PreloadRegisterImm12Op {
};

class PreloadRegisterImm12Op_PLD_literal
    : public PreloadRegisterImm12Op {
};

class PreloadRegisterImm12Op_PLI_immediate_literal
    : public PreloadRegisterImm12Op {
};

class PreloadRegisterPairOp_PLD_PLDW_register
    : public PreloadRegisterPairOp {
};

class PreloadRegisterPairOp_PLI_register
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

class Unpredictable_None
    : public Unpredictable {
};

class VcvtPtAndFixedPoint_FloatingPoint_VCVT_between_floating_point_and_fixed_point_Floating_point
    : public VcvtPtAndFixedPoint_FloatingPoint {
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

class Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_ADC_immediate
    : public Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_AND_immediate
    : public Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_EOR_immediate
    : public Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSB_immediate
    : public Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSC_immediate
    : public Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_SBC_immediate
    : public Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADC_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADD_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_AND_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_BIC_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_EOR_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ORR_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSB_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSC_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SBC_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SUB_register
    : public Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADC_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADD_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_AND_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_BIC_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_EOR_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ORR_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSB_register_shfited_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSC_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SBC_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SUB_register_shifted_register
    : public Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 {
};

class Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_ADD_immediate
    : public Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_SUB_immediate
    : public Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A1
    : public Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1 {
};

class Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A2
    : public Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1 {
};

class Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_ASR_immediate
    : public Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 {
};

class Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_LSR_immediate
    : public Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 {
};

class Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MOV_register
    : public Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 {
};

class Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MVN_register
    : public Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 {
};

class Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_RRX
    : public Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 {
};

class Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ASR_register
    : public Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 {
};

class Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSL_register
    : public Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 {
};

class Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSR_register
    : public Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 {
};

class Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_MVN_register_shifted_register
    : public Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 {
};

class Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ROR_register
    : public Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 {
};

class Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1_BFC
    : public Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1 {
};

class Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1_BFI
    : public Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1 {
};

class Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1_BIC_immediate
    : public Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1_BKPT
    : public Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_DBG
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MSR_immediate
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SEV
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFE
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFI
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_None
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_immediate
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_literal
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SVC
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_register_cccc000100101111111111110011mmmm_case_1_BLX_register
    : public Actual_BLX_register_cccc000100101111111111110011mmmm_case_1 {
};

class Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1_BL_BLX_immediate
    : public Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1_B
    : public Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_Bx_cccc000100101111111111110001mmmm_case_1_Bx
    : public Actual_Bx_cccc000100101111111111110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_CLZ
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_RBIT
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV16
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REVSH
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT16
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB16
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTH
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT16
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB16
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTH
    : public Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 {
};

class Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMN_immediate
    : public Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1 {
};

class Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMP_immediate
    : public Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1 {
};

class Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_TEQ_immediate
    : public Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1 {
};

class Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMN_register
    : public Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1 {
};

class Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMP_register
    : public Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1 {
};

class Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TEQ_register
    : public Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1 {
};

class Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TST_register
    : public Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1 {
};

class Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMN_register_shifted_register
    : public Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1 {
};

class Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMP_register_shifted_register
    : public Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1 {
};

class Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TEQ_register_shifted_register
    : public Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1 {
};

class Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TST_register_shifted_register
    : public Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1 {
};

class Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDA_LDMFA
    : public Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1 {
};

class Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDB_LDMEA
    : public Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1 {
};

class Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMIB_LDMED
    : public Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1 {
};

class Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDM_LDMIA_LDMFD
    : public Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1 {
};

class Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1_LDRB_immediate
    : public Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1 {
};

class Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1_LDRB_literal
    : public Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1 {
};

class Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1_LDRB_register
    : public Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1 {
};

class Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1_LDRD_immediate
    : public Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1 {
};

class Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1_LDRD_literal
    : public Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1 {
};

class Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1_LDRD_register
    : public Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1 {
};

class Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREX
    : public Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1 {
};

class Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREXB
    : public Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1 {
};

class Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1_STREXH
    : public Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1 {
};

class Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1_LDREXD
    : public Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1 {
};

class Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRH_immediate
    : public Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1 {
};

class Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSB_immediate
    : public Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1 {
};

class Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSH_immediate
    : public Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1 {
};

class Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRH_literal
    : public Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1 {
};

class Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSB_literal
    : public Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1 {
};

class Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRH_register
    : public Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1 {
};

class Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSB_register
    : public Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1 {
};

class Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSH_register
    : public Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1 {
};

class Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1_LDR_immediate
    : public Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1 {
};

class Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1_LDR_literal
    : public Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1 {
};

class Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1_LDR_register
    : public Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1 {
};

class Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_LSL_immediate
    : public Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1 {
};

class Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_ROR_immediate
    : public Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1 {
};

class Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1_MLA_A1
    : public Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1 {
};

class Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_MLS_A1
    : public Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1 {
};

class Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLABB_SMLABT_SMLATB_SMLATT
    : public Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1 {
};

class Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLAWB_SMLAWT
    : public Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1 {
};

class Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVT
    : public Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1 {
};

class Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVW
    : public Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1 {
};

class Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MOV_immediate_A1
    : public Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1 {
};

class Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MVN_immediate
    : public Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1 {
};

class Actual_MRS_cccc00010r001111dddd000000000000_case_1_MRS
    : public Actual_MRS_cccc00010r001111dddd000000000000_case_1 {
};

class Actual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1_MSR_immediate
    : public Actual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1 {
};

class Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1_MSR_register
    : public Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1 {
};

class Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1_MUL_A1
    : public Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1 {
};

class Actual_NOP_cccc0011001000001111000000000000_case_1_NOP
    : public Actual_NOP_cccc0011001000001111000000000000_case_1 {
};

class Actual_NOP_cccc0011001000001111000000000000_case_1_YIELD
    : public Actual_NOP_cccc0011001000001111000000000000_case_1 {
};

class Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1_ORR_immediate
    : public Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_PKH
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QASX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDADD
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDSUB
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSAX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SASX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SEL
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHASX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSAX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSAX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSSUB16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSUB8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UASX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHASX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSAX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQASX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSAX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USAX
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB16
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB8
    : public Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 {
};

class Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_SBFX
    : public Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1 {
};

class Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_UBFX
    : public Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1 {
};

class Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SDIV
    : public Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 {
};

class Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMMUL
    : public Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 {
};

class Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUAD
    : public Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 {
};

class Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUSD
    : public Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 {
};

class Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_UDIV
    : public Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 {
};

class Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLAD
    : public Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 {
};

class Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLSD
    : public Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 {
};

class Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLA
    : public Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 {
};

class Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLS
    : public Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 {
};

class Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_USADA8
    : public Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 {
};

class Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_SMLALBB_SMLALBT_SMLALTB_SMLALTT
    : public Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1 {
};

class Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_UMAAL_A1
    : public Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1 {
};

class Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLALD
    : public Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1 {
};

class Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLSLD
    : public Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1 {
};

class Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_SMLAL_A1
    : public Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1 {
};

class Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_UMLAL_A1
    : public Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1 {
};

class Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULBB_SMULBT_SMULTB_SMULTT
    : public Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1 {
};

class Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULWB_SMULWT
    : public Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1 {
};

class Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_USAD8
    : public Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1 {
};

class Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_SMULL_A1
    : public Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1 {
};

class Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_UMULL_A1
    : public Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1 {
};

class Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDA_STMED
    : public Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1 {
};

class Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDB_STMFD
    : public Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1 {
};

class Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMIB_STMFA
    : public Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1 {
};

class Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STM_STMIA_STMEA
    : public Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1 {
};

class Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1_STRB_immediate
    : public Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1 {
};

class Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1_STRB_register
    : public Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1 {
};

class Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1_STRD_immediate
    : public Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1 {
};

class Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1_STRD_register
    : public Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1 {
};

class Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREX
    : public Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1 {
};

class Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXB
    : public Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1 {
};

class Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXH
    : public Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1 {
};

class Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1_STREXD
    : public Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1 {
};

class Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1_STRH_immediate
    : public Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1 {
};

class Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1_STRH_register
    : public Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1 {
};

class Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1_STR_immediate
    : public Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1 {
};

class Actual_STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_1_STR_register
    : public Actual_STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_1 {
};

class Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB
    : public Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 {
};

class Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB16
    : public Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 {
};

class Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAH
    : public Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 {
};

class Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB
    : public Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 {
};

class Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB16
    : public Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 {
};

class Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAH
    : public Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 {
};

class Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1_TST_immediate
    : public Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1 {
};

class Actual_Unnamed_case_1_None
    : public Actual_Unnamed_case_1 {
};

class Forbidden_BXJ
    : public Forbidden {
};

class Forbidden_ERET
    : public Forbidden {
};

class Forbidden_HVC
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

class Forbidden_SMC
    : public Forbidden {
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

class NamedCondDecoder_NOP
    : public NamedClassDecoder {
 public:
  NamedCondDecoder_NOP()
    : NamedClassDecoder(decoder_, "CondDecoder NOP")
  {}

 private:
  nacl_arm_dec::CondDecoder_NOP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondDecoder_NOP);
};

class NamedCondDecoder_YIELD
    : public NamedClassDecoder {
 public:
  NamedCondDecoder_YIELD()
    : NamedClassDecoder(decoder_, "CondDecoder YIELD")
  {}

 private:
  nacl_arm_dec::CondDecoder_YIELD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondDecoder_YIELD);
};

class NamedCondVfpOp_VABS
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VABS()
    : NamedClassDecoder(decoder_, "CondVfpOp VABS")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VABS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VABS);
};

class NamedCondVfpOp_VADD_floating_point
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VADD_floating_point()
    : NamedClassDecoder(decoder_, "CondVfpOp VADD_floating_point")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VADD_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VADD_floating_point);
};

class NamedCondVfpOp_VCMP_VCMPE
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VCMP_VCMPE()
    : NamedClassDecoder(decoder_, "CondVfpOp VCMP_VCMPE")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VCMP_VCMPE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VCMP_VCMPE);
};

class NamedCondVfpOp_VCVTB_VCVTT
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VCVTB_VCVTT()
    : NamedClassDecoder(decoder_, "CondVfpOp VCVTB_VCVTT")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VCVTB_VCVTT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VCVTB_VCVTT);
};

class NamedCondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point()
    : NamedClassDecoder(decoder_, "CondVfpOp VCVT_VCVTR_between_floating_point_and_integer_Floating_point")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point);
};

class NamedCondVfpOp_VCVT_between_double_precision_and_single_precision
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VCVT_between_double_precision_and_single_precision()
    : NamedClassDecoder(decoder_, "CondVfpOp VCVT_between_double_precision_and_single_precision")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VCVT_between_double_precision_and_single_precision decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VCVT_between_double_precision_and_single_precision);
};

class NamedCondVfpOp_VDIV
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VDIV()
    : NamedClassDecoder(decoder_, "CondVfpOp VDIV")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VDIV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VDIV);
};

class NamedCondVfpOp_VFMA_VFMS
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VFMA_VFMS()
    : NamedClassDecoder(decoder_, "CondVfpOp VFMA_VFMS")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VFMA_VFMS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VFMA_VFMS);
};

class NamedCondVfpOp_VFNMA_VFNMS
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VFNMA_VFNMS()
    : NamedClassDecoder(decoder_, "CondVfpOp VFNMA_VFNMS")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VFNMA_VFNMS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VFNMA_VFNMS);
};

class NamedCondVfpOp_VMLA_VMLS_floating_point
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VMLA_VMLS_floating_point()
    : NamedClassDecoder(decoder_, "CondVfpOp VMLA_VMLS_floating_point")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VMLA_VMLS_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VMLA_VMLS_floating_point);
};

class NamedCondVfpOp_VMOV_immediate
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VMOV_immediate()
    : NamedClassDecoder(decoder_, "CondVfpOp VMOV_immediate")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VMOV_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VMOV_immediate);
};

class NamedCondVfpOp_VMOV_register
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VMOV_register()
    : NamedClassDecoder(decoder_, "CondVfpOp VMOV_register")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VMOV_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VMOV_register);
};

class NamedCondVfpOp_VMUL_floating_point
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VMUL_floating_point()
    : NamedClassDecoder(decoder_, "CondVfpOp VMUL_floating_point")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VMUL_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VMUL_floating_point);
};

class NamedCondVfpOp_VNEG
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VNEG()
    : NamedClassDecoder(decoder_, "CondVfpOp VNEG")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VNEG decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VNEG);
};

class NamedCondVfpOp_VNMLA_VNMLS
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VNMLA_VNMLS()
    : NamedClassDecoder(decoder_, "CondVfpOp VNMLA_VNMLS")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VNMLA_VNMLS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VNMLA_VNMLS);
};

class NamedCondVfpOp_VNMUL
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VNMUL()
    : NamedClassDecoder(decoder_, "CondVfpOp VNMUL")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VNMUL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VNMUL);
};

class NamedCondVfpOp_VSQRT
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VSQRT()
    : NamedClassDecoder(decoder_, "CondVfpOp VSQRT")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VSQRT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VSQRT);
};

class NamedCondVfpOp_VSUB_floating_point
    : public NamedClassDecoder {
 public:
  NamedCondVfpOp_VSUB_floating_point()
    : NamedClassDecoder(decoder_, "CondVfpOp VSUB_floating_point")
  {}

 private:
  nacl_arm_dec::CondVfpOp_VSUB_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCondVfpOp_VSUB_floating_point);
};

class NamedDataBarrier_DMB
    : public NamedClassDecoder {
 public:
  NamedDataBarrier_DMB()
    : NamedClassDecoder(decoder_, "DataBarrier DMB")
  {}

 private:
  nacl_arm_dec::DataBarrier_DMB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDataBarrier_DMB);
};

class NamedDataBarrier_DSB
    : public NamedClassDecoder {
 public:
  NamedDataBarrier_DSB()
    : NamedClassDecoder(decoder_, "DataBarrier DSB")
  {}

 private:
  nacl_arm_dec::DataBarrier_DSB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDataBarrier_DSB);
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

class NamedForbidden_DBG
    : public NamedClassDecoder {
 public:
  NamedForbidden_DBG()
    : NamedClassDecoder(decoder_, "Forbidden DBG")
  {}

 private:
  nacl_arm_dec::Forbidden_DBG decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_DBG);
};

class NamedForbidden_MSR_immediate
    : public NamedClassDecoder {
 public:
  NamedForbidden_MSR_immediate()
    : NamedClassDecoder(decoder_, "Forbidden MSR_immediate")
  {}

 private:
  nacl_arm_dec::Forbidden_MSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MSR_immediate);
};

class NamedForbidden_SEV
    : public NamedClassDecoder {
 public:
  NamedForbidden_SEV()
    : NamedClassDecoder(decoder_, "Forbidden SEV")
  {}

 private:
  nacl_arm_dec::Forbidden_SEV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_SEV);
};

class NamedForbidden_WFE
    : public NamedClassDecoder {
 public:
  NamedForbidden_WFE()
    : NamedClassDecoder(decoder_, "Forbidden WFE")
  {}

 private:
  nacl_arm_dec::Forbidden_WFE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_WFE);
};

class NamedForbidden_WFI
    : public NamedClassDecoder {
 public:
  NamedForbidden_WFI()
    : NamedClassDecoder(decoder_, "Forbidden WFI")
  {}

 private:
  nacl_arm_dec::Forbidden_WFI decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_WFI);
};

class NamedForbidden_BLX_immediate
    : public NamedClassDecoder {
 public:
  NamedForbidden_BLX_immediate()
    : NamedClassDecoder(decoder_, "Forbidden BLX_immediate")
  {}

 private:
  nacl_arm_dec::Forbidden_BLX_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_BLX_immediate);
};

class NamedForbidden_CDP
    : public NamedClassDecoder {
 public:
  NamedForbidden_CDP()
    : NamedClassDecoder(decoder_, "Forbidden CDP")
  {}

 private:
  nacl_arm_dec::Forbidden_CDP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_CDP);
};

class NamedForbidden_CDP2
    : public NamedClassDecoder {
 public:
  NamedForbidden_CDP2()
    : NamedClassDecoder(decoder_, "Forbidden CDP2")
  {}

 private:
  nacl_arm_dec::Forbidden_CDP2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_CDP2);
};

class NamedForbidden_CLREX
    : public NamedClassDecoder {
 public:
  NamedForbidden_CLREX()
    : NamedClassDecoder(decoder_, "Forbidden CLREX")
  {}

 private:
  nacl_arm_dec::Forbidden_CLREX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_CLREX);
};

class NamedForbidden_CPS
    : public NamedClassDecoder {
 public:
  NamedForbidden_CPS()
    : NamedClassDecoder(decoder_, "Forbidden CPS")
  {}

 private:
  nacl_arm_dec::Forbidden_CPS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_CPS);
};

class NamedForbidden_LDC2_immediate
    : public NamedClassDecoder {
 public:
  NamedForbidden_LDC2_immediate()
    : NamedClassDecoder(decoder_, "Forbidden LDC2_immediate")
  {}

 private:
  nacl_arm_dec::Forbidden_LDC2_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_LDC2_immediate);
};

class NamedForbidden_LDC2_literal
    : public NamedClassDecoder {
 public:
  NamedForbidden_LDC2_literal()
    : NamedClassDecoder(decoder_, "Forbidden LDC2_literal")
  {}

 private:
  nacl_arm_dec::Forbidden_LDC2_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_LDC2_literal);
};

class NamedForbidden_LDC_immediate
    : public NamedClassDecoder {
 public:
  NamedForbidden_LDC_immediate()
    : NamedClassDecoder(decoder_, "Forbidden LDC_immediate")
  {}

 private:
  nacl_arm_dec::Forbidden_LDC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_LDC_immediate);
};

class NamedForbidden_LDC_literal
    : public NamedClassDecoder {
 public:
  NamedForbidden_LDC_literal()
    : NamedClassDecoder(decoder_, "Forbidden LDC_literal")
  {}

 private:
  nacl_arm_dec::Forbidden_LDC_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_LDC_literal);
};

class NamedForbidden_MCR
    : public NamedClassDecoder {
 public:
  NamedForbidden_MCR()
    : NamedClassDecoder(decoder_, "Forbidden MCR")
  {}

 private:
  nacl_arm_dec::Forbidden_MCR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MCR);
};

class NamedForbidden_MCR2
    : public NamedClassDecoder {
 public:
  NamedForbidden_MCR2()
    : NamedClassDecoder(decoder_, "Forbidden MCR2")
  {}

 private:
  nacl_arm_dec::Forbidden_MCR2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MCR2);
};

class NamedForbidden_MCRR
    : public NamedClassDecoder {
 public:
  NamedForbidden_MCRR()
    : NamedClassDecoder(decoder_, "Forbidden MCRR")
  {}

 private:
  nacl_arm_dec::Forbidden_MCRR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MCRR);
};

class NamedForbidden_MCRR2
    : public NamedClassDecoder {
 public:
  NamedForbidden_MCRR2()
    : NamedClassDecoder(decoder_, "Forbidden MCRR2")
  {}

 private:
  nacl_arm_dec::Forbidden_MCRR2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MCRR2);
};

class NamedForbidden_MRC
    : public NamedClassDecoder {
 public:
  NamedForbidden_MRC()
    : NamedClassDecoder(decoder_, "Forbidden MRC")
  {}

 private:
  nacl_arm_dec::Forbidden_MRC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MRC);
};

class NamedForbidden_MRC2
    : public NamedClassDecoder {
 public:
  NamedForbidden_MRC2()
    : NamedClassDecoder(decoder_, "Forbidden MRC2")
  {}

 private:
  nacl_arm_dec::Forbidden_MRC2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MRC2);
};

class NamedForbidden_MRRC
    : public NamedClassDecoder {
 public:
  NamedForbidden_MRRC()
    : NamedClassDecoder(decoder_, "Forbidden MRRC")
  {}

 private:
  nacl_arm_dec::Forbidden_MRRC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MRRC);
};

class NamedForbidden_MRRC2
    : public NamedClassDecoder {
 public:
  NamedForbidden_MRRC2()
    : NamedClassDecoder(decoder_, "Forbidden MRRC2")
  {}

 private:
  nacl_arm_dec::Forbidden_MRRC2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_MRRC2);
};

class NamedForbidden_RFE
    : public NamedClassDecoder {
 public:
  NamedForbidden_RFE()
    : NamedClassDecoder(decoder_, "Forbidden RFE")
  {}

 private:
  nacl_arm_dec::Forbidden_RFE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_RFE);
};

class NamedForbidden_SETEND
    : public NamedClassDecoder {
 public:
  NamedForbidden_SETEND()
    : NamedClassDecoder(decoder_, "Forbidden SETEND")
  {}

 private:
  nacl_arm_dec::Forbidden_SETEND decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_SETEND);
};

class NamedForbidden_SRS
    : public NamedClassDecoder {
 public:
  NamedForbidden_SRS()
    : NamedClassDecoder(decoder_, "Forbidden SRS")
  {}

 private:
  nacl_arm_dec::Forbidden_SRS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_SRS);
};

class NamedForbidden_STC
    : public NamedClassDecoder {
 public:
  NamedForbidden_STC()
    : NamedClassDecoder(decoder_, "Forbidden STC")
  {}

 private:
  nacl_arm_dec::Forbidden_STC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_STC);
};

class NamedForbidden_STC2
    : public NamedClassDecoder {
 public:
  NamedForbidden_STC2()
    : NamedClassDecoder(decoder_, "Forbidden STC2")
  {}

 private:
  nacl_arm_dec::Forbidden_STC2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_STC2);
};

class NamedForbidden_SVC
    : public NamedClassDecoder {
 public:
  NamedForbidden_SVC()
    : NamedClassDecoder(decoder_, "Forbidden SVC")
  {}

 private:
  nacl_arm_dec::Forbidden_SVC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedForbidden_SVC);
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

class NamedInstructionBarrier_ISB
    : public NamedClassDecoder {
 public:
  NamedInstructionBarrier_ISB()
    : NamedClassDecoder(decoder_, "InstructionBarrier ISB")
  {}

 private:
  nacl_arm_dec::InstructionBarrier_ISB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedInstructionBarrier_ISB);
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

class NamedMOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_0_MOVT
    : public NamedClassDecoder {
 public:
  NamedMOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_0_MOVT()
    : NamedClassDecoder(decoder_, "MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_0 MOVT")
  {}

 private:
  nacl_arm_dec::MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_0_MOVT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_0_MOVT);
};

class NamedMOVW_cccc00110000iiiiddddiiiiiiiiiiii_case_0_MOVW
    : public NamedClassDecoder {
 public:
  NamedMOVW_cccc00110000iiiiddddiiiiiiiiiiii_case_0_MOVW()
    : NamedClassDecoder(decoder_, "MOVW_cccc00110000iiiiddddiiiiiiiiiiii_case_0 MOVW")
  {}

 private:
  nacl_arm_dec::MOVW_cccc00110000iiiiddddiiiiiiiiiiii_case_0_MOVW decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMOVW_cccc00110000iiiiddddiiiiiiiiiiii_case_0_MOVW);
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

class NamedMoveImmediate12ToApsr_MSR_immediate
    : public NamedClassDecoder {
 public:
  NamedMoveImmediate12ToApsr_MSR_immediate()
    : NamedClassDecoder(decoder_, "MoveImmediate12ToApsr MSR_immediate")
  {}

 private:
  nacl_arm_dec::MoveImmediate12ToApsr_MSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMoveImmediate12ToApsr_MSR_immediate);
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

class NamedPreloadRegisterImm12Op_PLD_PLDW_immediate
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterImm12Op_PLD_PLDW_immediate()
    : NamedClassDecoder(decoder_, "PreloadRegisterImm12Op PLD_PLDW_immediate")
  {}

 private:
  nacl_arm_dec::PreloadRegisterImm12Op_PLD_PLDW_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterImm12Op_PLD_PLDW_immediate);
};

class NamedPreloadRegisterImm12Op_PLD_literal
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterImm12Op_PLD_literal()
    : NamedClassDecoder(decoder_, "PreloadRegisterImm12Op PLD_literal")
  {}

 private:
  nacl_arm_dec::PreloadRegisterImm12Op_PLD_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterImm12Op_PLD_literal);
};

class NamedPreloadRegisterImm12Op_PLI_immediate_literal
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterImm12Op_PLI_immediate_literal()
    : NamedClassDecoder(decoder_, "PreloadRegisterImm12Op PLI_immediate_literal")
  {}

 private:
  nacl_arm_dec::PreloadRegisterImm12Op_PLI_immediate_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterImm12Op_PLI_immediate_literal);
};

class NamedPreloadRegisterPairOp_PLD_PLDW_register
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterPairOp_PLD_PLDW_register()
    : NamedClassDecoder(decoder_, "PreloadRegisterPairOp PLD_PLDW_register")
  {}

 private:
  nacl_arm_dec::PreloadRegisterPairOp_PLD_PLDW_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterPairOp_PLD_PLDW_register);
};

class NamedPreloadRegisterPairOp_PLI_register
    : public NamedClassDecoder {
 public:
  NamedPreloadRegisterPairOp_PLI_register()
    : NamedClassDecoder(decoder_, "PreloadRegisterPairOp PLI_register")
  {}

 private:
  nacl_arm_dec::PreloadRegisterPairOp_PLI_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPreloadRegisterPairOp_PLI_register);
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

class NamedUnpredictable_None
    : public NamedClassDecoder {
 public:
  NamedUnpredictable_None()
    : NamedClassDecoder(decoder_, "Unpredictable None")
  {}

 private:
  nacl_arm_dec::Unpredictable_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnpredictable_None);
};

class NamedVcvtPtAndFixedPoint_FloatingPoint_VCVT_between_floating_point_and_fixed_point_Floating_point
    : public NamedClassDecoder {
 public:
  NamedVcvtPtAndFixedPoint_FloatingPoint_VCVT_between_floating_point_and_fixed_point_Floating_point()
    : NamedClassDecoder(decoder_, "VcvtPtAndFixedPoint_FloatingPoint VCVT_between_floating_point_and_fixed_point_Floating_point")
  {}

 private:
  nacl_arm_dec::VcvtPtAndFixedPoint_FloatingPoint_VCVT_between_floating_point_and_fixed_point_Floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVcvtPtAndFixedPoint_FloatingPoint_VCVT_between_floating_point_and_fixed_point_Floating_point);
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

class NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_ADC_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_ADC_immediate()
    : NamedClassDecoder(decoder_, "Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 ADC_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_ADC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_ADC_immediate);
};

class NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_AND_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_AND_immediate()
    : NamedClassDecoder(decoder_, "Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 AND_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_AND_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_AND_immediate);
};

class NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_EOR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_EOR_immediate()
    : NamedClassDecoder(decoder_, "Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 EOR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_EOR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_EOR_immediate);
};

class NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSB_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSB_immediate()
    : NamedClassDecoder(decoder_, "Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 RSB_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSB_immediate);
};

class NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSC_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSC_immediate()
    : NamedClassDecoder(decoder_, "Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 RSC_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSC_immediate);
};

class NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_SBC_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_SBC_immediate()
    : NamedClassDecoder(decoder_, "Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1 SBC_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_SBC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_SBC_immediate);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADC_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADC_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 ADC_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADC_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADD_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADD_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 ADD_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADD_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADD_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_AND_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_AND_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 AND_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_AND_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_AND_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_BIC_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_BIC_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 BIC_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_BIC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_BIC_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_EOR_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_EOR_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 EOR_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_EOR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_EOR_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ORR_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ORR_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 ORR_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ORR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ORR_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSB_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSB_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 RSB_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSB_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSC_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSC_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 RSC_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSC_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SBC_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SBC_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 SBC_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SBC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SBC_register);
};

class NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SUB_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SUB_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1 SUB_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SUB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SUB_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADC_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 ADC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADC_register_shifted_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADD_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADD_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 ADD_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADD_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ADD_register_shifted_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_AND_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_AND_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 AND_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_AND_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_AND_register_shifted_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_BIC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_BIC_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 BIC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_BIC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_BIC_register_shifted_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_EOR_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_EOR_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 EOR_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_EOR_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_EOR_register_shifted_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ORR_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ORR_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 ORR_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ORR_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_ORR_register_shifted_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSB_register_shfited_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSB_register_shfited_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 RSB_register_shfited_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSB_register_shfited_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSB_register_shfited_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSC_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 RSC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_RSC_register_shifted_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SBC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SBC_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 SBC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SBC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SBC_register_shifted_register);
};

class NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SUB_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SUB_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1 SUB_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SUB_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1_SUB_register_shifted_register);
};

class NamedActual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_ADD_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_ADD_immediate()
    : NamedClassDecoder(decoder_, "Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1 ADD_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_ADD_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_ADD_immediate);
};

class NamedActual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_SUB_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_SUB_immediate()
    : NamedClassDecoder(decoder_, "Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1 SUB_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_SUB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_SUB_immediate);
};

class NamedActual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A1
    : public NamedClassDecoder {
 public:
  NamedActual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A1()
    : NamedClassDecoder(decoder_, "Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1 ADR_A1")
  {}

 private:
  nacl_arm_dec::Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A1);
};

class NamedActual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A2
    : public NamedClassDecoder {
 public:
  NamedActual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A2()
    : NamedClassDecoder(decoder_, "Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1 ADR_A2")
  {}

 private:
  nacl_arm_dec::Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A2);
};

class NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_ASR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_ASR_immediate()
    : NamedClassDecoder(decoder_, "Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 ASR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_ASR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_ASR_immediate);
};

class NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_LSR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_LSR_immediate()
    : NamedClassDecoder(decoder_, "Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 LSR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_LSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_LSR_immediate);
};

class NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MOV_register
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MOV_register()
    : NamedClassDecoder(decoder_, "Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 MOV_register")
  {}

 private:
  nacl_arm_dec::Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MOV_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MOV_register);
};

class NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MVN_register
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MVN_register()
    : NamedClassDecoder(decoder_, "Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 MVN_register")
  {}

 private:
  nacl_arm_dec::Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MVN_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MVN_register);
};

class NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_RRX
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_RRX()
    : NamedClassDecoder(decoder_, "Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1 RRX")
  {}

 private:
  nacl_arm_dec::Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_RRX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_RRX);
};

class NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ASR_register
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ASR_register()
    : NamedClassDecoder(decoder_, "Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 ASR_register")
  {}

 private:
  nacl_arm_dec::Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ASR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ASR_register);
};

class NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSL_register
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSL_register()
    : NamedClassDecoder(decoder_, "Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 LSL_register")
  {}

 private:
  nacl_arm_dec::Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSL_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSL_register);
};

class NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSR_register
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSR_register()
    : NamedClassDecoder(decoder_, "Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 LSR_register")
  {}

 private:
  nacl_arm_dec::Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_LSR_register);
};

class NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_MVN_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_MVN_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 MVN_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_MVN_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_MVN_register_shifted_register);
};

class NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ROR_register
    : public NamedClassDecoder {
 public:
  NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ROR_register()
    : NamedClassDecoder(decoder_, "Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1 ROR_register")
  {}

 private:
  nacl_arm_dec::Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ROR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1_ROR_register);
};

class NamedActual_BFC_cccc0111110mmmmmddddlllll0011111_case_1_BFC
    : public NamedClassDecoder {
 public:
  NamedActual_BFC_cccc0111110mmmmmddddlllll0011111_case_1_BFC()
    : NamedClassDecoder(decoder_, "Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1 BFC")
  {}

 private:
  nacl_arm_dec::Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1_BFC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BFC_cccc0111110mmmmmddddlllll0011111_case_1_BFC);
};

class NamedActual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1_BFI
    : public NamedClassDecoder {
 public:
  NamedActual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1_BFI()
    : NamedClassDecoder(decoder_, "Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1 BFI")
  {}

 private:
  nacl_arm_dec::Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1_BFI decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1_BFI);
};

class NamedActual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1_BIC_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1_BIC_immediate()
    : NamedClassDecoder(decoder_, "Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1 BIC_immediate")
  {}

 private:
  nacl_arm_dec::Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1_BIC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1_BIC_immediate);
};

class NamedActual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1_BKPT
    : public NamedClassDecoder {
 public:
  NamedActual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1_BKPT()
    : NamedClassDecoder(decoder_, "Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1 BKPT")
  {}

 private:
  nacl_arm_dec::Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1_BKPT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1_BKPT);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_DBG
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_DBG()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 DBG")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_DBG decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_DBG);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MSR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MSR_immediate()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MSR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MSR_immediate);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SEV
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SEV()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 SEV")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SEV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SEV);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFE
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFE()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 WFE")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFE);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFI
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFI()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 WFI")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFI decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_WFI);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_None
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_None()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 None")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_None);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 CDP")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_immediate()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 LDC_immediate")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_immediate);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_literal
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_literal()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 LDC_literal")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC_literal);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MCR")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MCRR")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MRC")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MRRC")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 STC")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SVC
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SVC()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 SVC")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SVC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SVC);
};

class NamedActual_BLX_register_cccc000100101111111111110011mmmm_case_1_BLX_register
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_register_cccc000100101111111111110011mmmm_case_1_BLX_register()
    : NamedClassDecoder(decoder_, "Actual_BLX_register_cccc000100101111111111110011mmmm_case_1 BLX_register")
  {}

 private:
  nacl_arm_dec::Actual_BLX_register_cccc000100101111111111110011mmmm_case_1_BLX_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_register_cccc000100101111111111110011mmmm_case_1_BLX_register);
};

class NamedActual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1_BL_BLX_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1_BL_BLX_immediate()
    : NamedClassDecoder(decoder_, "Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1 BL_BLX_immediate")
  {}

 private:
  nacl_arm_dec::Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1_BL_BLX_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1_BL_BLX_immediate);
};

class NamedActual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1_B
    : public NamedClassDecoder {
 public:
  NamedActual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1_B()
    : NamedClassDecoder(decoder_, "Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1 B")
  {}

 private:
  nacl_arm_dec::Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1_B decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1_B);
};

class NamedActual_Bx_cccc000100101111111111110001mmmm_case_1_Bx
    : public NamedClassDecoder {
 public:
  NamedActual_Bx_cccc000100101111111111110001mmmm_case_1_Bx()
    : NamedClassDecoder(decoder_, "Actual_Bx_cccc000100101111111111110001mmmm_case_1 Bx")
  {}

 private:
  nacl_arm_dec::Actual_Bx_cccc000100101111111111110001mmmm_case_1_Bx decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_Bx_cccc000100101111111111110001mmmm_case_1_Bx);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_CLZ
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_CLZ()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 CLZ")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_CLZ decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_CLZ);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_RBIT
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_RBIT()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 RBIT")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_RBIT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_RBIT);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 REV")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV16
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV16()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 REV16")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REV16);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REVSH
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REVSH()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 REVSH")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REVSH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_REVSH);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 SSAT")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT16
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT16()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 SSAT16")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SSAT16);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 SXTB")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB16
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB16()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 SXTB16")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTB16);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTH
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTH()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 SXTH")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_SXTH);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 USAT")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT16
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT16()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 USAT16")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_USAT16);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 UXTB")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB16
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB16()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 UXTB16")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTB16);
};

class NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTH
    : public NamedClassDecoder {
 public:
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTH()
    : NamedClassDecoder(decoder_, "Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1 UXTH")
  {}

 private:
  nacl_arm_dec::Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_UXTH);
};

class NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMN_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMN_immediate()
    : NamedClassDecoder(decoder_, "Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1 CMN_immediate")
  {}

 private:
  nacl_arm_dec::Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMN_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMN_immediate);
};

class NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMP_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMP_immediate()
    : NamedClassDecoder(decoder_, "Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1 CMP_immediate")
  {}

 private:
  nacl_arm_dec::Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMP_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMP_immediate);
};

class NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_TEQ_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_TEQ_immediate()
    : NamedClassDecoder(decoder_, "Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1 TEQ_immediate")
  {}

 private:
  nacl_arm_dec::Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_TEQ_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_TEQ_immediate);
};

class NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMN_register
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMN_register()
    : NamedClassDecoder(decoder_, "Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1 CMN_register")
  {}

 private:
  nacl_arm_dec::Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMN_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMN_register);
};

class NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMP_register
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMP_register()
    : NamedClassDecoder(decoder_, "Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1 CMP_register")
  {}

 private:
  nacl_arm_dec::Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMP_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMP_register);
};

class NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TEQ_register
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TEQ_register()
    : NamedClassDecoder(decoder_, "Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1 TEQ_register")
  {}

 private:
  nacl_arm_dec::Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TEQ_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TEQ_register);
};

class NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TST_register
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TST_register()
    : NamedClassDecoder(decoder_, "Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1 TST_register")
  {}

 private:
  nacl_arm_dec::Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TST_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TST_register);
};

class NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMN_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMN_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1 CMN_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMN_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMN_register_shifted_register);
};

class NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMP_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMP_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1 CMP_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMP_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_CMP_register_shifted_register);
};

class NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TEQ_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TEQ_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1 TEQ_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TEQ_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TEQ_register_shifted_register);
};

class NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TST_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TST_register_shifted_register()
    : NamedClassDecoder(decoder_, "Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1 TST_register_shifted_register")
  {}

 private:
  nacl_arm_dec::Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TST_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1_TST_register_shifted_register);
};

class NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDA_LDMFA
    : public NamedClassDecoder {
 public:
  NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDA_LDMFA()
    : NamedClassDecoder(decoder_, "Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1 LDMDA_LDMFA")
  {}

 private:
  nacl_arm_dec::Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDA_LDMFA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDA_LDMFA);
};

class NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDB_LDMEA
    : public NamedClassDecoder {
 public:
  NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDB_LDMEA()
    : NamedClassDecoder(decoder_, "Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1 LDMDB_LDMEA")
  {}

 private:
  nacl_arm_dec::Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDB_LDMEA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMDB_LDMEA);
};

class NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMIB_LDMED
    : public NamedClassDecoder {
 public:
  NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMIB_LDMED()
    : NamedClassDecoder(decoder_, "Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1 LDMIB_LDMED")
  {}

 private:
  nacl_arm_dec::Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMIB_LDMED decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDMIB_LDMED);
};

class NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDM_LDMIA_LDMFD
    : public NamedClassDecoder {
 public:
  NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDM_LDMIA_LDMFD()
    : NamedClassDecoder(decoder_, "Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1 LDM_LDMIA_LDMFD")
  {}

 private:
  nacl_arm_dec::Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDM_LDMIA_LDMFD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1_LDM_LDMIA_LDMFD);
};

class NamedActual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1_LDRB_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1_LDRB_immediate()
    : NamedClassDecoder(decoder_, "Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1 LDRB_immediate")
  {}

 private:
  nacl_arm_dec::Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1_LDRB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1_LDRB_immediate);
};

class NamedActual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1_LDRB_literal
    : public NamedClassDecoder {
 public:
  NamedActual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1_LDRB_literal()
    : NamedClassDecoder(decoder_, "Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1 LDRB_literal")
  {}

 private:
  nacl_arm_dec::Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1_LDRB_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1_LDRB_literal);
};

class NamedActual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1_LDRB_register
    : public NamedClassDecoder {
 public:
  NamedActual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1_LDRB_register()
    : NamedClassDecoder(decoder_, "Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1 LDRB_register")
  {}

 private:
  nacl_arm_dec::Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1_LDRB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1_LDRB_register);
};

class NamedActual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1_LDRD_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1_LDRD_immediate()
    : NamedClassDecoder(decoder_, "Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1 LDRD_immediate")
  {}

 private:
  nacl_arm_dec::Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1_LDRD_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1_LDRD_immediate);
};

class NamedActual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1_LDRD_literal
    : public NamedClassDecoder {
 public:
  NamedActual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1_LDRD_literal()
    : NamedClassDecoder(decoder_, "Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1 LDRD_literal")
  {}

 private:
  nacl_arm_dec::Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1_LDRD_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1_LDRD_literal);
};

class NamedActual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1_LDRD_register
    : public NamedClassDecoder {
 public:
  NamedActual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1_LDRD_register()
    : NamedClassDecoder(decoder_, "Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1 LDRD_register")
  {}

 private:
  nacl_arm_dec::Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1_LDRD_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1_LDRD_register);
};

class NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREX
    : public NamedClassDecoder {
 public:
  NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREX()
    : NamedClassDecoder(decoder_, "Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1 LDREX")
  {}

 private:
  nacl_arm_dec::Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREX);
};

class NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREXB
    : public NamedClassDecoder {
 public:
  NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREXB()
    : NamedClassDecoder(decoder_, "Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1 LDREXB")
  {}

 private:
  nacl_arm_dec::Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREXB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_LDREXB);
};

class NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_STREXH
    : public NamedClassDecoder {
 public:
  NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_STREXH()
    : NamedClassDecoder(decoder_, "Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1 STREXH")
  {}

 private:
  nacl_arm_dec::Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1_STREXH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDREXB_cccc00011101nnnntttt111110011111_case_1_STREXH);
};

class NamedActual_LDREXD_cccc00011011nnnntttt111110011111_case_1_LDREXD
    : public NamedClassDecoder {
 public:
  NamedActual_LDREXD_cccc00011011nnnntttt111110011111_case_1_LDREXD()
    : NamedClassDecoder(decoder_, "Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1 LDREXD")
  {}

 private:
  nacl_arm_dec::Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1_LDREXD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDREXD_cccc00011011nnnntttt111110011111_case_1_LDREXD);
};

class NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRH_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRH_immediate()
    : NamedClassDecoder(decoder_, "Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1 LDRH_immediate")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRH_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRH_immediate);
};

class NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSB_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSB_immediate()
    : NamedClassDecoder(decoder_, "Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1 LDRSB_immediate")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSB_immediate);
};

class NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSH_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSH_immediate()
    : NamedClassDecoder(decoder_, "Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1 LDRSH_immediate")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSH_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1_LDRSH_immediate);
};

class NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRH_literal
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRH_literal()
    : NamedClassDecoder(decoder_, "Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1 LDRH_literal")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRH_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRH_literal);
};

class NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSB_literal
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSB_literal()
    : NamedClassDecoder(decoder_, "Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1 LDRSB_literal")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSB_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSB_literal);
};

class NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRH_register
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRH_register()
    : NamedClassDecoder(decoder_, "Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1 LDRH_register")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRH_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRH_register);
};

class NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSB_register
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSB_register()
    : NamedClassDecoder(decoder_, "Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1 LDRSB_register")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSB_register);
};

class NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSH_register
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSH_register()
    : NamedClassDecoder(decoder_, "Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1 LDRSH_register")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSH_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1_LDRSH_register);
};

class NamedActual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1_LDR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1_LDR_immediate()
    : NamedClassDecoder(decoder_, "Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1 LDR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1_LDR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1_LDR_immediate);
};

class NamedActual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1_LDR_literal
    : public NamedClassDecoder {
 public:
  NamedActual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1_LDR_literal()
    : NamedClassDecoder(decoder_, "Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1 LDR_literal")
  {}

 private:
  nacl_arm_dec::Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1_LDR_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1_LDR_literal);
};

class NamedActual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1_LDR_register
    : public NamedClassDecoder {
 public:
  NamedActual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1_LDR_register()
    : NamedClassDecoder(decoder_, "Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1 LDR_register")
  {}

 private:
  nacl_arm_dec::Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1_LDR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1_LDR_register);
};

class NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_LSL_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_LSL_immediate()
    : NamedClassDecoder(decoder_, "Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1 LSL_immediate")
  {}

 private:
  nacl_arm_dec::Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_LSL_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_LSL_immediate);
};

class NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_ROR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_ROR_immediate()
    : NamedClassDecoder(decoder_, "Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1 ROR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_ROR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_ROR_immediate);
};

class NamedActual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1_MLA_A1
    : public NamedClassDecoder {
 public:
  NamedActual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1_MLA_A1()
    : NamedClassDecoder(decoder_, "Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1 MLA_A1")
  {}

 private:
  nacl_arm_dec::Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1_MLA_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1_MLA_A1);
};

class NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_MLS_A1
    : public NamedClassDecoder {
 public:
  NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_MLS_A1()
    : NamedClassDecoder(decoder_, "Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1 MLS_A1")
  {}

 private:
  nacl_arm_dec::Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_MLS_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_MLS_A1);
};

class NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLABB_SMLABT_SMLATB_SMLATT
    : public NamedClassDecoder {
 public:
  NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLABB_SMLABT_SMLATB_SMLATT()
    : NamedClassDecoder(decoder_, "Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1 SMLABB_SMLABT_SMLATB_SMLATT")
  {}

 private:
  nacl_arm_dec::Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLABB_SMLABT_SMLATB_SMLATT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLABB_SMLABT_SMLATB_SMLATT);
};

class NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLAWB_SMLAWT
    : public NamedClassDecoder {
 public:
  NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLAWB_SMLAWT()
    : NamedClassDecoder(decoder_, "Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1 SMLAWB_SMLAWT")
  {}

 private:
  nacl_arm_dec::Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLAWB_SMLAWT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1_SMLAWB_SMLAWT);
};

class NamedActual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVT
    : public NamedClassDecoder {
 public:
  NamedActual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVT()
    : NamedClassDecoder(decoder_, "Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1 MOVT")
  {}

 private:
  nacl_arm_dec::Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVT);
};

class NamedActual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVW
    : public NamedClassDecoder {
 public:
  NamedActual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVW()
    : NamedClassDecoder(decoder_, "Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1 MOVW")
  {}

 private:
  nacl_arm_dec::Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVW decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1_MOVW);
};

class NamedActual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MOV_immediate_A1
    : public NamedClassDecoder {
 public:
  NamedActual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MOV_immediate_A1()
    : NamedClassDecoder(decoder_, "Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1 MOV_immediate_A1")
  {}

 private:
  nacl_arm_dec::Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MOV_immediate_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MOV_immediate_A1);
};

class NamedActual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MVN_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MVN_immediate()
    : NamedClassDecoder(decoder_, "Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1 MVN_immediate")
  {}

 private:
  nacl_arm_dec::Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MVN_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MVN_immediate);
};

class NamedActual_MRS_cccc00010r001111dddd000000000000_case_1_MRS
    : public NamedClassDecoder {
 public:
  NamedActual_MRS_cccc00010r001111dddd000000000000_case_1_MRS()
    : NamedClassDecoder(decoder_, "Actual_MRS_cccc00010r001111dddd000000000000_case_1 MRS")
  {}

 private:
  nacl_arm_dec::Actual_MRS_cccc00010r001111dddd000000000000_case_1_MRS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MRS_cccc00010r001111dddd000000000000_case_1_MRS);
};

class NamedActual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1_MSR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1_MSR_immediate()
    : NamedClassDecoder(decoder_, "Actual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1 MSR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1_MSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1_MSR_immediate);
};

class NamedActual_MSR_register_cccc00010010mm00111100000000nnnn_case_1_MSR_register
    : public NamedClassDecoder {
 public:
  NamedActual_MSR_register_cccc00010010mm00111100000000nnnn_case_1_MSR_register()
    : NamedClassDecoder(decoder_, "Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1 MSR_register")
  {}

 private:
  nacl_arm_dec::Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1_MSR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MSR_register_cccc00010010mm00111100000000nnnn_case_1_MSR_register);
};

class NamedActual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1_MUL_A1
    : public NamedClassDecoder {
 public:
  NamedActual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1_MUL_A1()
    : NamedClassDecoder(decoder_, "Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1 MUL_A1")
  {}

 private:
  nacl_arm_dec::Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1_MUL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1_MUL_A1);
};

class NamedActual_NOP_cccc0011001000001111000000000000_case_1_NOP
    : public NamedClassDecoder {
 public:
  NamedActual_NOP_cccc0011001000001111000000000000_case_1_NOP()
    : NamedClassDecoder(decoder_, "Actual_NOP_cccc0011001000001111000000000000_case_1 NOP")
  {}

 private:
  nacl_arm_dec::Actual_NOP_cccc0011001000001111000000000000_case_1_NOP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_NOP_cccc0011001000001111000000000000_case_1_NOP);
};

class NamedActual_NOP_cccc0011001000001111000000000000_case_1_YIELD
    : public NamedClassDecoder {
 public:
  NamedActual_NOP_cccc0011001000001111000000000000_case_1_YIELD()
    : NamedClassDecoder(decoder_, "Actual_NOP_cccc0011001000001111000000000000_case_1 YIELD")
  {}

 private:
  nacl_arm_dec::Actual_NOP_cccc0011001000001111000000000000_case_1_YIELD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_NOP_cccc0011001000001111000000000000_case_1_YIELD);
};

class NamedActual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1_ORR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1_ORR_immediate()
    : NamedClassDecoder(decoder_, "Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1 ORR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1_ORR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1_ORR_immediate);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_PKH
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_PKH()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 PKH")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_PKH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_PKH);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QADD")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QADD16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QADD8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QADD8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QASX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QASX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QASX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QASX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDADD
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDADD()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QDADD")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDADD);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDSUB
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDSUB()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QDSUB")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDSUB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QDSUB);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSAX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSAX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QSAX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSAX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QSUB")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QSUB16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 QSUB8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_QSUB8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SADD16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SADD8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SADD8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SASX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SASX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SASX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SASX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SEL
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SEL()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SEL")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SEL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SEL);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SHADD16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SHADD8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHADD8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHASX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHASX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SHASX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHASX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSAX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSAX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SHSAX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSAX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SHSUB16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SHSUB8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SHSUB8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSAX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSAX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SSAX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSAX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSSUB16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSSUB16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SSSUB16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSSUB16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSUB8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSUB8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 SSUB8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_SSUB8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UADD16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UADD8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UADD8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UASX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UASX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UASX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UASX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UHADD16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UHADD8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHADD8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHASX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHASX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UHASX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHASX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSAX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSAX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UHSAX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSAX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UHSUB16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UHSUB8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UHSUB8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UQADD16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UQADD8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQADD8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQASX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQASX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UQASX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQASX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSAX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSAX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UQSAX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSAX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UQSUB16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 UQSUB8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_UQSUB8);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USAX
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USAX()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 USAX")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USAX);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB16
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB16()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 USUB16")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB16);
};

class NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB8
    : public NamedClassDecoder {
 public:
  NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB8()
    : NamedClassDecoder(decoder_, "Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1 USUB8")
  {}

 private:
  nacl_arm_dec::Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1_USUB8);
};

class NamedActual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_SBFX
    : public NamedClassDecoder {
 public:
  NamedActual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_SBFX()
    : NamedClassDecoder(decoder_, "Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1 SBFX")
  {}

 private:
  nacl_arm_dec::Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_SBFX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_SBFX);
};

class NamedActual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_UBFX
    : public NamedClassDecoder {
 public:
  NamedActual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_UBFX()
    : NamedClassDecoder(decoder_, "Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1 UBFX")
  {}

 private:
  nacl_arm_dec::Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_UBFX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1_UBFX);
};

class NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SDIV
    : public NamedClassDecoder {
 public:
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SDIV()
    : NamedClassDecoder(decoder_, "Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 SDIV")
  {}

 private:
  nacl_arm_dec::Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SDIV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SDIV);
};

class NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMMUL
    : public NamedClassDecoder {
 public:
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMMUL()
    : NamedClassDecoder(decoder_, "Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 SMMUL")
  {}

 private:
  nacl_arm_dec::Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMMUL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMMUL);
};

class NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUAD
    : public NamedClassDecoder {
 public:
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUAD()
    : NamedClassDecoder(decoder_, "Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 SMUAD")
  {}

 private:
  nacl_arm_dec::Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUAD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUAD);
};

class NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUSD
    : public NamedClassDecoder {
 public:
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUSD()
    : NamedClassDecoder(decoder_, "Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 SMUSD")
  {}

 private:
  nacl_arm_dec::Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUSD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUSD);
};

class NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_UDIV
    : public NamedClassDecoder {
 public:
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_UDIV()
    : NamedClassDecoder(decoder_, "Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1 UDIV")
  {}

 private:
  nacl_arm_dec::Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_UDIV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_UDIV);
};

class NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLAD
    : public NamedClassDecoder {
 public:
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLAD()
    : NamedClassDecoder(decoder_, "Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 SMLAD")
  {}

 private:
  nacl_arm_dec::Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLAD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLAD);
};

class NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLSD
    : public NamedClassDecoder {
 public:
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLSD()
    : NamedClassDecoder(decoder_, "Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 SMLSD")
  {}

 private:
  nacl_arm_dec::Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLSD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLSD);
};

class NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLA
    : public NamedClassDecoder {
 public:
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLA()
    : NamedClassDecoder(decoder_, "Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 SMMLA")
  {}

 private:
  nacl_arm_dec::Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLA);
};

class NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLS
    : public NamedClassDecoder {
 public:
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLS()
    : NamedClassDecoder(decoder_, "Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 SMMLS")
  {}

 private:
  nacl_arm_dec::Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLS);
};

class NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_USADA8
    : public NamedClassDecoder {
 public:
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_USADA8()
    : NamedClassDecoder(decoder_, "Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1 USADA8")
  {}

 private:
  nacl_arm_dec::Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_USADA8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_USADA8);
};

class NamedActual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_SMLALBB_SMLALBT_SMLALTB_SMLALTT
    : public NamedClassDecoder {
 public:
  NamedActual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_SMLALBB_SMLALBT_SMLALTB_SMLALTT()
    : NamedClassDecoder(decoder_, "Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1 SMLALBB_SMLALBT_SMLALTB_SMLALTT")
  {}

 private:
  nacl_arm_dec::Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_SMLALBB_SMLALBT_SMLALTB_SMLALTT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_SMLALBB_SMLALBT_SMLALTB_SMLALTT);
};

class NamedActual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_UMAAL_A1
    : public NamedClassDecoder {
 public:
  NamedActual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_UMAAL_A1()
    : NamedClassDecoder(decoder_, "Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1 UMAAL_A1")
  {}

 private:
  nacl_arm_dec::Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_UMAAL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1_UMAAL_A1);
};

class NamedActual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLALD
    : public NamedClassDecoder {
 public:
  NamedActual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLALD()
    : NamedClassDecoder(decoder_, "Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1 SMLALD")
  {}

 private:
  nacl_arm_dec::Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLALD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLALD);
};

class NamedActual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLSLD
    : public NamedClassDecoder {
 public:
  NamedActual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLSLD()
    : NamedClassDecoder(decoder_, "Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1 SMLSLD")
  {}

 private:
  nacl_arm_dec::Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLSLD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLSLD);
};

class NamedActual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_SMLAL_A1
    : public NamedClassDecoder {
 public:
  NamedActual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_SMLAL_A1()
    : NamedClassDecoder(decoder_, "Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1 SMLAL_A1")
  {}

 private:
  nacl_arm_dec::Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_SMLAL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_SMLAL_A1);
};

class NamedActual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_UMLAL_A1
    : public NamedClassDecoder {
 public:
  NamedActual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_UMLAL_A1()
    : NamedClassDecoder(decoder_, "Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1 UMLAL_A1")
  {}

 private:
  nacl_arm_dec::Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_UMLAL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1_UMLAL_A1);
};

class NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULBB_SMULBT_SMULTB_SMULTT
    : public NamedClassDecoder {
 public:
  NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULBB_SMULBT_SMULTB_SMULTT()
    : NamedClassDecoder(decoder_, "Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1 SMULBB_SMULBT_SMULTB_SMULTT")
  {}

 private:
  nacl_arm_dec::Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULBB_SMULBT_SMULTB_SMULTT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULBB_SMULBT_SMULTB_SMULTT);
};

class NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULWB_SMULWT
    : public NamedClassDecoder {
 public:
  NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULWB_SMULWT()
    : NamedClassDecoder(decoder_, "Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1 SMULWB_SMULWT")
  {}

 private:
  nacl_arm_dec::Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULWB_SMULWT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_SMULWB_SMULWT);
};

class NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_USAD8
    : public NamedClassDecoder {
 public:
  NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_USAD8()
    : NamedClassDecoder(decoder_, "Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1 USAD8")
  {}

 private:
  nacl_arm_dec::Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_USAD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1_USAD8);
};

class NamedActual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_SMULL_A1
    : public NamedClassDecoder {
 public:
  NamedActual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_SMULL_A1()
    : NamedClassDecoder(decoder_, "Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1 SMULL_A1")
  {}

 private:
  nacl_arm_dec::Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_SMULL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_SMULL_A1);
};

class NamedActual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_UMULL_A1
    : public NamedClassDecoder {
 public:
  NamedActual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_UMULL_A1()
    : NamedClassDecoder(decoder_, "Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1 UMULL_A1")
  {}

 private:
  nacl_arm_dec::Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_UMULL_A1 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1_UMULL_A1);
};

class NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDA_STMED
    : public NamedClassDecoder {
 public:
  NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDA_STMED()
    : NamedClassDecoder(decoder_, "Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1 STMDA_STMED")
  {}

 private:
  nacl_arm_dec::Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDA_STMED decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDA_STMED);
};

class NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDB_STMFD
    : public NamedClassDecoder {
 public:
  NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDB_STMFD()
    : NamedClassDecoder(decoder_, "Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1 STMDB_STMFD")
  {}

 private:
  nacl_arm_dec::Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDB_STMFD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMDB_STMFD);
};

class NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMIB_STMFA
    : public NamedClassDecoder {
 public:
  NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMIB_STMFA()
    : NamedClassDecoder(decoder_, "Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1 STMIB_STMFA")
  {}

 private:
  nacl_arm_dec::Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMIB_STMFA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STMIB_STMFA);
};

class NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STM_STMIA_STMEA
    : public NamedClassDecoder {
 public:
  NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STM_STMIA_STMEA()
    : NamedClassDecoder(decoder_, "Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1 STM_STMIA_STMEA")
  {}

 private:
  nacl_arm_dec::Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STM_STMIA_STMEA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1_STM_STMIA_STMEA);
};

class NamedActual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1_STRB_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1_STRB_immediate()
    : NamedClassDecoder(decoder_, "Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1 STRB_immediate")
  {}

 private:
  nacl_arm_dec::Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1_STRB_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1_STRB_immediate);
};

class NamedActual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1_STRB_register
    : public NamedClassDecoder {
 public:
  NamedActual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1_STRB_register()
    : NamedClassDecoder(decoder_, "Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1 STRB_register")
  {}

 private:
  nacl_arm_dec::Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1_STRB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1_STRB_register);
};

class NamedActual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1_STRD_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1_STRD_immediate()
    : NamedClassDecoder(decoder_, "Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1 STRD_immediate")
  {}

 private:
  nacl_arm_dec::Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1_STRD_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1_STRD_immediate);
};

class NamedActual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1_STRD_register
    : public NamedClassDecoder {
 public:
  NamedActual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1_STRD_register()
    : NamedClassDecoder(decoder_, "Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1 STRD_register")
  {}

 private:
  nacl_arm_dec::Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1_STRD_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1_STRD_register);
};

class NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREX
    : public NamedClassDecoder {
 public:
  NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREX()
    : NamedClassDecoder(decoder_, "Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1 STREX")
  {}

 private:
  nacl_arm_dec::Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREX);
};

class NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXB
    : public NamedClassDecoder {
 public:
  NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXB()
    : NamedClassDecoder(decoder_, "Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1 STREXB")
  {}

 private:
  nacl_arm_dec::Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXB);
};

class NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXH
    : public NamedClassDecoder {
 public:
  NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXH()
    : NamedClassDecoder(decoder_, "Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1 STREXH")
  {}

 private:
  nacl_arm_dec::Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STREXB_cccc00011100nnnndddd11111001tttt_case_1_STREXH);
};

class NamedActual_STREXD_cccc00011010nnnndddd11111001tttt_case_1_STREXD
    : public NamedClassDecoder {
 public:
  NamedActual_STREXD_cccc00011010nnnndddd11111001tttt_case_1_STREXD()
    : NamedClassDecoder(decoder_, "Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1 STREXD")
  {}

 private:
  nacl_arm_dec::Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1_STREXD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STREXD_cccc00011010nnnndddd11111001tttt_case_1_STREXD);
};

class NamedActual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1_STRH_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1_STRH_immediate()
    : NamedClassDecoder(decoder_, "Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1 STRH_immediate")
  {}

 private:
  nacl_arm_dec::Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1_STRH_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1_STRH_immediate);
};

class NamedActual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1_STRH_register
    : public NamedClassDecoder {
 public:
  NamedActual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1_STRH_register()
    : NamedClassDecoder(decoder_, "Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1 STRH_register")
  {}

 private:
  nacl_arm_dec::Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1_STRH_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1_STRH_register);
};

class NamedActual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1_STR_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1_STR_immediate()
    : NamedClassDecoder(decoder_, "Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1 STR_immediate")
  {}

 private:
  nacl_arm_dec::Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1_STR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1_STR_immediate);
};

class NamedActual_STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_1_STR_register
    : public NamedClassDecoder {
 public:
  NamedActual_STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_1_STR_register()
    : NamedClassDecoder(decoder_, "Actual_STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_1 STR_register")
  {}

 private:
  nacl_arm_dec::Actual_STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_1_STR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_1_STR_register);
};

class NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB
    : public NamedClassDecoder {
 public:
  NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB()
    : NamedClassDecoder(decoder_, "Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 SXTAB")
  {}

 private:
  nacl_arm_dec::Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB);
};

class NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB16
    : public NamedClassDecoder {
 public:
  NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB16()
    : NamedClassDecoder(decoder_, "Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 SXTAB16")
  {}

 private:
  nacl_arm_dec::Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAB16);
};

class NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAH
    : public NamedClassDecoder {
 public:
  NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAH()
    : NamedClassDecoder(decoder_, "Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 SXTAH")
  {}

 private:
  nacl_arm_dec::Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_SXTAH);
};

class NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB
    : public NamedClassDecoder {
 public:
  NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB()
    : NamedClassDecoder(decoder_, "Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 UXTAB")
  {}

 private:
  nacl_arm_dec::Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB);
};

class NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB16
    : public NamedClassDecoder {
 public:
  NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB16()
    : NamedClassDecoder(decoder_, "Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 UXTAB16")
  {}

 private:
  nacl_arm_dec::Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAB16);
};

class NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAH
    : public NamedClassDecoder {
 public:
  NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAH()
    : NamedClassDecoder(decoder_, "Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1 UXTAH")
  {}

 private:
  nacl_arm_dec::Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1_UXTAH);
};

class NamedActual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1_TST_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1_TST_immediate()
    : NamedClassDecoder(decoder_, "Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1 TST_immediate")
  {}

 private:
  nacl_arm_dec::Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1_TST_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1_TST_immediate);
};

class NamedActual_Unnamed_case_1_None
    : public NamedClassDecoder {
 public:
  NamedActual_Unnamed_case_1_None()
    : NamedClassDecoder(decoder_, "Actual_Unnamed_case_1 None")
  {}

 private:
  nacl_arm_dec::Actual_Unnamed_case_1_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_Unnamed_case_1_None);
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
