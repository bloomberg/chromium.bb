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

class ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0_ADC_register
    : public ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0 {
};

class ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_0_ADC_register_shifted_register
    : public ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_0 {
};

class ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0_ADD_register
    : public ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0 {
};

class ADD_register_shifted_register_cccc0000100snnnnddddssss0tt1mmmm_case_0_ADD_register_shifted_register
    : public ADD_register_shifted_register_cccc0000100snnnnddddssss0tt1mmmm_case_0 {
};

class AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0_AND_register
    : public AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0 {
};

class AND_register_shifted_register_cccc0000000snnnnddddssss0tt1mmmm_case_0_AND_register_shifted_register
    : public AND_register_shifted_register_cccc0000000snnnnddddssss0tt1mmmm_case_0 {
};

class ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0_ASR_immediate
    : public ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0 {
};

class ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_0_ASR_register
    : public ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_0 {
};

class BFC_cccc0111110mmmmmddddlllll0011111_case_0_BFC
    : public BFC_cccc0111110mmmmmddddlllll0011111_case_0 {
};

class BFI_cccc0111110mmmmmddddlllll001nnnn_case_0_BFI
    : public BFI_cccc0111110mmmmmddddlllll001nnnn_case_0 {
};

class BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0_BIC_register
    : public BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0 {
};

class BIC_register_shifted_register_cccc0001110snnnnddddssss0tt1mmmm_case_0_BIC_register_shifted_register
    : public BIC_register_shifted_register_cccc0001110snnnnddddssss0tt1mmmm_case_0 {
};

class BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0_BKPT
    : public BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0 {
};

class BLX_register_cccc000100101111111111110011mmmm_case_0_BLX_register
    : public BLX_register_cccc000100101111111111110011mmmm_case_0 {
};

class BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0_BL_BLX_immediate
    : public BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0 {
};

class B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0_B
    : public B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0 {
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

class Binary3RegisterOpAltA_MUL_A1
    : public Binary3RegisterOpAltA {
};

class Binary4RegisterDualOp_MLS_A1
    : public Binary4RegisterDualOp {
};

class Binary4RegisterDualOpLtV6RdNotRn_MLA_A1
    : public Binary4RegisterDualOpLtV6RdNotRn {
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

class Binary4RegisterDualResultUsesRnRm_SMULL_A1
    : public Binary4RegisterDualResultUsesRnRm {
};

class Binary4RegisterDualResultUsesRnRm_UMULL_A1
    : public Binary4RegisterDualResultUsesRnRm {
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

class Bx_cccc000100101111111111110001mmmm_case_0_Bx
    : public Bx_cccc000100101111111111110001mmmm_case_0 {
};

class CLZ_cccc000101101111dddd11110001mmmm_case_0_CLZ
    : public CLZ_cccc000101101111dddd11110001mmmm_case_0 {
};

class CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0_CMN_register
    : public CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0 {
};

class CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_0_CMN_register_shifted_register
    : public CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_0 {
};

class CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0_CMP_register
    : public CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0 {
};

class CMP_register_shifted_register_cccc00010101nnnn0000ssss0tt1mmmm_case_0_CMP_register_shifted_register
    : public CMP_register_shifted_register_cccc00010101nnnn0000ssss0tt1mmmm_case_0 {
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

class DBG_cccc001100100000111100001111iiii_case_0_DBG
    : public DBG_cccc001100100000111100001111iiii_case_0 {
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

class DuplicateToAdvSIMDRegisters_VDUP_ARM_core_register
    : public DuplicateToAdvSIMDRegisters {
};

class EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0_EOR_register
    : public EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0 {
};

class EOR_register_shifted_register_cccc0000001snnnnddddssss0tt1mmmm_case_0_EOR_register_shifted_register
    : public EOR_register_shifted_register_cccc0000001snnnnddddssss0tt1mmmm_case_0 {
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

class LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDA_LDMFA
    : public LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0 {
};

class LDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDB_LDMEA
    : public LDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0 {
};

class LDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMIB_LDMED
    : public LDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0 {
};

class LDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0_LDM_LDMIA_LDMFD
    : public LDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0 {
};

class LDREXB_cccc00011101nnnntttt111110011111_case_0_LDREXB
    : public LDREXB_cccc00011101nnnntttt111110011111_case_0 {
};

class LDREXD_cccc00011011nnnntttt111110011111_case_0_LDREXD
    : public LDREXD_cccc00011011nnnntttt111110011111_case_0 {
};

class LDREX_cccc00011001nnnntttt111110011111_case_0_LDREX
    : public LDREX_cccc00011001nnnntttt111110011111_case_0 {
};

class LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0_LSL_immediate
    : public LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0 {
};

class LSL_register_cccc0001101s0000ddddmmmm0001nnnn_case_0_LSL_register
    : public LSL_register_cccc0001101s0000ddddmmmm0001nnnn_case_0 {
};

class LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0_LSR_immediate
    : public LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0 {
};

class LSR_register_cccc0001101s0000ddddmmmm0011nnnn_case_0_LSR_register
    : public LSR_register_cccc0001101s0000ddddmmmm0011nnnn_case_0 {
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

class LoadRegisterImm8DoubleOp_LDRD_literal
    : public LoadRegisterImm8DoubleOp {
};

class LoadRegisterImm8Op_LDRH_literal
    : public LoadRegisterImm8Op {
};

class LoadRegisterImm8Op_LDRSB_literal
    : public LoadRegisterImm8Op {
};

class LoadRegisterImm8Op_LDRSH_literal
    : public LoadRegisterImm8Op {
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

class MOV_register_cccc0001101s0000dddd00000000mmmm_case_0_MOV_register
    : public MOV_register_cccc0001101s0000dddd00000000mmmm_case_0 {
};

class MRS_cccc00010r001111dddd000000000000_case_0_MRS
    : public MRS_cccc00010r001111dddd000000000000_case_0 {
};

class MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_0_MSR_immediate
    : public MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_0 {
};

class MSR_immediate_cccc00110r10mmmm1111iiiiiiiiiiii_case_0_MSR_immediate
    : public MSR_immediate_cccc00110r10mmmm1111iiiiiiiiiiii_case_0 {
};

class MSR_register_cccc00010010mm00111100000000nnnn_case_0_MSR_register
    : public MSR_register_cccc00010010mm00111100000000nnnn_case_0 {
};

class MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0_MVN_register
    : public MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0 {
};

class MVN_register_shifted_register_cccc0001111s0000ddddssss0tt1mmmm_case_0_MVN_register_shifted_register
    : public MVN_register_shifted_register_cccc0001111s0000ddddssss0tt1mmmm_case_0 {
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

class MoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register
    : public MoveVfpRegisterOp {
};

class MoveVfpRegisterOpWithTypeSel_MOVE_scalar_to_ARM_core_register
    : public MoveVfpRegisterOpWithTypeSel {
};

class MoveVfpRegisterOpWithTypeSel_VMOV_ARM_core_register_to_scalar
    : public MoveVfpRegisterOpWithTypeSel {
};

class NOP_cccc0011001000001111000000000000_case_0_NOP
    : public NOP_cccc0011001000001111000000000000_case_0 {
};

class ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0_ORR_register
    : public ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0 {
};

class ORR_register_shifted_register_cccc0001100snnnnddddssss0tt1mmmm_case_0_ORR_register_shifted_register
    : public ORR_register_shifted_register_cccc0001100snnnnddddssss0tt1mmmm_case_0 {
};

class PKH_cccc01101000nnnnddddiiiiit01mmmm_case_0_PKH
    : public PKH_cccc01101000nnnnddddiiiiit01mmmm_case_0 {
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

class QADD16_cccc01100010nnnndddd11110001mmmm_case_0_QADD16
    : public QADD16_cccc01100010nnnndddd11110001mmmm_case_0 {
};

class QADD8_cccc01100010nnnndddd11111001mmmm_case_0_QADD8
    : public QADD8_cccc01100010nnnndddd11111001mmmm_case_0 {
};

class QADD_cccc00010000nnnndddd00000101mmmm_case_0_QADD
    : public QADD_cccc00010000nnnndddd00000101mmmm_case_0 {
};

class QASX_cccc01100010nnnndddd11110011mmmm_case_0_QASX
    : public QASX_cccc01100010nnnndddd11110011mmmm_case_0 {
};

class QDADD_cccc00010100nnnndddd00000101mmmm_case_0_QDADD
    : public QDADD_cccc00010100nnnndddd00000101mmmm_case_0 {
};

class QDSUB_cccc00010110nnnndddd00000101mmmm_case_0_QDSUB
    : public QDSUB_cccc00010110nnnndddd00000101mmmm_case_0 {
};

class QSAX_cccc01100010nnnndddd11110101mmmm_case_0_QSAX
    : public QSAX_cccc01100010nnnndddd11110101mmmm_case_0 {
};

class QSUB16_cccc01100010nnnndddd11110111mmmm_case_0_QSUB16
    : public QSUB16_cccc01100010nnnndddd11110111mmmm_case_0 {
};

class QSUB8_cccc01100010nnnndddd11111111mmmm_case_0_QSUB8
    : public QSUB8_cccc01100010nnnndddd11111111mmmm_case_0 {
};

class QSUB_cccc00010010nnnndddd00000101mmmm_case_0_QSUB
    : public QSUB_cccc00010010nnnndddd00000101mmmm_case_0 {
};

class RBIT_cccc011011111111dddd11110011mmmm_case_0_RBIT
    : public RBIT_cccc011011111111dddd11110011mmmm_case_0 {
};

class REV16_cccc011010111111dddd11111011mmmm_case_0_REV16
    : public REV16_cccc011010111111dddd11111011mmmm_case_0 {
};

class REVSH_cccc011011111111dddd11111011mmmm_case_0_REVSH
    : public REVSH_cccc011011111111dddd11111011mmmm_case_0 {
};

class REV_cccc011010111111dddd11110011mmmm_case_0_REV
    : public REV_cccc011010111111dddd11110011mmmm_case_0 {
};

class ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0_ROR_immediate
    : public ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0 {
};

class ROR_register_cccc0001101s0000ddddmmmm0111nnnn_case_0_ROR_register
    : public ROR_register_cccc0001101s0000ddddmmmm0111nnnn_case_0 {
};

class RRX_cccc0001101s0000dddd00000110mmmm_case_0_RRX
    : public RRX_cccc0001101s0000dddd00000110mmmm_case_0 {
};

class RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0_RSB_register
    : public RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0 {
};

class RSB_register_shfited_register_cccc0000011snnnnddddssss0tt1mmmm_case_0_RSB_register_shfited_register
    : public RSB_register_shfited_register_cccc0000011snnnnddddssss0tt1mmmm_case_0 {
};

class RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0_RSC_register
    : public RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0 {
};

class RSC_register_shifted_register_cccc0000111snnnnddddssss0tt1mmmm_case_0_RSC_register_shifted_register
    : public RSC_register_shifted_register_cccc0000111snnnnddddssss0tt1mmmm_case_0 {
};

class SADD16_cccc01100001nnnndddd11110001mmmm_case_0_SADD16
    : public SADD16_cccc01100001nnnndddd11110001mmmm_case_0 {
};

class SADD8_cccc01100001nnnndddd11111001mmmm_case_0_SADD8
    : public SADD8_cccc01100001nnnndddd11111001mmmm_case_0 {
};

class SASX_cccc01100001nnnndddd11110011mmmm_case_0_SASX
    : public SASX_cccc01100001nnnndddd11110011mmmm_case_0 {
};

class SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0_SBC_register
    : public SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0 {
};

class SBC_register_shifted_register_cccc0000110snnnnddddssss0tt1mmmm_case_0_SBC_register_shifted_register
    : public SBC_register_shifted_register_cccc0000110snnnnddddssss0tt1mmmm_case_0 {
};

class SBFX_cccc0111101wwwwwddddlllll101nnnn_case_0_SBFX
    : public SBFX_cccc0111101wwwwwddddlllll101nnnn_case_0 {
};

class SDIV_cccc01110001dddd1111mmmm0001nnnn_case_0_SDIV
    : public SDIV_cccc01110001dddd1111mmmm0001nnnn_case_0 {
};

class SEL_cccc01101000nnnndddd11111011mmmm_case_0_SEL
    : public SEL_cccc01101000nnnndddd11111011mmmm_case_0 {
};

class SEV_cccc0011001000001111000000000100_case_0_SEV
    : public SEV_cccc0011001000001111000000000100_case_0 {
};

class SHADD16_cccc01100011nnnndddd11110001mmmm_case_0_SHADD16
    : public SHADD16_cccc01100011nnnndddd11110001mmmm_case_0 {
};

class SHADD8_cccc01100011nnnndddd11111001mmmm_case_0_SHADD8
    : public SHADD8_cccc01100011nnnndddd11111001mmmm_case_0 {
};

class SHASX_cccc01100011nnnndddd11110011mmmm_case_0_SHASX
    : public SHASX_cccc01100011nnnndddd11110011mmmm_case_0 {
};

class SHSAX_cccc01100011nnnndddd11110101mmmm_case_0_SHSAX
    : public SHSAX_cccc01100011nnnndddd11110101mmmm_case_0 {
};

class SHSUB16_cccc01100011nnnndddd11110111mmmm_case_0_SHSUB16
    : public SHSUB16_cccc01100011nnnndddd11110111mmmm_case_0 {
};

class SHSUB8_cccc01100011nnnndddd11111111mmmm_case_0_SHSUB8
    : public SHSUB8_cccc01100011nnnndddd11111111mmmm_case_0 {
};

class SMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0_SMLABB_SMLABT_SMLATB_SMLATT
    : public SMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0 {
};

class SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0_SMLAD
    : public SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0 {
};

class SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0_SMLALBB_SMLALBT_SMLALTB_SMLALTT
    : public SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0 {
};

class SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0_SMLALD
    : public SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0 {
};

class SMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0_SMLAWB_SMLAWT
    : public SMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0 {
};

class SMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0_SMLSD
    : public SMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0 {
};

class SMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0_SMLSLD
    : public SMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0 {
};

class SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0_SMMLA
    : public SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0 {
};

class SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0_SMMLS
    : public SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0 {
};

class SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0_SMMUL
    : public SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0 {
};

class SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0_SMUAD
    : public SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0 {
};

class SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0_SMULBB_SMULBT_SMULTB_SMULTT
    : public SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0 {
};

class SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0_SMULWB_SMULWT
    : public SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0 {
};

class SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0_SMUSD
    : public SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0 {
};

class SSAT16_cccc01101010iiiidddd11110011nnnn_case_0_SSAT16
    : public SSAT16_cccc01101010iiiidddd11110011nnnn_case_0 {
};

class SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0_SSAT
    : public SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0 {
};

class SSAX_cccc01100001nnnndddd11110101mmmm_case_0_SSAX
    : public SSAX_cccc01100001nnnndddd11110101mmmm_case_0 {
};

class SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0_SSSUB16
    : public SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0 {
};

class SSUB8_cccc01100001nnnndddd11111111mmmm_case_0_SSUB8
    : public SSUB8_cccc01100001nnnndddd11111111mmmm_case_0 {
};

class STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDA_STMED
    : public STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0 {
};

class STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDB_STMFD
    : public STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0 {
};

class STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0_STMIB_STMFA
    : public STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0 {
};

class STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0_STM_STMIA_STMEA
    : public STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0 {
};

class STREXB_cccc00011100nnnndddd11111001tttt_case_0_STREXB
    : public STREXB_cccc00011100nnnndddd11111001tttt_case_0 {
};

class STREXD_cccc00011010nnnndddd11111001tttt_case_0_STREXD
    : public STREXD_cccc00011010nnnndddd11111001tttt_case_0 {
};

class STREXH_cccc00011110nnnndddd11111001tttt_case_0_STREXH
    : public STREXH_cccc00011110nnnndddd11111001tttt_case_0 {
};

class STREXH_cccc00011111nnnntttt111110011111_case_0_STREXH
    : public STREXH_cccc00011111nnnntttt111110011111_case_0 {
};

class STREX_cccc00011000nnnndddd11111001tttt_case_0_STREX
    : public STREX_cccc00011000nnnndddd11111001tttt_case_0 {
};

class SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0_SUB_register
    : public SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0 {
};

class SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0_SUB_register_shifted_register
    : public SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0 {
};

class SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0_SXTAB16
    : public SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0 {
};

class SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0_SXTAB
    : public SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0 {
};

class SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0_SXTAH
    : public SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0 {
};

class SXTB16_cccc011010001111ddddrr000111mmmm_case_0_SXTB16
    : public SXTB16_cccc011010001111ddddrr000111mmmm_case_0 {
};

class SXTB_cccc011010101111ddddrr000111mmmm_case_0_SXTB
    : public SXTB_cccc011010101111ddddrr000111mmmm_case_0 {
};

class SXTH_cccc011010111111ddddrr000111mmmm_case_0_SXTH
    : public SXTH_cccc011010111111ddddrr000111mmmm_case_0 {
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

class StoreVectorRegister_VSTR
    : public StoreVectorRegister {
};

class StoreVectorRegisterList_VPUSH
    : public StoreVectorRegisterList {
};

class StoreVectorRegisterList_VSTM
    : public StoreVectorRegisterList {
};

class TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0_TEQ_register
    : public TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0 {
};

class TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0_TEQ_register_shifted_register
    : public TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0 {
};

class TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0_TST_register
    : public TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0 {
};

class TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0_TST_register_shifted_register
    : public TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0 {
};

class UADD16_cccc01100101nnnndddd11110001mmmm_case_0_UADD16
    : public UADD16_cccc01100101nnnndddd11110001mmmm_case_0 {
};

class UADD8_cccc01100101nnnndddd11111001mmmm_case_0_UADD8
    : public UADD8_cccc01100101nnnndddd11111001mmmm_case_0 {
};

class UASX_cccc01100101nnnndddd11110011mmmm_case_0_UASX
    : public UASX_cccc01100101nnnndddd11110011mmmm_case_0 {
};

class UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0_UBFX
    : public UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0 {
};

class UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0_UDIV
    : public UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0 {
};

class UHADD16_cccc01100111nnnndddd11110001mmmm_case_0_UHADD16
    : public UHADD16_cccc01100111nnnndddd11110001mmmm_case_0 {
};

class UHADD8_cccc01100111nnnndddd11111001mmmm_case_0_UHADD8
    : public UHADD8_cccc01100111nnnndddd11111001mmmm_case_0 {
};

class UHASX_cccc01100111nnnndddd11110011mmmm_case_0_UHASX
    : public UHASX_cccc01100111nnnndddd11110011mmmm_case_0 {
};

class UHSAX_cccc01100111nnnndddd11110101mmmm_case_0_UHSAX
    : public UHSAX_cccc01100111nnnndddd11110101mmmm_case_0 {
};

class UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0_UHSUB16
    : public UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0 {
};

class UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0_UHSUB8
    : public UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0 {
};

class UQADD16_cccc01100110nnnndddd11110001mmmm_case_0_UQADD16
    : public UQADD16_cccc01100110nnnndddd11110001mmmm_case_0 {
};

class UQADD8_cccc01100110nnnndddd11111001mmmm_case_0_UQADD8
    : public UQADD8_cccc01100110nnnndddd11111001mmmm_case_0 {
};

class UQASX_cccc01100110nnnndddd11110011mmmm_case_0_UQASX
    : public UQASX_cccc01100110nnnndddd11110011mmmm_case_0 {
};

class UQSAX_cccc01100110nnnndddd11110101mmmm_case_0_UQSAX
    : public UQSAX_cccc01100110nnnndddd11110101mmmm_case_0 {
};

class UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0_UQSUB16
    : public UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0 {
};

class UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0_UQSUB8
    : public UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0 {
};

class USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0_USAD8
    : public USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0 {
};

class USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0_USADA8
    : public USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0 {
};

class USAT16_cccc01101110iiiidddd11110011nnnn_case_0_USAT16
    : public USAT16_cccc01101110iiiidddd11110011nnnn_case_0 {
};

class USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0_USAT
    : public USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0 {
};

class USAX_cccc01100101nnnndddd11110101mmmm_case_0_USAX
    : public USAX_cccc01100101nnnndddd11110101mmmm_case_0 {
};

class USUB16_cccc01100101nnnndddd11110111mmmm_case_0_USUB16
    : public USUB16_cccc01100101nnnndddd11110111mmmm_case_0 {
};

class USUB8_cccc01100101nnnndddd11111111mmmm_case_0_USUB8
    : public USUB8_cccc01100101nnnndddd11111111mmmm_case_0 {
};

class UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0_UXTAB16
    : public UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0 {
};

class UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0_UXTAB
    : public UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0 {
};

class UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0_UXTAH
    : public UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0 {
};

class UXTB16_cccc011011001111ddddrr000111mmmm_case_0_UXTB16
    : public UXTB16_cccc011011001111ddddrr000111mmmm_case_0 {
};

class UXTB_cccc011011101111ddddrr000111mmmm_case_0_UXTB
    : public UXTB_cccc011011101111ddddrr000111mmmm_case_0 {
};

class UXTH_cccc011011111111ddddrr000111mmmm_case_0_UXTH
    : public UXTH_cccc011011111111ddddrr000111mmmm_case_0 {
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

class Undefined_None
    : public Undefined {
};

class Unnamed_case_0_None
    : public Unnamed_case_0 {
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

class VectorBinary3RegisterImmOp_VEXT
    : public VectorBinary3RegisterImmOp {
};

class VectorBinary3RegisterLookupOp_VTBL_VTBX
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

class VectorUnary2RegisterDup_VDUP_scalar
    : public VectorUnary2RegisterDup {
};

class VfpMrsOp_VMRS
    : public VfpMrsOp {
};

class VfpUsesRegOp_VMSR
    : public VfpUsesRegOp {
};

class WFE_cccc0011001000001111000000000010_case_0_WFE
    : public WFE_cccc0011001000001111000000000010_case_0 {
};

class WFI_cccc0011001000001111000000000011_case_0_WFI
    : public WFI_cccc0011001000001111000000000011_case_0 {
};

class YIELD_cccc0011001000001111000000000001_case_0_YIELD
    : public YIELD_cccc0011001000001111000000000001_case_0 {
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

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_BLX_immediate
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP2
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CLREX
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CPS
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_immediate
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_literal
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

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR2
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR2
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC2
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC2
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_RFE
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SETEND
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SRS
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC2
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SVC
    : public Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 {
};

class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_None
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

class Actual_DMB_1111010101111111111100000101xxxx_case_1_DMB
    : public Actual_DMB_1111010101111111111100000101xxxx_case_1 {
};

class Actual_DMB_1111010101111111111100000101xxxx_case_1_DSB
    : public Actual_DMB_1111010101111111111100000101xxxx_case_1 {
};

class Actual_ISB_1111010101111111111100000110xxxx_case_1_ISB
    : public Actual_ISB_1111010101111111111100000110xxxx_case_1 {
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

class Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSH_literal
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

class Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1_MOVE_scalar_to_ARM_core_register
    : public Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1 {
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

class Actual_PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1_PLD_PLDW_immediate
    : public Actual_PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1 {
};

class Actual_PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_1_PLD_PLDW_register
    : public Actual_PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_1 {
};

class Actual_PLD_literal_11110101u10111111111iiiiiiiiiiii_case_1_PLD_literal
    : public Actual_PLD_literal_11110101u10111111111iiiiiiiiiiii_case_1 {
};

class Actual_PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_1_PLI_immediate_literal
    : public Actual_PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_1 {
};

class Actual_PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_1_PLI_register
    : public Actual_PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_1 {
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

class Actual_Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_1_None
    : public Actual_Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_1 {
};

class Actual_Unnamed_case_1_None
    : public Actual_Unnamed_case_1 {
};

class Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VABS
    : public Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 {
};

class Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCMP_VCMPE
    : public Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 {
};

class Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVTB_VCVTT
    : public Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 {
};

class Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVT_between_double_precision_and_single_precision
    : public Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 {
};

class Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_immediate
    : public Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 {
};

class Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_register
    : public Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 {
};

class Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VNEG
    : public Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 {
};

class Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VSQRT
    : public Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VADD_floating_point
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VDIV
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFMA_VFMS
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFNMA_VFNMS
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMLA_VMLS_floating_point
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMUL_floating_point
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMLA_VNMLS
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMUL
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VSUB_floating_point
    : public Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 {
};

class Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1_VCVT_VCVTR_between_floating_point_and_integer_Floating_point
    : public Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1 {
};

class Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1_VCVT_between_floating_point_and_fixed_point_Floating_point
    : public Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1 {
};

class Actual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1_VDUP_ARM_core_register
    : public Actual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1 {
};

class Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1_VLDM
    : public Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1 {
};

class Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1_VLDM
    : public Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1 {
};

class Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1_VLDR
    : public Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1 {
};

class Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1_VMOV_ARM_core_register_to_scalar
    : public Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1 {
};

class Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1_VMOV_between_ARM_core_register_and_single_precision_register
    : public Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1 {
};

class Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register
    : public Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1 {
};

class Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers
    : public Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1 {
};

class Actual_VMRS_cccc111011110001tttt101000010000_case_1_VMRS
    : public Actual_VMRS_cccc111011110001tttt101000010000_case_1 {
};

class Actual_VMSR_cccc111011100001tttt101000010000_case_1_VMSR
    : public Actual_VMSR_cccc111011100001tttt101000010000_case_1 {
};

class Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPOP
    : public Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1 {
};

class Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPUSH
    : public Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1 {
};

class Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPOP
    : public Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1 {
};

class Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPUSH
    : public Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1 {
};

class Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1_VSTM
    : public Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1 {
};

class Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1_VSTM
    : public Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1 {
};

class Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1_VSTR
    : public Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1 {
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

class NamedADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0_ADC_register
    : public NamedClassDecoder {
 public:
  NamedADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0_ADC_register()
    : NamedClassDecoder(decoder_, "ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0 ADC_register")
  {}

 private:
  nacl_arm_dec::ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0_ADC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0_ADC_register);
};

class NamedADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_0_ADC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_0_ADC_register_shifted_register()
    : NamedClassDecoder(decoder_, "ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_0 ADC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_0_ADC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_0_ADC_register_shifted_register);
};

class NamedADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0_ADD_register
    : public NamedClassDecoder {
 public:
  NamedADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0_ADD_register()
    : NamedClassDecoder(decoder_, "ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0 ADD_register")
  {}

 private:
  nacl_arm_dec::ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0_ADD_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0_ADD_register);
};

class NamedADD_register_shifted_register_cccc0000100snnnnddddssss0tt1mmmm_case_0_ADD_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedADD_register_shifted_register_cccc0000100snnnnddddssss0tt1mmmm_case_0_ADD_register_shifted_register()
    : NamedClassDecoder(decoder_, "ADD_register_shifted_register_cccc0000100snnnnddddssss0tt1mmmm_case_0 ADD_register_shifted_register")
  {}

 private:
  nacl_arm_dec::ADD_register_shifted_register_cccc0000100snnnnddddssss0tt1mmmm_case_0_ADD_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedADD_register_shifted_register_cccc0000100snnnnddddssss0tt1mmmm_case_0_ADD_register_shifted_register);
};

class NamedAND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0_AND_register
    : public NamedClassDecoder {
 public:
  NamedAND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0_AND_register()
    : NamedClassDecoder(decoder_, "AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0 AND_register")
  {}

 private:
  nacl_arm_dec::AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0_AND_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedAND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0_AND_register);
};

class NamedAND_register_shifted_register_cccc0000000snnnnddddssss0tt1mmmm_case_0_AND_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedAND_register_shifted_register_cccc0000000snnnnddddssss0tt1mmmm_case_0_AND_register_shifted_register()
    : NamedClassDecoder(decoder_, "AND_register_shifted_register_cccc0000000snnnnddddssss0tt1mmmm_case_0 AND_register_shifted_register")
  {}

 private:
  nacl_arm_dec::AND_register_shifted_register_cccc0000000snnnnddddssss0tt1mmmm_case_0_AND_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedAND_register_shifted_register_cccc0000000snnnnddddssss0tt1mmmm_case_0_AND_register_shifted_register);
};

class NamedASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0_ASR_immediate
    : public NamedClassDecoder {
 public:
  NamedASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0_ASR_immediate()
    : NamedClassDecoder(decoder_, "ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0 ASR_immediate")
  {}

 private:
  nacl_arm_dec::ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0_ASR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0_ASR_immediate);
};

class NamedASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_0_ASR_register
    : public NamedClassDecoder {
 public:
  NamedASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_0_ASR_register()
    : NamedClassDecoder(decoder_, "ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_0 ASR_register")
  {}

 private:
  nacl_arm_dec::ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_0_ASR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_0_ASR_register);
};

class NamedBFC_cccc0111110mmmmmddddlllll0011111_case_0_BFC
    : public NamedClassDecoder {
 public:
  NamedBFC_cccc0111110mmmmmddddlllll0011111_case_0_BFC()
    : NamedClassDecoder(decoder_, "BFC_cccc0111110mmmmmddddlllll0011111_case_0 BFC")
  {}

 private:
  nacl_arm_dec::BFC_cccc0111110mmmmmddddlllll0011111_case_0_BFC decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBFC_cccc0111110mmmmmddddlllll0011111_case_0_BFC);
};

class NamedBFI_cccc0111110mmmmmddddlllll001nnnn_case_0_BFI
    : public NamedClassDecoder {
 public:
  NamedBFI_cccc0111110mmmmmddddlllll001nnnn_case_0_BFI()
    : NamedClassDecoder(decoder_, "BFI_cccc0111110mmmmmddddlllll001nnnn_case_0 BFI")
  {}

 private:
  nacl_arm_dec::BFI_cccc0111110mmmmmddddlllll001nnnn_case_0_BFI decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBFI_cccc0111110mmmmmddddlllll001nnnn_case_0_BFI);
};

class NamedBIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0_BIC_register
    : public NamedClassDecoder {
 public:
  NamedBIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0_BIC_register()
    : NamedClassDecoder(decoder_, "BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0 BIC_register")
  {}

 private:
  nacl_arm_dec::BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0_BIC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0_BIC_register);
};

class NamedBIC_register_shifted_register_cccc0001110snnnnddddssss0tt1mmmm_case_0_BIC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedBIC_register_shifted_register_cccc0001110snnnnddddssss0tt1mmmm_case_0_BIC_register_shifted_register()
    : NamedClassDecoder(decoder_, "BIC_register_shifted_register_cccc0001110snnnnddddssss0tt1mmmm_case_0 BIC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::BIC_register_shifted_register_cccc0001110snnnnddddssss0tt1mmmm_case_0_BIC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBIC_register_shifted_register_cccc0001110snnnnddddssss0tt1mmmm_case_0_BIC_register_shifted_register);
};

class NamedBKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0_BKPT
    : public NamedClassDecoder {
 public:
  NamedBKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0_BKPT()
    : NamedClassDecoder(decoder_, "BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0 BKPT")
  {}

 private:
  nacl_arm_dec::BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0_BKPT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0_BKPT);
};

class NamedBLX_register_cccc000100101111111111110011mmmm_case_0_BLX_register
    : public NamedClassDecoder {
 public:
  NamedBLX_register_cccc000100101111111111110011mmmm_case_0_BLX_register()
    : NamedClassDecoder(decoder_, "BLX_register_cccc000100101111111111110011mmmm_case_0 BLX_register")
  {}

 private:
  nacl_arm_dec::BLX_register_cccc000100101111111111110011mmmm_case_0_BLX_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBLX_register_cccc000100101111111111110011mmmm_case_0_BLX_register);
};

class NamedBL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0_BL_BLX_immediate
    : public NamedClassDecoder {
 public:
  NamedBL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0_BL_BLX_immediate()
    : NamedClassDecoder(decoder_, "BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0 BL_BLX_immediate")
  {}

 private:
  nacl_arm_dec::BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0_BL_BLX_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0_BL_BLX_immediate);
};

class NamedB_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0_B
    : public NamedClassDecoder {
 public:
  NamedB_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0_B()
    : NamedClassDecoder(decoder_, "B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0 B")
  {}

 private:
  nacl_arm_dec::B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0_B decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedB_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0_B);
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

class NamedBx_cccc000100101111111111110001mmmm_case_0_Bx
    : public NamedClassDecoder {
 public:
  NamedBx_cccc000100101111111111110001mmmm_case_0_Bx()
    : NamedClassDecoder(decoder_, "Bx_cccc000100101111111111110001mmmm_case_0 Bx")
  {}

 private:
  nacl_arm_dec::Bx_cccc000100101111111111110001mmmm_case_0_Bx decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedBx_cccc000100101111111111110001mmmm_case_0_Bx);
};

class NamedCLZ_cccc000101101111dddd11110001mmmm_case_0_CLZ
    : public NamedClassDecoder {
 public:
  NamedCLZ_cccc000101101111dddd11110001mmmm_case_0_CLZ()
    : NamedClassDecoder(decoder_, "CLZ_cccc000101101111dddd11110001mmmm_case_0 CLZ")
  {}

 private:
  nacl_arm_dec::CLZ_cccc000101101111dddd11110001mmmm_case_0_CLZ decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCLZ_cccc000101101111dddd11110001mmmm_case_0_CLZ);
};

class NamedCMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0_CMN_register
    : public NamedClassDecoder {
 public:
  NamedCMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0_CMN_register()
    : NamedClassDecoder(decoder_, "CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0 CMN_register")
  {}

 private:
  nacl_arm_dec::CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0_CMN_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0_CMN_register);
};

class NamedCMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_0_CMN_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedCMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_0_CMN_register_shifted_register()
    : NamedClassDecoder(decoder_, "CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_0 CMN_register_shifted_register")
  {}

 private:
  nacl_arm_dec::CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_0_CMN_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_0_CMN_register_shifted_register);
};

class NamedCMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0_CMP_register
    : public NamedClassDecoder {
 public:
  NamedCMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0_CMP_register()
    : NamedClassDecoder(decoder_, "CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0 CMP_register")
  {}

 private:
  nacl_arm_dec::CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0_CMP_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0_CMP_register);
};

class NamedCMP_register_shifted_register_cccc00010101nnnn0000ssss0tt1mmmm_case_0_CMP_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedCMP_register_shifted_register_cccc00010101nnnn0000ssss0tt1mmmm_case_0_CMP_register_shifted_register()
    : NamedClassDecoder(decoder_, "CMP_register_shifted_register_cccc00010101nnnn0000ssss0tt1mmmm_case_0 CMP_register_shifted_register")
  {}

 private:
  nacl_arm_dec::CMP_register_shifted_register_cccc00010101nnnn0000ssss0tt1mmmm_case_0_CMP_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedCMP_register_shifted_register_cccc00010101nnnn0000ssss0tt1mmmm_case_0_CMP_register_shifted_register);
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

class NamedDBG_cccc001100100000111100001111iiii_case_0_DBG
    : public NamedClassDecoder {
 public:
  NamedDBG_cccc001100100000111100001111iiii_case_0_DBG()
    : NamedClassDecoder(decoder_, "DBG_cccc001100100000111100001111iiii_case_0 DBG")
  {}

 private:
  nacl_arm_dec::DBG_cccc001100100000111100001111iiii_case_0_DBG decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDBG_cccc001100100000111100001111iiii_case_0_DBG);
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

class NamedDuplicateToAdvSIMDRegisters_VDUP_ARM_core_register
    : public NamedClassDecoder {
 public:
  NamedDuplicateToAdvSIMDRegisters_VDUP_ARM_core_register()
    : NamedClassDecoder(decoder_, "DuplicateToAdvSIMDRegisters VDUP_ARM_core_register")
  {}

 private:
  nacl_arm_dec::DuplicateToAdvSIMDRegisters_VDUP_ARM_core_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedDuplicateToAdvSIMDRegisters_VDUP_ARM_core_register);
};

class NamedEOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0_EOR_register
    : public NamedClassDecoder {
 public:
  NamedEOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0_EOR_register()
    : NamedClassDecoder(decoder_, "EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0 EOR_register")
  {}

 private:
  nacl_arm_dec::EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0_EOR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedEOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0_EOR_register);
};

class NamedEOR_register_shifted_register_cccc0000001snnnnddddssss0tt1mmmm_case_0_EOR_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedEOR_register_shifted_register_cccc0000001snnnnddddssss0tt1mmmm_case_0_EOR_register_shifted_register()
    : NamedClassDecoder(decoder_, "EOR_register_shifted_register_cccc0000001snnnnddddssss0tt1mmmm_case_0 EOR_register_shifted_register")
  {}

 private:
  nacl_arm_dec::EOR_register_shifted_register_cccc0000001snnnnddddssss0tt1mmmm_case_0_EOR_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedEOR_register_shifted_register_cccc0000001snnnnddddssss0tt1mmmm_case_0_EOR_register_shifted_register);
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

class NamedLDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDA_LDMFA
    : public NamedClassDecoder {
 public:
  NamedLDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDA_LDMFA()
    : NamedClassDecoder(decoder_, "LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0 LDMDA_LDMFA")
  {}

 private:
  nacl_arm_dec::LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDA_LDMFA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDA_LDMFA);
};

class NamedLDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDB_LDMEA
    : public NamedClassDecoder {
 public:
  NamedLDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDB_LDMEA()
    : NamedClassDecoder(decoder_, "LDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0 LDMDB_LDMEA")
  {}

 private:
  nacl_arm_dec::LDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDB_LDMEA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMDB_LDMEA);
};

class NamedLDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMIB_LDMED
    : public NamedClassDecoder {
 public:
  NamedLDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMIB_LDMED()
    : NamedClassDecoder(decoder_, "LDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0 LDMIB_LDMED")
  {}

 private:
  nacl_arm_dec::LDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMIB_LDMED decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0_LDMIB_LDMED);
};

class NamedLDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0_LDM_LDMIA_LDMFD
    : public NamedClassDecoder {
 public:
  NamedLDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0_LDM_LDMIA_LDMFD()
    : NamedClassDecoder(decoder_, "LDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0 LDM_LDMIA_LDMFD")
  {}

 private:
  nacl_arm_dec::LDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0_LDM_LDMIA_LDMFD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0_LDM_LDMIA_LDMFD);
};

class NamedLDREXB_cccc00011101nnnntttt111110011111_case_0_LDREXB
    : public NamedClassDecoder {
 public:
  NamedLDREXB_cccc00011101nnnntttt111110011111_case_0_LDREXB()
    : NamedClassDecoder(decoder_, "LDREXB_cccc00011101nnnntttt111110011111_case_0 LDREXB")
  {}

 private:
  nacl_arm_dec::LDREXB_cccc00011101nnnntttt111110011111_case_0_LDREXB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLDREXB_cccc00011101nnnntttt111110011111_case_0_LDREXB);
};

class NamedLDREXD_cccc00011011nnnntttt111110011111_case_0_LDREXD
    : public NamedClassDecoder {
 public:
  NamedLDREXD_cccc00011011nnnntttt111110011111_case_0_LDREXD()
    : NamedClassDecoder(decoder_, "LDREXD_cccc00011011nnnntttt111110011111_case_0 LDREXD")
  {}

 private:
  nacl_arm_dec::LDREXD_cccc00011011nnnntttt111110011111_case_0_LDREXD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLDREXD_cccc00011011nnnntttt111110011111_case_0_LDREXD);
};

class NamedLDREX_cccc00011001nnnntttt111110011111_case_0_LDREX
    : public NamedClassDecoder {
 public:
  NamedLDREX_cccc00011001nnnntttt111110011111_case_0_LDREX()
    : NamedClassDecoder(decoder_, "LDREX_cccc00011001nnnntttt111110011111_case_0 LDREX")
  {}

 private:
  nacl_arm_dec::LDREX_cccc00011001nnnntttt111110011111_case_0_LDREX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLDREX_cccc00011001nnnntttt111110011111_case_0_LDREX);
};

class NamedLSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0_LSL_immediate
    : public NamedClassDecoder {
 public:
  NamedLSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0_LSL_immediate()
    : NamedClassDecoder(decoder_, "LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0 LSL_immediate")
  {}

 private:
  nacl_arm_dec::LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0_LSL_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0_LSL_immediate);
};

class NamedLSL_register_cccc0001101s0000ddddmmmm0001nnnn_case_0_LSL_register
    : public NamedClassDecoder {
 public:
  NamedLSL_register_cccc0001101s0000ddddmmmm0001nnnn_case_0_LSL_register()
    : NamedClassDecoder(decoder_, "LSL_register_cccc0001101s0000ddddmmmm0001nnnn_case_0 LSL_register")
  {}

 private:
  nacl_arm_dec::LSL_register_cccc0001101s0000ddddmmmm0001nnnn_case_0_LSL_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLSL_register_cccc0001101s0000ddddmmmm0001nnnn_case_0_LSL_register);
};

class NamedLSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0_LSR_immediate
    : public NamedClassDecoder {
 public:
  NamedLSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0_LSR_immediate()
    : NamedClassDecoder(decoder_, "LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0 LSR_immediate")
  {}

 private:
  nacl_arm_dec::LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0_LSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0_LSR_immediate);
};

class NamedLSR_register_cccc0001101s0000ddddmmmm0011nnnn_case_0_LSR_register
    : public NamedClassDecoder {
 public:
  NamedLSR_register_cccc0001101s0000ddddmmmm0011nnnn_case_0_LSR_register()
    : NamedClassDecoder(decoder_, "LSR_register_cccc0001101s0000ddddmmmm0011nnnn_case_0 LSR_register")
  {}

 private:
  nacl_arm_dec::LSR_register_cccc0001101s0000ddddmmmm0011nnnn_case_0_LSR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLSR_register_cccc0001101s0000ddddmmmm0011nnnn_case_0_LSR_register);
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

class NamedLoadRegisterImm8Op_LDRSH_literal
    : public NamedClassDecoder {
 public:
  NamedLoadRegisterImm8Op_LDRSH_literal()
    : NamedClassDecoder(decoder_, "LoadRegisterImm8Op LDRSH_literal")
  {}

 private:
  nacl_arm_dec::LoadRegisterImm8Op_LDRSH_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedLoadRegisterImm8Op_LDRSH_literal);
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

class NamedMOV_register_cccc0001101s0000dddd00000000mmmm_case_0_MOV_register
    : public NamedClassDecoder {
 public:
  NamedMOV_register_cccc0001101s0000dddd00000000mmmm_case_0_MOV_register()
    : NamedClassDecoder(decoder_, "MOV_register_cccc0001101s0000dddd00000000mmmm_case_0 MOV_register")
  {}

 private:
  nacl_arm_dec::MOV_register_cccc0001101s0000dddd00000000mmmm_case_0_MOV_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMOV_register_cccc0001101s0000dddd00000000mmmm_case_0_MOV_register);
};

class NamedMRS_cccc00010r001111dddd000000000000_case_0_MRS
    : public NamedClassDecoder {
 public:
  NamedMRS_cccc00010r001111dddd000000000000_case_0_MRS()
    : NamedClassDecoder(decoder_, "MRS_cccc00010r001111dddd000000000000_case_0 MRS")
  {}

 private:
  nacl_arm_dec::MRS_cccc00010r001111dddd000000000000_case_0_MRS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMRS_cccc00010r001111dddd000000000000_case_0_MRS);
};

class NamedMSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_0_MSR_immediate
    : public NamedClassDecoder {
 public:
  NamedMSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_0_MSR_immediate()
    : NamedClassDecoder(decoder_, "MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_0 MSR_immediate")
  {}

 private:
  nacl_arm_dec::MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_0_MSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_0_MSR_immediate);
};

class NamedMSR_immediate_cccc00110r10mmmm1111iiiiiiiiiiii_case_0_MSR_immediate
    : public NamedClassDecoder {
 public:
  NamedMSR_immediate_cccc00110r10mmmm1111iiiiiiiiiiii_case_0_MSR_immediate()
    : NamedClassDecoder(decoder_, "MSR_immediate_cccc00110r10mmmm1111iiiiiiiiiiii_case_0 MSR_immediate")
  {}

 private:
  nacl_arm_dec::MSR_immediate_cccc00110r10mmmm1111iiiiiiiiiiii_case_0_MSR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMSR_immediate_cccc00110r10mmmm1111iiiiiiiiiiii_case_0_MSR_immediate);
};

class NamedMSR_register_cccc00010010mm00111100000000nnnn_case_0_MSR_register
    : public NamedClassDecoder {
 public:
  NamedMSR_register_cccc00010010mm00111100000000nnnn_case_0_MSR_register()
    : NamedClassDecoder(decoder_, "MSR_register_cccc00010010mm00111100000000nnnn_case_0 MSR_register")
  {}

 private:
  nacl_arm_dec::MSR_register_cccc00010010mm00111100000000nnnn_case_0_MSR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMSR_register_cccc00010010mm00111100000000nnnn_case_0_MSR_register);
};

class NamedMVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0_MVN_register
    : public NamedClassDecoder {
 public:
  NamedMVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0_MVN_register()
    : NamedClassDecoder(decoder_, "MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0 MVN_register")
  {}

 private:
  nacl_arm_dec::MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0_MVN_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0_MVN_register);
};

class NamedMVN_register_shifted_register_cccc0001111s0000ddddssss0tt1mmmm_case_0_MVN_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedMVN_register_shifted_register_cccc0001111s0000ddddssss0tt1mmmm_case_0_MVN_register_shifted_register()
    : NamedClassDecoder(decoder_, "MVN_register_shifted_register_cccc0001111s0000ddddssss0tt1mmmm_case_0 MVN_register_shifted_register")
  {}

 private:
  nacl_arm_dec::MVN_register_shifted_register_cccc0001111s0000ddddssss0tt1mmmm_case_0_MVN_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedMVN_register_shifted_register_cccc0001111s0000ddddssss0tt1mmmm_case_0_MVN_register_shifted_register);
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

class NamedNOP_cccc0011001000001111000000000000_case_0_NOP
    : public NamedClassDecoder {
 public:
  NamedNOP_cccc0011001000001111000000000000_case_0_NOP()
    : NamedClassDecoder(decoder_, "NOP_cccc0011001000001111000000000000_case_0 NOP")
  {}

 private:
  nacl_arm_dec::NOP_cccc0011001000001111000000000000_case_0_NOP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedNOP_cccc0011001000001111000000000000_case_0_NOP);
};

class NamedORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0_ORR_register
    : public NamedClassDecoder {
 public:
  NamedORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0_ORR_register()
    : NamedClassDecoder(decoder_, "ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0 ORR_register")
  {}

 private:
  nacl_arm_dec::ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0_ORR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0_ORR_register);
};

class NamedORR_register_shifted_register_cccc0001100snnnnddddssss0tt1mmmm_case_0_ORR_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedORR_register_shifted_register_cccc0001100snnnnddddssss0tt1mmmm_case_0_ORR_register_shifted_register()
    : NamedClassDecoder(decoder_, "ORR_register_shifted_register_cccc0001100snnnnddddssss0tt1mmmm_case_0 ORR_register_shifted_register")
  {}

 private:
  nacl_arm_dec::ORR_register_shifted_register_cccc0001100snnnnddddssss0tt1mmmm_case_0_ORR_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedORR_register_shifted_register_cccc0001100snnnnddddssss0tt1mmmm_case_0_ORR_register_shifted_register);
};

class NamedPKH_cccc01101000nnnnddddiiiiit01mmmm_case_0_PKH
    : public NamedClassDecoder {
 public:
  NamedPKH_cccc01101000nnnnddddiiiiit01mmmm_case_0_PKH()
    : NamedClassDecoder(decoder_, "PKH_cccc01101000nnnnddddiiiiit01mmmm_case_0 PKH")
  {}

 private:
  nacl_arm_dec::PKH_cccc01101000nnnnddddiiiiit01mmmm_case_0_PKH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedPKH_cccc01101000nnnnddddiiiiit01mmmm_case_0_PKH);
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

class NamedQADD16_cccc01100010nnnndddd11110001mmmm_case_0_QADD16
    : public NamedClassDecoder {
 public:
  NamedQADD16_cccc01100010nnnndddd11110001mmmm_case_0_QADD16()
    : NamedClassDecoder(decoder_, "QADD16_cccc01100010nnnndddd11110001mmmm_case_0 QADD16")
  {}

 private:
  nacl_arm_dec::QADD16_cccc01100010nnnndddd11110001mmmm_case_0_QADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQADD16_cccc01100010nnnndddd11110001mmmm_case_0_QADD16);
};

class NamedQADD8_cccc01100010nnnndddd11111001mmmm_case_0_QADD8
    : public NamedClassDecoder {
 public:
  NamedQADD8_cccc01100010nnnndddd11111001mmmm_case_0_QADD8()
    : NamedClassDecoder(decoder_, "QADD8_cccc01100010nnnndddd11111001mmmm_case_0 QADD8")
  {}

 private:
  nacl_arm_dec::QADD8_cccc01100010nnnndddd11111001mmmm_case_0_QADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQADD8_cccc01100010nnnndddd11111001mmmm_case_0_QADD8);
};

class NamedQADD_cccc00010000nnnndddd00000101mmmm_case_0_QADD
    : public NamedClassDecoder {
 public:
  NamedQADD_cccc00010000nnnndddd00000101mmmm_case_0_QADD()
    : NamedClassDecoder(decoder_, "QADD_cccc00010000nnnndddd00000101mmmm_case_0 QADD")
  {}

 private:
  nacl_arm_dec::QADD_cccc00010000nnnndddd00000101mmmm_case_0_QADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQADD_cccc00010000nnnndddd00000101mmmm_case_0_QADD);
};

class NamedQASX_cccc01100010nnnndddd11110011mmmm_case_0_QASX
    : public NamedClassDecoder {
 public:
  NamedQASX_cccc01100010nnnndddd11110011mmmm_case_0_QASX()
    : NamedClassDecoder(decoder_, "QASX_cccc01100010nnnndddd11110011mmmm_case_0 QASX")
  {}

 private:
  nacl_arm_dec::QASX_cccc01100010nnnndddd11110011mmmm_case_0_QASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQASX_cccc01100010nnnndddd11110011mmmm_case_0_QASX);
};

class NamedQDADD_cccc00010100nnnndddd00000101mmmm_case_0_QDADD
    : public NamedClassDecoder {
 public:
  NamedQDADD_cccc00010100nnnndddd00000101mmmm_case_0_QDADD()
    : NamedClassDecoder(decoder_, "QDADD_cccc00010100nnnndddd00000101mmmm_case_0 QDADD")
  {}

 private:
  nacl_arm_dec::QDADD_cccc00010100nnnndddd00000101mmmm_case_0_QDADD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQDADD_cccc00010100nnnndddd00000101mmmm_case_0_QDADD);
};

class NamedQDSUB_cccc00010110nnnndddd00000101mmmm_case_0_QDSUB
    : public NamedClassDecoder {
 public:
  NamedQDSUB_cccc00010110nnnndddd00000101mmmm_case_0_QDSUB()
    : NamedClassDecoder(decoder_, "QDSUB_cccc00010110nnnndddd00000101mmmm_case_0 QDSUB")
  {}

 private:
  nacl_arm_dec::QDSUB_cccc00010110nnnndddd00000101mmmm_case_0_QDSUB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQDSUB_cccc00010110nnnndddd00000101mmmm_case_0_QDSUB);
};

class NamedQSAX_cccc01100010nnnndddd11110101mmmm_case_0_QSAX
    : public NamedClassDecoder {
 public:
  NamedQSAX_cccc01100010nnnndddd11110101mmmm_case_0_QSAX()
    : NamedClassDecoder(decoder_, "QSAX_cccc01100010nnnndddd11110101mmmm_case_0 QSAX")
  {}

 private:
  nacl_arm_dec::QSAX_cccc01100010nnnndddd11110101mmmm_case_0_QSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQSAX_cccc01100010nnnndddd11110101mmmm_case_0_QSAX);
};

class NamedQSUB16_cccc01100010nnnndddd11110111mmmm_case_0_QSUB16
    : public NamedClassDecoder {
 public:
  NamedQSUB16_cccc01100010nnnndddd11110111mmmm_case_0_QSUB16()
    : NamedClassDecoder(decoder_, "QSUB16_cccc01100010nnnndddd11110111mmmm_case_0 QSUB16")
  {}

 private:
  nacl_arm_dec::QSUB16_cccc01100010nnnndddd11110111mmmm_case_0_QSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQSUB16_cccc01100010nnnndddd11110111mmmm_case_0_QSUB16);
};

class NamedQSUB8_cccc01100010nnnndddd11111111mmmm_case_0_QSUB8
    : public NamedClassDecoder {
 public:
  NamedQSUB8_cccc01100010nnnndddd11111111mmmm_case_0_QSUB8()
    : NamedClassDecoder(decoder_, "QSUB8_cccc01100010nnnndddd11111111mmmm_case_0 QSUB8")
  {}

 private:
  nacl_arm_dec::QSUB8_cccc01100010nnnndddd11111111mmmm_case_0_QSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQSUB8_cccc01100010nnnndddd11111111mmmm_case_0_QSUB8);
};

class NamedQSUB_cccc00010010nnnndddd00000101mmmm_case_0_QSUB
    : public NamedClassDecoder {
 public:
  NamedQSUB_cccc00010010nnnndddd00000101mmmm_case_0_QSUB()
    : NamedClassDecoder(decoder_, "QSUB_cccc00010010nnnndddd00000101mmmm_case_0 QSUB")
  {}

 private:
  nacl_arm_dec::QSUB_cccc00010010nnnndddd00000101mmmm_case_0_QSUB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedQSUB_cccc00010010nnnndddd00000101mmmm_case_0_QSUB);
};

class NamedRBIT_cccc011011111111dddd11110011mmmm_case_0_RBIT
    : public NamedClassDecoder {
 public:
  NamedRBIT_cccc011011111111dddd11110011mmmm_case_0_RBIT()
    : NamedClassDecoder(decoder_, "RBIT_cccc011011111111dddd11110011mmmm_case_0 RBIT")
  {}

 private:
  nacl_arm_dec::RBIT_cccc011011111111dddd11110011mmmm_case_0_RBIT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedRBIT_cccc011011111111dddd11110011mmmm_case_0_RBIT);
};

class NamedREV16_cccc011010111111dddd11111011mmmm_case_0_REV16
    : public NamedClassDecoder {
 public:
  NamedREV16_cccc011010111111dddd11111011mmmm_case_0_REV16()
    : NamedClassDecoder(decoder_, "REV16_cccc011010111111dddd11111011mmmm_case_0 REV16")
  {}

 private:
  nacl_arm_dec::REV16_cccc011010111111dddd11111011mmmm_case_0_REV16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedREV16_cccc011010111111dddd11111011mmmm_case_0_REV16);
};

class NamedREVSH_cccc011011111111dddd11111011mmmm_case_0_REVSH
    : public NamedClassDecoder {
 public:
  NamedREVSH_cccc011011111111dddd11111011mmmm_case_0_REVSH()
    : NamedClassDecoder(decoder_, "REVSH_cccc011011111111dddd11111011mmmm_case_0 REVSH")
  {}

 private:
  nacl_arm_dec::REVSH_cccc011011111111dddd11111011mmmm_case_0_REVSH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedREVSH_cccc011011111111dddd11111011mmmm_case_0_REVSH);
};

class NamedREV_cccc011010111111dddd11110011mmmm_case_0_REV
    : public NamedClassDecoder {
 public:
  NamedREV_cccc011010111111dddd11110011mmmm_case_0_REV()
    : NamedClassDecoder(decoder_, "REV_cccc011010111111dddd11110011mmmm_case_0 REV")
  {}

 private:
  nacl_arm_dec::REV_cccc011010111111dddd11110011mmmm_case_0_REV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedREV_cccc011010111111dddd11110011mmmm_case_0_REV);
};

class NamedROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0_ROR_immediate
    : public NamedClassDecoder {
 public:
  NamedROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0_ROR_immediate()
    : NamedClassDecoder(decoder_, "ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0 ROR_immediate")
  {}

 private:
  nacl_arm_dec::ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0_ROR_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0_ROR_immediate);
};

class NamedROR_register_cccc0001101s0000ddddmmmm0111nnnn_case_0_ROR_register
    : public NamedClassDecoder {
 public:
  NamedROR_register_cccc0001101s0000ddddmmmm0111nnnn_case_0_ROR_register()
    : NamedClassDecoder(decoder_, "ROR_register_cccc0001101s0000ddddmmmm0111nnnn_case_0 ROR_register")
  {}

 private:
  nacl_arm_dec::ROR_register_cccc0001101s0000ddddmmmm0111nnnn_case_0_ROR_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedROR_register_cccc0001101s0000ddddmmmm0111nnnn_case_0_ROR_register);
};

class NamedRRX_cccc0001101s0000dddd00000110mmmm_case_0_RRX
    : public NamedClassDecoder {
 public:
  NamedRRX_cccc0001101s0000dddd00000110mmmm_case_0_RRX()
    : NamedClassDecoder(decoder_, "RRX_cccc0001101s0000dddd00000110mmmm_case_0 RRX")
  {}

 private:
  nacl_arm_dec::RRX_cccc0001101s0000dddd00000110mmmm_case_0_RRX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedRRX_cccc0001101s0000dddd00000110mmmm_case_0_RRX);
};

class NamedRSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0_RSB_register
    : public NamedClassDecoder {
 public:
  NamedRSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0_RSB_register()
    : NamedClassDecoder(decoder_, "RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0 RSB_register")
  {}

 private:
  nacl_arm_dec::RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0_RSB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedRSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0_RSB_register);
};

class NamedRSB_register_shfited_register_cccc0000011snnnnddddssss0tt1mmmm_case_0_RSB_register_shfited_register
    : public NamedClassDecoder {
 public:
  NamedRSB_register_shfited_register_cccc0000011snnnnddddssss0tt1mmmm_case_0_RSB_register_shfited_register()
    : NamedClassDecoder(decoder_, "RSB_register_shfited_register_cccc0000011snnnnddddssss0tt1mmmm_case_0 RSB_register_shfited_register")
  {}

 private:
  nacl_arm_dec::RSB_register_shfited_register_cccc0000011snnnnddddssss0tt1mmmm_case_0_RSB_register_shfited_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedRSB_register_shfited_register_cccc0000011snnnnddddssss0tt1mmmm_case_0_RSB_register_shfited_register);
};

class NamedRSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0_RSC_register
    : public NamedClassDecoder {
 public:
  NamedRSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0_RSC_register()
    : NamedClassDecoder(decoder_, "RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0 RSC_register")
  {}

 private:
  nacl_arm_dec::RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0_RSC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedRSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0_RSC_register);
};

class NamedRSC_register_shifted_register_cccc0000111snnnnddddssss0tt1mmmm_case_0_RSC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedRSC_register_shifted_register_cccc0000111snnnnddddssss0tt1mmmm_case_0_RSC_register_shifted_register()
    : NamedClassDecoder(decoder_, "RSC_register_shifted_register_cccc0000111snnnnddddssss0tt1mmmm_case_0 RSC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::RSC_register_shifted_register_cccc0000111snnnnddddssss0tt1mmmm_case_0_RSC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedRSC_register_shifted_register_cccc0000111snnnnddddssss0tt1mmmm_case_0_RSC_register_shifted_register);
};

class NamedSADD16_cccc01100001nnnndddd11110001mmmm_case_0_SADD16
    : public NamedClassDecoder {
 public:
  NamedSADD16_cccc01100001nnnndddd11110001mmmm_case_0_SADD16()
    : NamedClassDecoder(decoder_, "SADD16_cccc01100001nnnndddd11110001mmmm_case_0 SADD16")
  {}

 private:
  nacl_arm_dec::SADD16_cccc01100001nnnndddd11110001mmmm_case_0_SADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSADD16_cccc01100001nnnndddd11110001mmmm_case_0_SADD16);
};

class NamedSADD8_cccc01100001nnnndddd11111001mmmm_case_0_SADD8
    : public NamedClassDecoder {
 public:
  NamedSADD8_cccc01100001nnnndddd11111001mmmm_case_0_SADD8()
    : NamedClassDecoder(decoder_, "SADD8_cccc01100001nnnndddd11111001mmmm_case_0 SADD8")
  {}

 private:
  nacl_arm_dec::SADD8_cccc01100001nnnndddd11111001mmmm_case_0_SADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSADD8_cccc01100001nnnndddd11111001mmmm_case_0_SADD8);
};

class NamedSASX_cccc01100001nnnndddd11110011mmmm_case_0_SASX
    : public NamedClassDecoder {
 public:
  NamedSASX_cccc01100001nnnndddd11110011mmmm_case_0_SASX()
    : NamedClassDecoder(decoder_, "SASX_cccc01100001nnnndddd11110011mmmm_case_0 SASX")
  {}

 private:
  nacl_arm_dec::SASX_cccc01100001nnnndddd11110011mmmm_case_0_SASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSASX_cccc01100001nnnndddd11110011mmmm_case_0_SASX);
};

class NamedSBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0_SBC_register
    : public NamedClassDecoder {
 public:
  NamedSBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0_SBC_register()
    : NamedClassDecoder(decoder_, "SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0 SBC_register")
  {}

 private:
  nacl_arm_dec::SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0_SBC_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0_SBC_register);
};

class NamedSBC_register_shifted_register_cccc0000110snnnnddddssss0tt1mmmm_case_0_SBC_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedSBC_register_shifted_register_cccc0000110snnnnddddssss0tt1mmmm_case_0_SBC_register_shifted_register()
    : NamedClassDecoder(decoder_, "SBC_register_shifted_register_cccc0000110snnnnddddssss0tt1mmmm_case_0 SBC_register_shifted_register")
  {}

 private:
  nacl_arm_dec::SBC_register_shifted_register_cccc0000110snnnnddddssss0tt1mmmm_case_0_SBC_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSBC_register_shifted_register_cccc0000110snnnnddddssss0tt1mmmm_case_0_SBC_register_shifted_register);
};

class NamedSBFX_cccc0111101wwwwwddddlllll101nnnn_case_0_SBFX
    : public NamedClassDecoder {
 public:
  NamedSBFX_cccc0111101wwwwwddddlllll101nnnn_case_0_SBFX()
    : NamedClassDecoder(decoder_, "SBFX_cccc0111101wwwwwddddlllll101nnnn_case_0 SBFX")
  {}

 private:
  nacl_arm_dec::SBFX_cccc0111101wwwwwddddlllll101nnnn_case_0_SBFX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSBFX_cccc0111101wwwwwddddlllll101nnnn_case_0_SBFX);
};

class NamedSDIV_cccc01110001dddd1111mmmm0001nnnn_case_0_SDIV
    : public NamedClassDecoder {
 public:
  NamedSDIV_cccc01110001dddd1111mmmm0001nnnn_case_0_SDIV()
    : NamedClassDecoder(decoder_, "SDIV_cccc01110001dddd1111mmmm0001nnnn_case_0 SDIV")
  {}

 private:
  nacl_arm_dec::SDIV_cccc01110001dddd1111mmmm0001nnnn_case_0_SDIV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSDIV_cccc01110001dddd1111mmmm0001nnnn_case_0_SDIV);
};

class NamedSEL_cccc01101000nnnndddd11111011mmmm_case_0_SEL
    : public NamedClassDecoder {
 public:
  NamedSEL_cccc01101000nnnndddd11111011mmmm_case_0_SEL()
    : NamedClassDecoder(decoder_, "SEL_cccc01101000nnnndddd11111011mmmm_case_0 SEL")
  {}

 private:
  nacl_arm_dec::SEL_cccc01101000nnnndddd11111011mmmm_case_0_SEL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSEL_cccc01101000nnnndddd11111011mmmm_case_0_SEL);
};

class NamedSEV_cccc0011001000001111000000000100_case_0_SEV
    : public NamedClassDecoder {
 public:
  NamedSEV_cccc0011001000001111000000000100_case_0_SEV()
    : NamedClassDecoder(decoder_, "SEV_cccc0011001000001111000000000100_case_0 SEV")
  {}

 private:
  nacl_arm_dec::SEV_cccc0011001000001111000000000100_case_0_SEV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSEV_cccc0011001000001111000000000100_case_0_SEV);
};

class NamedSHADD16_cccc01100011nnnndddd11110001mmmm_case_0_SHADD16
    : public NamedClassDecoder {
 public:
  NamedSHADD16_cccc01100011nnnndddd11110001mmmm_case_0_SHADD16()
    : NamedClassDecoder(decoder_, "SHADD16_cccc01100011nnnndddd11110001mmmm_case_0 SHADD16")
  {}

 private:
  nacl_arm_dec::SHADD16_cccc01100011nnnndddd11110001mmmm_case_0_SHADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSHADD16_cccc01100011nnnndddd11110001mmmm_case_0_SHADD16);
};

class NamedSHADD8_cccc01100011nnnndddd11111001mmmm_case_0_SHADD8
    : public NamedClassDecoder {
 public:
  NamedSHADD8_cccc01100011nnnndddd11111001mmmm_case_0_SHADD8()
    : NamedClassDecoder(decoder_, "SHADD8_cccc01100011nnnndddd11111001mmmm_case_0 SHADD8")
  {}

 private:
  nacl_arm_dec::SHADD8_cccc01100011nnnndddd11111001mmmm_case_0_SHADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSHADD8_cccc01100011nnnndddd11111001mmmm_case_0_SHADD8);
};

class NamedSHASX_cccc01100011nnnndddd11110011mmmm_case_0_SHASX
    : public NamedClassDecoder {
 public:
  NamedSHASX_cccc01100011nnnndddd11110011mmmm_case_0_SHASX()
    : NamedClassDecoder(decoder_, "SHASX_cccc01100011nnnndddd11110011mmmm_case_0 SHASX")
  {}

 private:
  nacl_arm_dec::SHASX_cccc01100011nnnndddd11110011mmmm_case_0_SHASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSHASX_cccc01100011nnnndddd11110011mmmm_case_0_SHASX);
};

class NamedSHSAX_cccc01100011nnnndddd11110101mmmm_case_0_SHSAX
    : public NamedClassDecoder {
 public:
  NamedSHSAX_cccc01100011nnnndddd11110101mmmm_case_0_SHSAX()
    : NamedClassDecoder(decoder_, "SHSAX_cccc01100011nnnndddd11110101mmmm_case_0 SHSAX")
  {}

 private:
  nacl_arm_dec::SHSAX_cccc01100011nnnndddd11110101mmmm_case_0_SHSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSHSAX_cccc01100011nnnndddd11110101mmmm_case_0_SHSAX);
};

class NamedSHSUB16_cccc01100011nnnndddd11110111mmmm_case_0_SHSUB16
    : public NamedClassDecoder {
 public:
  NamedSHSUB16_cccc01100011nnnndddd11110111mmmm_case_0_SHSUB16()
    : NamedClassDecoder(decoder_, "SHSUB16_cccc01100011nnnndddd11110111mmmm_case_0 SHSUB16")
  {}

 private:
  nacl_arm_dec::SHSUB16_cccc01100011nnnndddd11110111mmmm_case_0_SHSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSHSUB16_cccc01100011nnnndddd11110111mmmm_case_0_SHSUB16);
};

class NamedSHSUB8_cccc01100011nnnndddd11111111mmmm_case_0_SHSUB8
    : public NamedClassDecoder {
 public:
  NamedSHSUB8_cccc01100011nnnndddd11111111mmmm_case_0_SHSUB8()
    : NamedClassDecoder(decoder_, "SHSUB8_cccc01100011nnnndddd11111111mmmm_case_0 SHSUB8")
  {}

 private:
  nacl_arm_dec::SHSUB8_cccc01100011nnnndddd11111111mmmm_case_0_SHSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSHSUB8_cccc01100011nnnndddd11111111mmmm_case_0_SHSUB8);
};

class NamedSMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0_SMLABB_SMLABT_SMLATB_SMLATT
    : public NamedClassDecoder {
 public:
  NamedSMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0_SMLABB_SMLABT_SMLATB_SMLATT()
    : NamedClassDecoder(decoder_, "SMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0 SMLABB_SMLABT_SMLATB_SMLATT")
  {}

 private:
  nacl_arm_dec::SMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0_SMLABB_SMLABT_SMLATB_SMLATT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0_SMLABB_SMLABT_SMLATB_SMLATT);
};

class NamedSMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0_SMLAD
    : public NamedClassDecoder {
 public:
  NamedSMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0_SMLAD()
    : NamedClassDecoder(decoder_, "SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0 SMLAD")
  {}

 private:
  nacl_arm_dec::SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0_SMLAD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0_SMLAD);
};

class NamedSMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0_SMLALBB_SMLALBT_SMLALTB_SMLALTT
    : public NamedClassDecoder {
 public:
  NamedSMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0_SMLALBB_SMLALBT_SMLALTB_SMLALTT()
    : NamedClassDecoder(decoder_, "SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0 SMLALBB_SMLALBT_SMLALTB_SMLALTT")
  {}

 private:
  nacl_arm_dec::SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0_SMLALBB_SMLALBT_SMLALTB_SMLALTT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0_SMLALBB_SMLALBT_SMLALTB_SMLALTT);
};

class NamedSMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0_SMLALD
    : public NamedClassDecoder {
 public:
  NamedSMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0_SMLALD()
    : NamedClassDecoder(decoder_, "SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0 SMLALD")
  {}

 private:
  nacl_arm_dec::SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0_SMLALD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0_SMLALD);
};

class NamedSMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0_SMLAWB_SMLAWT
    : public NamedClassDecoder {
 public:
  NamedSMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0_SMLAWB_SMLAWT()
    : NamedClassDecoder(decoder_, "SMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0 SMLAWB_SMLAWT")
  {}

 private:
  nacl_arm_dec::SMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0_SMLAWB_SMLAWT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0_SMLAWB_SMLAWT);
};

class NamedSMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0_SMLSD
    : public NamedClassDecoder {
 public:
  NamedSMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0_SMLSD()
    : NamedClassDecoder(decoder_, "SMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0 SMLSD")
  {}

 private:
  nacl_arm_dec::SMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0_SMLSD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0_SMLSD);
};

class NamedSMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0_SMLSLD
    : public NamedClassDecoder {
 public:
  NamedSMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0_SMLSLD()
    : NamedClassDecoder(decoder_, "SMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0 SMLSLD")
  {}

 private:
  nacl_arm_dec::SMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0_SMLSLD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0_SMLSLD);
};

class NamedSMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0_SMMLA
    : public NamedClassDecoder {
 public:
  NamedSMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0_SMMLA()
    : NamedClassDecoder(decoder_, "SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0 SMMLA")
  {}

 private:
  nacl_arm_dec::SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0_SMMLA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0_SMMLA);
};

class NamedSMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0_SMMLS
    : public NamedClassDecoder {
 public:
  NamedSMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0_SMMLS()
    : NamedClassDecoder(decoder_, "SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0 SMMLS")
  {}

 private:
  nacl_arm_dec::SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0_SMMLS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0_SMMLS);
};

class NamedSMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0_SMMUL
    : public NamedClassDecoder {
 public:
  NamedSMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0_SMMUL()
    : NamedClassDecoder(decoder_, "SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0 SMMUL")
  {}

 private:
  nacl_arm_dec::SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0_SMMUL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0_SMMUL);
};

class NamedSMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0_SMUAD
    : public NamedClassDecoder {
 public:
  NamedSMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0_SMUAD()
    : NamedClassDecoder(decoder_, "SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0 SMUAD")
  {}

 private:
  nacl_arm_dec::SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0_SMUAD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0_SMUAD);
};

class NamedSMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0_SMULBB_SMULBT_SMULTB_SMULTT
    : public NamedClassDecoder {
 public:
  NamedSMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0_SMULBB_SMULBT_SMULTB_SMULTT()
    : NamedClassDecoder(decoder_, "SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0 SMULBB_SMULBT_SMULTB_SMULTT")
  {}

 private:
  nacl_arm_dec::SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0_SMULBB_SMULBT_SMULTB_SMULTT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0_SMULBB_SMULBT_SMULTB_SMULTT);
};

class NamedSMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0_SMULWB_SMULWT
    : public NamedClassDecoder {
 public:
  NamedSMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0_SMULWB_SMULWT()
    : NamedClassDecoder(decoder_, "SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0 SMULWB_SMULWT")
  {}

 private:
  nacl_arm_dec::SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0_SMULWB_SMULWT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0_SMULWB_SMULWT);
};

class NamedSMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0_SMUSD
    : public NamedClassDecoder {
 public:
  NamedSMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0_SMUSD()
    : NamedClassDecoder(decoder_, "SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0 SMUSD")
  {}

 private:
  nacl_arm_dec::SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0_SMUSD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0_SMUSD);
};

class NamedSSAT16_cccc01101010iiiidddd11110011nnnn_case_0_SSAT16
    : public NamedClassDecoder {
 public:
  NamedSSAT16_cccc01101010iiiidddd11110011nnnn_case_0_SSAT16()
    : NamedClassDecoder(decoder_, "SSAT16_cccc01101010iiiidddd11110011nnnn_case_0 SSAT16")
  {}

 private:
  nacl_arm_dec::SSAT16_cccc01101010iiiidddd11110011nnnn_case_0_SSAT16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSSAT16_cccc01101010iiiidddd11110011nnnn_case_0_SSAT16);
};

class NamedSSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0_SSAT
    : public NamedClassDecoder {
 public:
  NamedSSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0_SSAT()
    : NamedClassDecoder(decoder_, "SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0 SSAT")
  {}

 private:
  nacl_arm_dec::SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0_SSAT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0_SSAT);
};

class NamedSSAX_cccc01100001nnnndddd11110101mmmm_case_0_SSAX
    : public NamedClassDecoder {
 public:
  NamedSSAX_cccc01100001nnnndddd11110101mmmm_case_0_SSAX()
    : NamedClassDecoder(decoder_, "SSAX_cccc01100001nnnndddd11110101mmmm_case_0 SSAX")
  {}

 private:
  nacl_arm_dec::SSAX_cccc01100001nnnndddd11110101mmmm_case_0_SSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSSAX_cccc01100001nnnndddd11110101mmmm_case_0_SSAX);
};

class NamedSSSUB16_cccc01100001nnnndddd11110111mmmm_case_0_SSSUB16
    : public NamedClassDecoder {
 public:
  NamedSSSUB16_cccc01100001nnnndddd11110111mmmm_case_0_SSSUB16()
    : NamedClassDecoder(decoder_, "SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0 SSSUB16")
  {}

 private:
  nacl_arm_dec::SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0_SSSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSSSUB16_cccc01100001nnnndddd11110111mmmm_case_0_SSSUB16);
};

class NamedSSUB8_cccc01100001nnnndddd11111111mmmm_case_0_SSUB8
    : public NamedClassDecoder {
 public:
  NamedSSUB8_cccc01100001nnnndddd11111111mmmm_case_0_SSUB8()
    : NamedClassDecoder(decoder_, "SSUB8_cccc01100001nnnndddd11111111mmmm_case_0 SSUB8")
  {}

 private:
  nacl_arm_dec::SSUB8_cccc01100001nnnndddd11111111mmmm_case_0_SSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSSUB8_cccc01100001nnnndddd11111111mmmm_case_0_SSUB8);
};

class NamedSTMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDA_STMED
    : public NamedClassDecoder {
 public:
  NamedSTMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDA_STMED()
    : NamedClassDecoder(decoder_, "STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0 STMDA_STMED")
  {}

 private:
  nacl_arm_dec::STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDA_STMED decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDA_STMED);
};

class NamedSTMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDB_STMFD
    : public NamedClassDecoder {
 public:
  NamedSTMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDB_STMFD()
    : NamedClassDecoder(decoder_, "STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0 STMDB_STMFD")
  {}

 private:
  nacl_arm_dec::STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDB_STMFD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0_STMDB_STMFD);
};

class NamedSTMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0_STMIB_STMFA
    : public NamedClassDecoder {
 public:
  NamedSTMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0_STMIB_STMFA()
    : NamedClassDecoder(decoder_, "STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0 STMIB_STMFA")
  {}

 private:
  nacl_arm_dec::STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0_STMIB_STMFA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0_STMIB_STMFA);
};

class NamedSTM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0_STM_STMIA_STMEA
    : public NamedClassDecoder {
 public:
  NamedSTM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0_STM_STMIA_STMEA()
    : NamedClassDecoder(decoder_, "STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0 STM_STMIA_STMEA")
  {}

 private:
  nacl_arm_dec::STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0_STM_STMIA_STMEA decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0_STM_STMIA_STMEA);
};

class NamedSTREXB_cccc00011100nnnndddd11111001tttt_case_0_STREXB
    : public NamedClassDecoder {
 public:
  NamedSTREXB_cccc00011100nnnndddd11111001tttt_case_0_STREXB()
    : NamedClassDecoder(decoder_, "STREXB_cccc00011100nnnndddd11111001tttt_case_0 STREXB")
  {}

 private:
  nacl_arm_dec::STREXB_cccc00011100nnnndddd11111001tttt_case_0_STREXB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTREXB_cccc00011100nnnndddd11111001tttt_case_0_STREXB);
};

class NamedSTREXD_cccc00011010nnnndddd11111001tttt_case_0_STREXD
    : public NamedClassDecoder {
 public:
  NamedSTREXD_cccc00011010nnnndddd11111001tttt_case_0_STREXD()
    : NamedClassDecoder(decoder_, "STREXD_cccc00011010nnnndddd11111001tttt_case_0 STREXD")
  {}

 private:
  nacl_arm_dec::STREXD_cccc00011010nnnndddd11111001tttt_case_0_STREXD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTREXD_cccc00011010nnnndddd11111001tttt_case_0_STREXD);
};

class NamedSTREXH_cccc00011110nnnndddd11111001tttt_case_0_STREXH
    : public NamedClassDecoder {
 public:
  NamedSTREXH_cccc00011110nnnndddd11111001tttt_case_0_STREXH()
    : NamedClassDecoder(decoder_, "STREXH_cccc00011110nnnndddd11111001tttt_case_0 STREXH")
  {}

 private:
  nacl_arm_dec::STREXH_cccc00011110nnnndddd11111001tttt_case_0_STREXH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTREXH_cccc00011110nnnndddd11111001tttt_case_0_STREXH);
};

class NamedSTREXH_cccc00011111nnnntttt111110011111_case_0_STREXH
    : public NamedClassDecoder {
 public:
  NamedSTREXH_cccc00011111nnnntttt111110011111_case_0_STREXH()
    : NamedClassDecoder(decoder_, "STREXH_cccc00011111nnnntttt111110011111_case_0 STREXH")
  {}

 private:
  nacl_arm_dec::STREXH_cccc00011111nnnntttt111110011111_case_0_STREXH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTREXH_cccc00011111nnnntttt111110011111_case_0_STREXH);
};

class NamedSTREX_cccc00011000nnnndddd11111001tttt_case_0_STREX
    : public NamedClassDecoder {
 public:
  NamedSTREX_cccc00011000nnnndddd11111001tttt_case_0_STREX()
    : NamedClassDecoder(decoder_, "STREX_cccc00011000nnnndddd11111001tttt_case_0 STREX")
  {}

 private:
  nacl_arm_dec::STREX_cccc00011000nnnndddd11111001tttt_case_0_STREX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSTREX_cccc00011000nnnndddd11111001tttt_case_0_STREX);
};

class NamedSUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0_SUB_register
    : public NamedClassDecoder {
 public:
  NamedSUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0_SUB_register()
    : NamedClassDecoder(decoder_, "SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0 SUB_register")
  {}

 private:
  nacl_arm_dec::SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0_SUB_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0_SUB_register);
};

class NamedSUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0_SUB_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedSUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0_SUB_register_shifted_register()
    : NamedClassDecoder(decoder_, "SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0 SUB_register_shifted_register")
  {}

 private:
  nacl_arm_dec::SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0_SUB_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0_SUB_register_shifted_register);
};

class NamedSXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0_SXTAB16
    : public NamedClassDecoder {
 public:
  NamedSXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0_SXTAB16()
    : NamedClassDecoder(decoder_, "SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0 SXTAB16")
  {}

 private:
  nacl_arm_dec::SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0_SXTAB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0_SXTAB16);
};

class NamedSXTAB_cccc01101010nnnnddddrr000111mmmm_case_0_SXTAB
    : public NamedClassDecoder {
 public:
  NamedSXTAB_cccc01101010nnnnddddrr000111mmmm_case_0_SXTAB()
    : NamedClassDecoder(decoder_, "SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0 SXTAB")
  {}

 private:
  nacl_arm_dec::SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0_SXTAB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSXTAB_cccc01101010nnnnddddrr000111mmmm_case_0_SXTAB);
};

class NamedSXTAH_cccc01101011nnnnddddrr000111mmmm_case_0_SXTAH
    : public NamedClassDecoder {
 public:
  NamedSXTAH_cccc01101011nnnnddddrr000111mmmm_case_0_SXTAH()
    : NamedClassDecoder(decoder_, "SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0 SXTAH")
  {}

 private:
  nacl_arm_dec::SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0_SXTAH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSXTAH_cccc01101011nnnnddddrr000111mmmm_case_0_SXTAH);
};

class NamedSXTB16_cccc011010001111ddddrr000111mmmm_case_0_SXTB16
    : public NamedClassDecoder {
 public:
  NamedSXTB16_cccc011010001111ddddrr000111mmmm_case_0_SXTB16()
    : NamedClassDecoder(decoder_, "SXTB16_cccc011010001111ddddrr000111mmmm_case_0 SXTB16")
  {}

 private:
  nacl_arm_dec::SXTB16_cccc011010001111ddddrr000111mmmm_case_0_SXTB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSXTB16_cccc011010001111ddddrr000111mmmm_case_0_SXTB16);
};

class NamedSXTB_cccc011010101111ddddrr000111mmmm_case_0_SXTB
    : public NamedClassDecoder {
 public:
  NamedSXTB_cccc011010101111ddddrr000111mmmm_case_0_SXTB()
    : NamedClassDecoder(decoder_, "SXTB_cccc011010101111ddddrr000111mmmm_case_0 SXTB")
  {}

 private:
  nacl_arm_dec::SXTB_cccc011010101111ddddrr000111mmmm_case_0_SXTB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSXTB_cccc011010101111ddddrr000111mmmm_case_0_SXTB);
};

class NamedSXTH_cccc011010111111ddddrr000111mmmm_case_0_SXTH
    : public NamedClassDecoder {
 public:
  NamedSXTH_cccc011010111111ddddrr000111mmmm_case_0_SXTH()
    : NamedClassDecoder(decoder_, "SXTH_cccc011010111111ddddrr000111mmmm_case_0 SXTH")
  {}

 private:
  nacl_arm_dec::SXTH_cccc011010111111ddddrr000111mmmm_case_0_SXTH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedSXTH_cccc011010111111ddddrr000111mmmm_case_0_SXTH);
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

class NamedTEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0_TEQ_register
    : public NamedClassDecoder {
 public:
  NamedTEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0_TEQ_register()
    : NamedClassDecoder(decoder_, "TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0 TEQ_register")
  {}

 private:
  nacl_arm_dec::TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0_TEQ_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedTEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0_TEQ_register);
};

class NamedTEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0_TEQ_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedTEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0_TEQ_register_shifted_register()
    : NamedClassDecoder(decoder_, "TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0 TEQ_register_shifted_register")
  {}

 private:
  nacl_arm_dec::TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0_TEQ_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedTEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0_TEQ_register_shifted_register);
};

class NamedTST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0_TST_register
    : public NamedClassDecoder {
 public:
  NamedTST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0_TST_register()
    : NamedClassDecoder(decoder_, "TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0 TST_register")
  {}

 private:
  nacl_arm_dec::TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0_TST_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedTST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0_TST_register);
};

class NamedTST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0_TST_register_shifted_register
    : public NamedClassDecoder {
 public:
  NamedTST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0_TST_register_shifted_register()
    : NamedClassDecoder(decoder_, "TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0 TST_register_shifted_register")
  {}

 private:
  nacl_arm_dec::TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0_TST_register_shifted_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedTST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0_TST_register_shifted_register);
};

class NamedUADD16_cccc01100101nnnndddd11110001mmmm_case_0_UADD16
    : public NamedClassDecoder {
 public:
  NamedUADD16_cccc01100101nnnndddd11110001mmmm_case_0_UADD16()
    : NamedClassDecoder(decoder_, "UADD16_cccc01100101nnnndddd11110001mmmm_case_0 UADD16")
  {}

 private:
  nacl_arm_dec::UADD16_cccc01100101nnnndddd11110001mmmm_case_0_UADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUADD16_cccc01100101nnnndddd11110001mmmm_case_0_UADD16);
};

class NamedUADD8_cccc01100101nnnndddd11111001mmmm_case_0_UADD8
    : public NamedClassDecoder {
 public:
  NamedUADD8_cccc01100101nnnndddd11111001mmmm_case_0_UADD8()
    : NamedClassDecoder(decoder_, "UADD8_cccc01100101nnnndddd11111001mmmm_case_0 UADD8")
  {}

 private:
  nacl_arm_dec::UADD8_cccc01100101nnnndddd11111001mmmm_case_0_UADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUADD8_cccc01100101nnnndddd11111001mmmm_case_0_UADD8);
};

class NamedUASX_cccc01100101nnnndddd11110011mmmm_case_0_UASX
    : public NamedClassDecoder {
 public:
  NamedUASX_cccc01100101nnnndddd11110011mmmm_case_0_UASX()
    : NamedClassDecoder(decoder_, "UASX_cccc01100101nnnndddd11110011mmmm_case_0 UASX")
  {}

 private:
  nacl_arm_dec::UASX_cccc01100101nnnndddd11110011mmmm_case_0_UASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUASX_cccc01100101nnnndddd11110011mmmm_case_0_UASX);
};

class NamedUBFX_cccc0111111mmmmmddddlllll101nnnn_case_0_UBFX
    : public NamedClassDecoder {
 public:
  NamedUBFX_cccc0111111mmmmmddddlllll101nnnn_case_0_UBFX()
    : NamedClassDecoder(decoder_, "UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0 UBFX")
  {}

 private:
  nacl_arm_dec::UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0_UBFX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUBFX_cccc0111111mmmmmddddlllll101nnnn_case_0_UBFX);
};

class NamedUDIV_cccc01110011dddd1111mmmm0001nnnn_case_0_UDIV
    : public NamedClassDecoder {
 public:
  NamedUDIV_cccc01110011dddd1111mmmm0001nnnn_case_0_UDIV()
    : NamedClassDecoder(decoder_, "UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0 UDIV")
  {}

 private:
  nacl_arm_dec::UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0_UDIV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUDIV_cccc01110011dddd1111mmmm0001nnnn_case_0_UDIV);
};

class NamedUHADD16_cccc01100111nnnndddd11110001mmmm_case_0_UHADD16
    : public NamedClassDecoder {
 public:
  NamedUHADD16_cccc01100111nnnndddd11110001mmmm_case_0_UHADD16()
    : NamedClassDecoder(decoder_, "UHADD16_cccc01100111nnnndddd11110001mmmm_case_0 UHADD16")
  {}

 private:
  nacl_arm_dec::UHADD16_cccc01100111nnnndddd11110001mmmm_case_0_UHADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUHADD16_cccc01100111nnnndddd11110001mmmm_case_0_UHADD16);
};

class NamedUHADD8_cccc01100111nnnndddd11111001mmmm_case_0_UHADD8
    : public NamedClassDecoder {
 public:
  NamedUHADD8_cccc01100111nnnndddd11111001mmmm_case_0_UHADD8()
    : NamedClassDecoder(decoder_, "UHADD8_cccc01100111nnnndddd11111001mmmm_case_0 UHADD8")
  {}

 private:
  nacl_arm_dec::UHADD8_cccc01100111nnnndddd11111001mmmm_case_0_UHADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUHADD8_cccc01100111nnnndddd11111001mmmm_case_0_UHADD8);
};

class NamedUHASX_cccc01100111nnnndddd11110011mmmm_case_0_UHASX
    : public NamedClassDecoder {
 public:
  NamedUHASX_cccc01100111nnnndddd11110011mmmm_case_0_UHASX()
    : NamedClassDecoder(decoder_, "UHASX_cccc01100111nnnndddd11110011mmmm_case_0 UHASX")
  {}

 private:
  nacl_arm_dec::UHASX_cccc01100111nnnndddd11110011mmmm_case_0_UHASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUHASX_cccc01100111nnnndddd11110011mmmm_case_0_UHASX);
};

class NamedUHSAX_cccc01100111nnnndddd11110101mmmm_case_0_UHSAX
    : public NamedClassDecoder {
 public:
  NamedUHSAX_cccc01100111nnnndddd11110101mmmm_case_0_UHSAX()
    : NamedClassDecoder(decoder_, "UHSAX_cccc01100111nnnndddd11110101mmmm_case_0 UHSAX")
  {}

 private:
  nacl_arm_dec::UHSAX_cccc01100111nnnndddd11110101mmmm_case_0_UHSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUHSAX_cccc01100111nnnndddd11110101mmmm_case_0_UHSAX);
};

class NamedUHSUB16_cccc01100111nnnndddd11110111mmmm_case_0_UHSUB16
    : public NamedClassDecoder {
 public:
  NamedUHSUB16_cccc01100111nnnndddd11110111mmmm_case_0_UHSUB16()
    : NamedClassDecoder(decoder_, "UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0 UHSUB16")
  {}

 private:
  nacl_arm_dec::UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0_UHSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUHSUB16_cccc01100111nnnndddd11110111mmmm_case_0_UHSUB16);
};

class NamedUHSUB8_cccc01100111nnnndddd11111111mmmm_case_0_UHSUB8
    : public NamedClassDecoder {
 public:
  NamedUHSUB8_cccc01100111nnnndddd11111111mmmm_case_0_UHSUB8()
    : NamedClassDecoder(decoder_, "UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0 UHSUB8")
  {}

 private:
  nacl_arm_dec::UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0_UHSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUHSUB8_cccc01100111nnnndddd11111111mmmm_case_0_UHSUB8);
};

class NamedUQADD16_cccc01100110nnnndddd11110001mmmm_case_0_UQADD16
    : public NamedClassDecoder {
 public:
  NamedUQADD16_cccc01100110nnnndddd11110001mmmm_case_0_UQADD16()
    : NamedClassDecoder(decoder_, "UQADD16_cccc01100110nnnndddd11110001mmmm_case_0 UQADD16")
  {}

 private:
  nacl_arm_dec::UQADD16_cccc01100110nnnndddd11110001mmmm_case_0_UQADD16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUQADD16_cccc01100110nnnndddd11110001mmmm_case_0_UQADD16);
};

class NamedUQADD8_cccc01100110nnnndddd11111001mmmm_case_0_UQADD8
    : public NamedClassDecoder {
 public:
  NamedUQADD8_cccc01100110nnnndddd11111001mmmm_case_0_UQADD8()
    : NamedClassDecoder(decoder_, "UQADD8_cccc01100110nnnndddd11111001mmmm_case_0 UQADD8")
  {}

 private:
  nacl_arm_dec::UQADD8_cccc01100110nnnndddd11111001mmmm_case_0_UQADD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUQADD8_cccc01100110nnnndddd11111001mmmm_case_0_UQADD8);
};

class NamedUQASX_cccc01100110nnnndddd11110011mmmm_case_0_UQASX
    : public NamedClassDecoder {
 public:
  NamedUQASX_cccc01100110nnnndddd11110011mmmm_case_0_UQASX()
    : NamedClassDecoder(decoder_, "UQASX_cccc01100110nnnndddd11110011mmmm_case_0 UQASX")
  {}

 private:
  nacl_arm_dec::UQASX_cccc01100110nnnndddd11110011mmmm_case_0_UQASX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUQASX_cccc01100110nnnndddd11110011mmmm_case_0_UQASX);
};

class NamedUQSAX_cccc01100110nnnndddd11110101mmmm_case_0_UQSAX
    : public NamedClassDecoder {
 public:
  NamedUQSAX_cccc01100110nnnndddd11110101mmmm_case_0_UQSAX()
    : NamedClassDecoder(decoder_, "UQSAX_cccc01100110nnnndddd11110101mmmm_case_0 UQSAX")
  {}

 private:
  nacl_arm_dec::UQSAX_cccc01100110nnnndddd11110101mmmm_case_0_UQSAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUQSAX_cccc01100110nnnndddd11110101mmmm_case_0_UQSAX);
};

class NamedUQSUB16_cccc01100110nnnndddd11110111mmmm_case_0_UQSUB16
    : public NamedClassDecoder {
 public:
  NamedUQSUB16_cccc01100110nnnndddd11110111mmmm_case_0_UQSUB16()
    : NamedClassDecoder(decoder_, "UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0 UQSUB16")
  {}

 private:
  nacl_arm_dec::UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0_UQSUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUQSUB16_cccc01100110nnnndddd11110111mmmm_case_0_UQSUB16);
};

class NamedUQSUB8_cccc01100110nnnndddd11111111mmmm_case_0_UQSUB8
    : public NamedClassDecoder {
 public:
  NamedUQSUB8_cccc01100110nnnndddd11111111mmmm_case_0_UQSUB8()
    : NamedClassDecoder(decoder_, "UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0 UQSUB8")
  {}

 private:
  nacl_arm_dec::UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0_UQSUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUQSUB8_cccc01100110nnnndddd11111111mmmm_case_0_UQSUB8);
};

class NamedUSAD8_cccc01111000dddd1111mmmm0001nnnn_case_0_USAD8
    : public NamedClassDecoder {
 public:
  NamedUSAD8_cccc01111000dddd1111mmmm0001nnnn_case_0_USAD8()
    : NamedClassDecoder(decoder_, "USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0 USAD8")
  {}

 private:
  nacl_arm_dec::USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0_USAD8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUSAD8_cccc01111000dddd1111mmmm0001nnnn_case_0_USAD8);
};

class NamedUSADA8_cccc01111000ddddaaaammmm0001nnnn_case_0_USADA8
    : public NamedClassDecoder {
 public:
  NamedUSADA8_cccc01111000ddddaaaammmm0001nnnn_case_0_USADA8()
    : NamedClassDecoder(decoder_, "USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0 USADA8")
  {}

 private:
  nacl_arm_dec::USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0_USADA8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUSADA8_cccc01111000ddddaaaammmm0001nnnn_case_0_USADA8);
};

class NamedUSAT16_cccc01101110iiiidddd11110011nnnn_case_0_USAT16
    : public NamedClassDecoder {
 public:
  NamedUSAT16_cccc01101110iiiidddd11110011nnnn_case_0_USAT16()
    : NamedClassDecoder(decoder_, "USAT16_cccc01101110iiiidddd11110011nnnn_case_0 USAT16")
  {}

 private:
  nacl_arm_dec::USAT16_cccc01101110iiiidddd11110011nnnn_case_0_USAT16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUSAT16_cccc01101110iiiidddd11110011nnnn_case_0_USAT16);
};

class NamedUSAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0_USAT
    : public NamedClassDecoder {
 public:
  NamedUSAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0_USAT()
    : NamedClassDecoder(decoder_, "USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0 USAT")
  {}

 private:
  nacl_arm_dec::USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0_USAT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUSAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0_USAT);
};

class NamedUSAX_cccc01100101nnnndddd11110101mmmm_case_0_USAX
    : public NamedClassDecoder {
 public:
  NamedUSAX_cccc01100101nnnndddd11110101mmmm_case_0_USAX()
    : NamedClassDecoder(decoder_, "USAX_cccc01100101nnnndddd11110101mmmm_case_0 USAX")
  {}

 private:
  nacl_arm_dec::USAX_cccc01100101nnnndddd11110101mmmm_case_0_USAX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUSAX_cccc01100101nnnndddd11110101mmmm_case_0_USAX);
};

class NamedUSUB16_cccc01100101nnnndddd11110111mmmm_case_0_USUB16
    : public NamedClassDecoder {
 public:
  NamedUSUB16_cccc01100101nnnndddd11110111mmmm_case_0_USUB16()
    : NamedClassDecoder(decoder_, "USUB16_cccc01100101nnnndddd11110111mmmm_case_0 USUB16")
  {}

 private:
  nacl_arm_dec::USUB16_cccc01100101nnnndddd11110111mmmm_case_0_USUB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUSUB16_cccc01100101nnnndddd11110111mmmm_case_0_USUB16);
};

class NamedUSUB8_cccc01100101nnnndddd11111111mmmm_case_0_USUB8
    : public NamedClassDecoder {
 public:
  NamedUSUB8_cccc01100101nnnndddd11111111mmmm_case_0_USUB8()
    : NamedClassDecoder(decoder_, "USUB8_cccc01100101nnnndddd11111111mmmm_case_0 USUB8")
  {}

 private:
  nacl_arm_dec::USUB8_cccc01100101nnnndddd11111111mmmm_case_0_USUB8 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUSUB8_cccc01100101nnnndddd11111111mmmm_case_0_USUB8);
};

class NamedUXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0_UXTAB16
    : public NamedClassDecoder {
 public:
  NamedUXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0_UXTAB16()
    : NamedClassDecoder(decoder_, "UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0 UXTAB16")
  {}

 private:
  nacl_arm_dec::UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0_UXTAB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0_UXTAB16);
};

class NamedUXTAB_cccc01101110nnnnddddrr000111mmmm_case_0_UXTAB
    : public NamedClassDecoder {
 public:
  NamedUXTAB_cccc01101110nnnnddddrr000111mmmm_case_0_UXTAB()
    : NamedClassDecoder(decoder_, "UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0 UXTAB")
  {}

 private:
  nacl_arm_dec::UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0_UXTAB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUXTAB_cccc01101110nnnnddddrr000111mmmm_case_0_UXTAB);
};

class NamedUXTAH_cccc01101111nnnnddddrr000111mmmm_case_0_UXTAH
    : public NamedClassDecoder {
 public:
  NamedUXTAH_cccc01101111nnnnddddrr000111mmmm_case_0_UXTAH()
    : NamedClassDecoder(decoder_, "UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0 UXTAH")
  {}

 private:
  nacl_arm_dec::UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0_UXTAH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUXTAH_cccc01101111nnnnddddrr000111mmmm_case_0_UXTAH);
};

class NamedUXTB16_cccc011011001111ddddrr000111mmmm_case_0_UXTB16
    : public NamedClassDecoder {
 public:
  NamedUXTB16_cccc011011001111ddddrr000111mmmm_case_0_UXTB16()
    : NamedClassDecoder(decoder_, "UXTB16_cccc011011001111ddddrr000111mmmm_case_0 UXTB16")
  {}

 private:
  nacl_arm_dec::UXTB16_cccc011011001111ddddrr000111mmmm_case_0_UXTB16 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUXTB16_cccc011011001111ddddrr000111mmmm_case_0_UXTB16);
};

class NamedUXTB_cccc011011101111ddddrr000111mmmm_case_0_UXTB
    : public NamedClassDecoder {
 public:
  NamedUXTB_cccc011011101111ddddrr000111mmmm_case_0_UXTB()
    : NamedClassDecoder(decoder_, "UXTB_cccc011011101111ddddrr000111mmmm_case_0 UXTB")
  {}

 private:
  nacl_arm_dec::UXTB_cccc011011101111ddddrr000111mmmm_case_0_UXTB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUXTB_cccc011011101111ddddrr000111mmmm_case_0_UXTB);
};

class NamedUXTH_cccc011011111111ddddrr000111mmmm_case_0_UXTH
    : public NamedClassDecoder {
 public:
  NamedUXTH_cccc011011111111ddddrr000111mmmm_case_0_UXTH()
    : NamedClassDecoder(decoder_, "UXTH_cccc011011111111ddddrr000111mmmm_case_0 UXTH")
  {}

 private:
  nacl_arm_dec::UXTH_cccc011011111111ddddrr000111mmmm_case_0_UXTH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUXTH_cccc011011111111ddddrr000111mmmm_case_0_UXTH);
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

class NamedUnnamed_case_0_None
    : public NamedClassDecoder {
 public:
  NamedUnnamed_case_0_None()
    : NamedClassDecoder(decoder_, "Unnamed_case_0 None")
  {}

 private:
  nacl_arm_dec::Unnamed_case_0_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedUnnamed_case_0_None);
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

class NamedVectorBinary3RegisterImmOp_VEXT
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterImmOp_VEXT()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterImmOp VEXT")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterImmOp_VEXT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterImmOp_VEXT);
};

class NamedVectorBinary3RegisterLookupOp_VTBL_VTBX
    : public NamedClassDecoder {
 public:
  NamedVectorBinary3RegisterLookupOp_VTBL_VTBX()
    : NamedClassDecoder(decoder_, "VectorBinary3RegisterLookupOp VTBL_VTBX")
  {}

 private:
  nacl_arm_dec::VectorBinary3RegisterLookupOp_VTBL_VTBX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorBinary3RegisterLookupOp_VTBL_VTBX);
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

class NamedVectorUnary2RegisterDup_VDUP_scalar
    : public NamedClassDecoder {
 public:
  NamedVectorUnary2RegisterDup_VDUP_scalar()
    : NamedClassDecoder(decoder_, "VectorUnary2RegisterDup VDUP_scalar")
  {}

 private:
  nacl_arm_dec::VectorUnary2RegisterDup_VDUP_scalar decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedVectorUnary2RegisterDup_VDUP_scalar);
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

class NamedWFE_cccc0011001000001111000000000010_case_0_WFE
    : public NamedClassDecoder {
 public:
  NamedWFE_cccc0011001000001111000000000010_case_0_WFE()
    : NamedClassDecoder(decoder_, "WFE_cccc0011001000001111000000000010_case_0 WFE")
  {}

 private:
  nacl_arm_dec::WFE_cccc0011001000001111000000000010_case_0_WFE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedWFE_cccc0011001000001111000000000010_case_0_WFE);
};

class NamedWFI_cccc0011001000001111000000000011_case_0_WFI
    : public NamedClassDecoder {
 public:
  NamedWFI_cccc0011001000001111000000000011_case_0_WFI()
    : NamedClassDecoder(decoder_, "WFI_cccc0011001000001111000000000011_case_0 WFI")
  {}

 private:
  nacl_arm_dec::WFI_cccc0011001000001111000000000011_case_0_WFI decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedWFI_cccc0011001000001111000000000011_case_0_WFI);
};

class NamedYIELD_cccc0011001000001111000000000001_case_0_YIELD
    : public NamedClassDecoder {
 public:
  NamedYIELD_cccc0011001000001111000000000001_case_0_YIELD()
    : NamedClassDecoder(decoder_, "YIELD_cccc0011001000001111000000000001_case_0 YIELD")
  {}

 private:
  nacl_arm_dec::YIELD_cccc0011001000001111000000000001_case_0_YIELD decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedYIELD_cccc0011001000001111000000000001_case_0_YIELD);
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

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_BLX_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_BLX_immediate()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 BLX_immediate")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_BLX_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_BLX_immediate);
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

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP2
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP2()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 CDP2")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CDP2);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CLREX
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CLREX()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 CLREX")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CLREX decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CLREX);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CPS
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CPS()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 CPS")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CPS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_CPS);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_immediate()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 LDC2_immediate")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_immediate);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_literal
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_literal()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 LDC2_literal")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_LDC2_literal);
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

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR2
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR2()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MCR2")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCR2);
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

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR2
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR2()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MCRR2")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MCRR2);
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

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC2
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC2()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MRC2")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRC2);
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

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC2
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC2()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 MRRC2")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_MRRC2);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_RFE
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_RFE()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 RFE")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_RFE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_RFE);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SETEND
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SETEND()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 SETEND")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SETEND decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SETEND);
};

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SRS
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SRS()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 SRS")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SRS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_SRS);
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

class NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC2
    : public NamedClassDecoder {
 public:
  NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC2()
    : NamedClassDecoder(decoder_, "Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1 STC2")
  {}

 private:
  nacl_arm_dec::Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC2 decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1_STC2);
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

class NamedActual_DMB_1111010101111111111100000101xxxx_case_1_DMB
    : public NamedClassDecoder {
 public:
  NamedActual_DMB_1111010101111111111100000101xxxx_case_1_DMB()
    : NamedClassDecoder(decoder_, "Actual_DMB_1111010101111111111100000101xxxx_case_1 DMB")
  {}

 private:
  nacl_arm_dec::Actual_DMB_1111010101111111111100000101xxxx_case_1_DMB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_DMB_1111010101111111111100000101xxxx_case_1_DMB);
};

class NamedActual_DMB_1111010101111111111100000101xxxx_case_1_DSB
    : public NamedClassDecoder {
 public:
  NamedActual_DMB_1111010101111111111100000101xxxx_case_1_DSB()
    : NamedClassDecoder(decoder_, "Actual_DMB_1111010101111111111100000101xxxx_case_1 DSB")
  {}

 private:
  nacl_arm_dec::Actual_DMB_1111010101111111111100000101xxxx_case_1_DSB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_DMB_1111010101111111111100000101xxxx_case_1_DSB);
};

class NamedActual_ISB_1111010101111111111100000110xxxx_case_1_ISB
    : public NamedClassDecoder {
 public:
  NamedActual_ISB_1111010101111111111100000110xxxx_case_1_ISB()
    : NamedClassDecoder(decoder_, "Actual_ISB_1111010101111111111100000110xxxx_case_1 ISB")
  {}

 private:
  nacl_arm_dec::Actual_ISB_1111010101111111111100000110xxxx_case_1_ISB decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_ISB_1111010101111111111100000110xxxx_case_1_ISB);
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

class NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSH_literal
    : public NamedClassDecoder {
 public:
  NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSH_literal()
    : NamedClassDecoder(decoder_, "Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1 LDRSH_literal")
  {}

 private:
  nacl_arm_dec::Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSH_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1_LDRSH_literal);
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

class NamedActual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1_MOVE_scalar_to_ARM_core_register
    : public NamedClassDecoder {
 public:
  NamedActual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1_MOVE_scalar_to_ARM_core_register()
    : NamedClassDecoder(decoder_, "Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1 MOVE_scalar_to_ARM_core_register")
  {}

 private:
  nacl_arm_dec::Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1_MOVE_scalar_to_ARM_core_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1_MOVE_scalar_to_ARM_core_register);
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

class NamedActual_PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1_PLD_PLDW_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1_PLD_PLDW_immediate()
    : NamedClassDecoder(decoder_, "Actual_PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1 PLD_PLDW_immediate")
  {}

 private:
  nacl_arm_dec::Actual_PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1_PLD_PLDW_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1_PLD_PLDW_immediate);
};

class NamedActual_PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_1_PLD_PLDW_register
    : public NamedClassDecoder {
 public:
  NamedActual_PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_1_PLD_PLDW_register()
    : NamedClassDecoder(decoder_, "Actual_PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_1 PLD_PLDW_register")
  {}

 private:
  nacl_arm_dec::Actual_PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_1_PLD_PLDW_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_1_PLD_PLDW_register);
};

class NamedActual_PLD_literal_11110101u10111111111iiiiiiiiiiii_case_1_PLD_literal
    : public NamedClassDecoder {
 public:
  NamedActual_PLD_literal_11110101u10111111111iiiiiiiiiiii_case_1_PLD_literal()
    : NamedClassDecoder(decoder_, "Actual_PLD_literal_11110101u10111111111iiiiiiiiiiii_case_1 PLD_literal")
  {}

 private:
  nacl_arm_dec::Actual_PLD_literal_11110101u10111111111iiiiiiiiiiii_case_1_PLD_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PLD_literal_11110101u10111111111iiiiiiiiiiii_case_1_PLD_literal);
};

class NamedActual_PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_1_PLI_immediate_literal
    : public NamedClassDecoder {
 public:
  NamedActual_PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_1_PLI_immediate_literal()
    : NamedClassDecoder(decoder_, "Actual_PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_1 PLI_immediate_literal")
  {}

 private:
  nacl_arm_dec::Actual_PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_1_PLI_immediate_literal decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_1_PLI_immediate_literal);
};

class NamedActual_PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_1_PLI_register
    : public NamedClassDecoder {
 public:
  NamedActual_PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_1_PLI_register()
    : NamedClassDecoder(decoder_, "Actual_PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_1 PLI_register")
  {}

 private:
  nacl_arm_dec::Actual_PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_1_PLI_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_1_PLI_register);
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

class NamedActual_Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_1_None
    : public NamedClassDecoder {
 public:
  NamedActual_Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_1_None()
    : NamedClassDecoder(decoder_, "Actual_Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_1 None")
  {}

 private:
  nacl_arm_dec::Actual_Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_1_None decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_1_None);
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

class NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VABS
    : public NamedClassDecoder {
 public:
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VABS()
    : NamedClassDecoder(decoder_, "Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 VABS")
  {}

 private:
  nacl_arm_dec::Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VABS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VABS);
};

class NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCMP_VCMPE
    : public NamedClassDecoder {
 public:
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCMP_VCMPE()
    : NamedClassDecoder(decoder_, "Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 VCMP_VCMPE")
  {}

 private:
  nacl_arm_dec::Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCMP_VCMPE decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCMP_VCMPE);
};

class NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVTB_VCVTT
    : public NamedClassDecoder {
 public:
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVTB_VCVTT()
    : NamedClassDecoder(decoder_, "Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 VCVTB_VCVTT")
  {}

 private:
  nacl_arm_dec::Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVTB_VCVTT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVTB_VCVTT);
};

class NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVT_between_double_precision_and_single_precision
    : public NamedClassDecoder {
 public:
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVT_between_double_precision_and_single_precision()
    : NamedClassDecoder(decoder_, "Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 VCVT_between_double_precision_and_single_precision")
  {}

 private:
  nacl_arm_dec::Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVT_between_double_precision_and_single_precision decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVT_between_double_precision_and_single_precision);
};

class NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_immediate
    : public NamedClassDecoder {
 public:
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_immediate()
    : NamedClassDecoder(decoder_, "Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 VMOV_immediate")
  {}

 private:
  nacl_arm_dec::Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_immediate decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_immediate);
};

class NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_register
    : public NamedClassDecoder {
 public:
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_register()
    : NamedClassDecoder(decoder_, "Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 VMOV_register")
  {}

 private:
  nacl_arm_dec::Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_register);
};

class NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VNEG
    : public NamedClassDecoder {
 public:
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VNEG()
    : NamedClassDecoder(decoder_, "Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 VNEG")
  {}

 private:
  nacl_arm_dec::Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VNEG decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VNEG);
};

class NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VSQRT
    : public NamedClassDecoder {
 public:
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VSQRT()
    : NamedClassDecoder(decoder_, "Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1 VSQRT")
  {}

 private:
  nacl_arm_dec::Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VSQRT decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VSQRT);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VADD_floating_point
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VADD_floating_point()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VADD_floating_point")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VADD_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VADD_floating_point);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VDIV
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VDIV()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VDIV")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VDIV decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VDIV);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFMA_VFMS
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFMA_VFMS()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VFMA_VFMS")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFMA_VFMS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFMA_VFMS);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFNMA_VFNMS
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFNMA_VFNMS()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VFNMA_VFNMS")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFNMA_VFNMS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VFNMA_VFNMS);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMLA_VMLS_floating_point
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMLA_VMLS_floating_point()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VMLA_VMLS_floating_point")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMLA_VMLS_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMLA_VMLS_floating_point);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMUL_floating_point
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMUL_floating_point()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VMUL_floating_point")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMUL_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VMUL_floating_point);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMLA_VNMLS
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMLA_VNMLS()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VNMLA_VNMLS")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMLA_VNMLS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMLA_VNMLS);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMUL
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMUL()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VNMUL")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMUL decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VNMUL);
};

class NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VSUB_floating_point
    : public NamedClassDecoder {
 public:
  NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VSUB_floating_point()
    : NamedClassDecoder(decoder_, "Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1 VSUB_floating_point")
  {}

 private:
  nacl_arm_dec::Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VSUB_floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1_VSUB_floating_point);
};

class NamedActual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1_VCVT_VCVTR_between_floating_point_and_integer_Floating_point
    : public NamedClassDecoder {
 public:
  NamedActual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1_VCVT_VCVTR_between_floating_point_and_integer_Floating_point()
    : NamedClassDecoder(decoder_, "Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1 VCVT_VCVTR_between_floating_point_and_integer_Floating_point")
  {}

 private:
  nacl_arm_dec::Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1_VCVT_VCVTR_between_floating_point_and_integer_Floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1_VCVT_VCVTR_between_floating_point_and_integer_Floating_point);
};

class NamedActual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1_VCVT_between_floating_point_and_fixed_point_Floating_point
    : public NamedClassDecoder {
 public:
  NamedActual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1_VCVT_between_floating_point_and_fixed_point_Floating_point()
    : NamedClassDecoder(decoder_, "Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1 VCVT_between_floating_point_and_fixed_point_Floating_point")
  {}

 private:
  nacl_arm_dec::Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1_VCVT_between_floating_point_and_fixed_point_Floating_point decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1_VCVT_between_floating_point_and_fixed_point_Floating_point);
};

class NamedActual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1_VDUP_ARM_core_register
    : public NamedClassDecoder {
 public:
  NamedActual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1_VDUP_ARM_core_register()
    : NamedClassDecoder(decoder_, "Actual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1 VDUP_ARM_core_register")
  {}

 private:
  nacl_arm_dec::Actual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1_VDUP_ARM_core_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1_VDUP_ARM_core_register);
};

class NamedActual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1_VLDM
    : public NamedClassDecoder {
 public:
  NamedActual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1_VLDM()
    : NamedClassDecoder(decoder_, "Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1 VLDM")
  {}

 private:
  nacl_arm_dec::Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1_VLDM decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1_VLDM);
};

class NamedActual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1_VLDM
    : public NamedClassDecoder {
 public:
  NamedActual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1_VLDM()
    : NamedClassDecoder(decoder_, "Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1 VLDM")
  {}

 private:
  nacl_arm_dec::Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1_VLDM decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1_VLDM);
};

class NamedActual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1_VLDR
    : public NamedClassDecoder {
 public:
  NamedActual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1_VLDR()
    : NamedClassDecoder(decoder_, "Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1 VLDR")
  {}

 private:
  nacl_arm_dec::Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1_VLDR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1_VLDR);
};

class NamedActual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1_VMOV_ARM_core_register_to_scalar
    : public NamedClassDecoder {
 public:
  NamedActual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1_VMOV_ARM_core_register_to_scalar()
    : NamedClassDecoder(decoder_, "Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1 VMOV_ARM_core_register_to_scalar")
  {}

 private:
  nacl_arm_dec::Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1_VMOV_ARM_core_register_to_scalar decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1_VMOV_ARM_core_register_to_scalar);
};

class NamedActual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1_VMOV_between_ARM_core_register_and_single_precision_register
    : public NamedClassDecoder {
 public:
  NamedActual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1_VMOV_between_ARM_core_register_and_single_precision_register()
    : NamedClassDecoder(decoder_, "Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1 VMOV_between_ARM_core_register_and_single_precision_register")
  {}

 private:
  nacl_arm_dec::Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1_VMOV_between_ARM_core_register_and_single_precision_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1_VMOV_between_ARM_core_register_and_single_precision_register);
};

class NamedActual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register
    : public NamedClassDecoder {
 public:
  NamedActual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register()
    : NamedClassDecoder(decoder_, "Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1 VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register")
  {}

 private:
  nacl_arm_dec::Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register);
};

class NamedActual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers
    : public NamedClassDecoder {
 public:
  NamedActual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers()
    : NamedClassDecoder(decoder_, "Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1 VMOV_between_two_ARM_core_registers_and_two_single_precision_registers")
  {}

 private:
  nacl_arm_dec::Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers);
};

class NamedActual_VMRS_cccc111011110001tttt101000010000_case_1_VMRS
    : public NamedClassDecoder {
 public:
  NamedActual_VMRS_cccc111011110001tttt101000010000_case_1_VMRS()
    : NamedClassDecoder(decoder_, "Actual_VMRS_cccc111011110001tttt101000010000_case_1 VMRS")
  {}

 private:
  nacl_arm_dec::Actual_VMRS_cccc111011110001tttt101000010000_case_1_VMRS decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VMRS_cccc111011110001tttt101000010000_case_1_VMRS);
};

class NamedActual_VMSR_cccc111011100001tttt101000010000_case_1_VMSR
    : public NamedClassDecoder {
 public:
  NamedActual_VMSR_cccc111011100001tttt101000010000_case_1_VMSR()
    : NamedClassDecoder(decoder_, "Actual_VMSR_cccc111011100001tttt101000010000_case_1 VMSR")
  {}

 private:
  nacl_arm_dec::Actual_VMSR_cccc111011100001tttt101000010000_case_1_VMSR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VMSR_cccc111011100001tttt101000010000_case_1_VMSR);
};

class NamedActual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPOP
    : public NamedClassDecoder {
 public:
  NamedActual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPOP()
    : NamedClassDecoder(decoder_, "Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1 VPOP")
  {}

 private:
  nacl_arm_dec::Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPOP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPOP);
};

class NamedActual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPUSH
    : public NamedClassDecoder {
 public:
  NamedActual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPUSH()
    : NamedClassDecoder(decoder_, "Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1 VPUSH")
  {}

 private:
  nacl_arm_dec::Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPUSH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1_VPUSH);
};

class NamedActual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPOP
    : public NamedClassDecoder {
 public:
  NamedActual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPOP()
    : NamedClassDecoder(decoder_, "Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1 VPOP")
  {}

 private:
  nacl_arm_dec::Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPOP decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPOP);
};

class NamedActual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPUSH
    : public NamedClassDecoder {
 public:
  NamedActual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPUSH()
    : NamedClassDecoder(decoder_, "Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1 VPUSH")
  {}

 private:
  nacl_arm_dec::Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPUSH decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1_VPUSH);
};

class NamedActual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1_VSTM
    : public NamedClassDecoder {
 public:
  NamedActual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1_VSTM()
    : NamedClassDecoder(decoder_, "Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1 VSTM")
  {}

 private:
  nacl_arm_dec::Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1_VSTM decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1_VSTM);
};

class NamedActual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1_VSTM
    : public NamedClassDecoder {
 public:
  NamedActual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1_VSTM()
    : NamedClassDecoder(decoder_, "Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1 VSTM")
  {}

 private:
  nacl_arm_dec::Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1_VSTM decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1_VSTM);
};

class NamedActual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1_VSTR
    : public NamedClassDecoder {
 public:
  NamedActual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1_VSTR()
    : NamedClassDecoder(decoder_, "Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1 VSTR")
  {}

 private:
  nacl_arm_dec::Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1_VSTR decoder_;
  NACL_DISALLOW_COPY_AND_ASSIGN(NamedActual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1_VSTR);
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
