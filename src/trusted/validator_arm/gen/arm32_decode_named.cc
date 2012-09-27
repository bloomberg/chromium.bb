/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_decoder.h"

using nacl_arm_dec::ClassDecoder;
using nacl_arm_dec::Instruction;

namespace nacl_arm_test {

NamedArm32DecoderState::NamedArm32DecoderState()
{}

/*
 * Implementation of table ARMv7.
 * Specified by: ('See Section A5.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_ARMv7(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x04000000 /* op1(27:25)=010 */)
    return decode_load_store_word_and_unsigned_byte(inst);

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25)=011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */)
    return decode_load_store_word_and_unsigned_byte(inst);

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25)=011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */)
    return decode_media_instructions(inst);

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x00000000 /* op1(27:25)=00x */)
    return decode_data_processing_and_miscellaneous_instructions(inst);

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x08000000 /* op1(27:25)=10x */)
    return decode_branch_branch_with_link_and_block_data_transfer(inst);

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x0C000000 /* op1(27:25)=11x */)
    return decode_coprocessor_instructions_and_supervisor_call(inst);

  if ((inst.Bits() & 0xF0000000) == 0xF0000000 /* cond(31:28)=1111 */)
    return decode_unconditional_instructions(inst);

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table branch_branch_with_link_and_block_data_transfer.
 * Specified by: ('See Section A5.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_branch_branch_with_link_and_block_data_transfer(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x03F00000) == 0x00900000 /* op(25:20)=001001 */)
    return LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_;

  if ((inst.Bits() & 0x03F00000) == 0x00B00000 /* op(25:20)=001011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */)
    return LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_;

  if ((inst.Bits() & 0x03F00000) == 0x00B00000 /* op(25:20)=001011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */)
    return LoadRegisterList_Pop_Rule_A1_instance_;

  if ((inst.Bits() & 0x03F00000) == 0x01000000 /* op(25:20)=010000 */)
    return StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_;

  if ((inst.Bits() & 0x03F00000) == 0x01200000 /* op(25:20)=010010 */ &&
      (inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */)
    return StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_;

  if ((inst.Bits() & 0x03F00000) == 0x01200000 /* op(25:20)=010010 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */)
    return StoreRegisterList_Push_Rule_A1_instance_;

  if ((inst.Bits() & 0x03D00000) == 0x00000000 /* op(25:20)=0000x0 */)
    return StoreRegisterList_Stmda_Stmed_Rule_190_A1_P376_instance_;

  if ((inst.Bits() & 0x03D00000) == 0x00100000 /* op(25:20)=0000x1 */)
    return LoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112_instance_;

  if ((inst.Bits() & 0x03D00000) == 0x00800000 /* op(25:20)=0010x0 */)
    return StoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374_instance_;

  if ((inst.Bits() & 0x03D00000) == 0x01100000 /* op(25:20)=0100x1 */)
    return LoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114_instance_;

  if ((inst.Bits() & 0x03D00000) == 0x01800000 /* op(25:20)=0110x0 */)
    return StoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380_instance_;

  if ((inst.Bits() & 0x03D00000) == 0x01900000 /* op(25:20)=0110x1 */)
    return LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116_instance_;

  if ((inst.Bits() & 0x02500000) == 0x00400000 /* op(25:20)=0xx1x0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */)
    return ForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22_instance_;

  if ((inst.Bits() & 0x02500000) == 0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000) == 0x00000000 /* R(15)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */)
    return ForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7_instance_;

  if ((inst.Bits() & 0x02500000) == 0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000) == 0x00008000 /* R(15)=1 */)
    return ForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5_instance_;

  if ((inst.Bits() & 0x03000000) == 0x02000000 /* op(25:20)=10xxxx */)
    return BranchImmediate24_B_Rule_16_A1_P44_instance_;

  if ((inst.Bits() & 0x03000000) == 0x03000000 /* op(25:20)=11xxxx */)
    return BranchImmediate24_Bl_Blx_Rule_23_A1_P58_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table coprocessor_instructions_and_supervisor_call.
 * Specified by: ('See Section A5.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_coprocessor_instructions_and_supervisor_call(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x03E00000) == 0x00000000 /* op1(25:20)=00000x */)
    return UndefinedCondDecoder_Undefined_A5_6_instance_;

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03F00000) == 0x00400000 /* op1(25:20)=000100 */)
    return ForbiddenCondDecoder_Mcrr_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03F00000) == 0x00500000 /* op1(25:20)=000101 */)
    return ForbiddenCondDecoder_Mrrc_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03100000) == 0x02000000 /* op1(25:20)=10xxx0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */)
    return ForbiddenCondDecoder_Mcr_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03100000) == 0x02100000 /* op1(25:20)=10xxx1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */)
    return ForbiddenCondDecoder_Mrc_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000) == 0x00000000 /* op1(25:20)=0xxxx0 */ &&
      (inst.Bits() & 0x03B00000) != 0x00000000 /* op1_repeated(25:20)=~000x00 */)
    return ForbiddenCondDecoder_Stc_Rule_A2_instance_;

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000) == 0x00100000 /* op1(25:20)=0xxxx1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x03B00000) != 0x00100000 /* op1_repeated(25:20)=~000x01 */)
    return ForbiddenCondDecoder_Ldc_immediate_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000) == 0x00100000 /* op1(25:20)=0xxxx1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x03B00000) != 0x00100000 /* op1_repeated(25:20)=~000x01 */)
    return ForbiddenCondDecoder_Ldc_literal_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */)
    return ForbiddenCondDecoder_Cdp_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20)=00010x */)
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */)
    return decode_floating_point_data_processing_instructions(inst);

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */)
    return decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(inst);

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x02000000) == 0x00000000 /* op1(25:20)=0xxxxx */ &&
      (inst.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20)=~000x0x */)
    return decode_extension_register_load_store_instructions(inst);

  if ((inst.Bits() & 0x03000000) == 0x03000000 /* op1(25:20)=11xxxx */)
    return ForbiddenCondDecoder_Svc_Rule_A1_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table data_processing_and_miscellaneous_instructions.
 * Specified by: ('See Section A5.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_data_processing_and_miscellaneous_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */ &&
      (inst.Bits() & 0x00000090) == 0x00000010 /* op2(7:4)=0xx1 */)
    return decode_data_processing_register_shifted_register(inst);

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */)
    return decode_data_processing_register(inst);

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) == 0x01000000 /* op1(24:20)=10xx0 */ &&
      (inst.Bits() & 0x00000090) == 0x00000080 /* op2(7:4)=1xx0 */)
    return decode_halfword_multiply_and_multiply_accumulate(inst);

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) == 0x01000000 /* op1(24:20)=10xx0 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:4)=0xxx */)
    return decode_miscellaneous_instructions(inst);

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20)=~0xx1x */ &&
      (inst.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4)=1011 */)
    return decode_extra_load_store_instructions(inst);

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20)=~0xx1x */ &&
      (inst.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4)=11x1 */)
    return decode_extra_load_store_instructions(inst);

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) == 0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4)=1011 */)
    return ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) == 0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4)=11x1 */)
    return ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* op1(24:20)=0xxxx */ &&
      (inst.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4)=1001 */)
    return decode_multiply_and_multiply_accumulate(inst);

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* op1(24:20)=1xxxx */ &&
      (inst.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4)=1001 */)
    return decode_synchronization_primitives(inst);

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01F00000) == 0x01000000 /* op1(24:20)=10000 */)
    return Unary1RegisterImmediateOpDynCodeReplace_Movw_Rule_96_A2_P194_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01F00000) == 0x01400000 /* op1(24:20)=10100 */)
    return Unary1RegisterImmediateOpDynCodeReplace_Movt_Rule_99_A1_P200_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01B00000) == 0x01200000 /* op1(24:20)=10x10 */)
    return decode_msr_immediate_and_hints(inst);

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */)
    return decode_data_processing_immediate(inst);

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table data_processing_immediate.
 * Specified by: ('See Section A5.2.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_data_processing_immediate(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01F00000) == 0x01100000 /* op(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return MaskedBinaryRegisterImmediateTest_TST_immediate_A1_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01300000 /* op(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return BinaryRegisterImmediateTest_TEQ_immediate_A1_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01500000 /* op(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return BinaryRegisterImmediateTest_CMP_immediate_A1_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01700000 /* op(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return BinaryRegisterImmediateTest_CMN_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00000000 /* op(24:20)=0000x */)
    return Binary2RegisterImmediateOp_AND_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00200000 /* op(24:20)=0001x */)
    return Binary2RegisterImmediateOp_EOR_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* op(24:20)=0010x */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */)
    return Binary2RegisterImmediateOp_SUB_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* op(24:20)=0010x */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */)
    return Unary1RegisterImmediateOp_ADR_A2_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00600000 /* op(24:20)=0011x */)
    return Binary2RegisterImmediateOp_RSB_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00800000 /* op(24:20)=0100x */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */)
    return Binary2RegisterImmediateOp_ADD_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00800000 /* op(24:20)=0100x */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */)
    return Unary1RegisterImmediateOp_ADR_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00A00000 /* op(24:20)=0101x */)
    return Binary2RegisterImmediateOp_ADC_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00C00000 /* op(24:20)=0110x */)
    return Binary2RegisterImmediateOp_SBC_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00E00000 /* op(24:20)=0111x */)
    return Binary2RegisterImmediateOp_RSC_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01800000 /* op(24:20)=1100x */)
    return Binary2RegisterImmediateOpDynCodeReplace_ORR_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op(24:20)=1101x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary1RegisterImmediateOpDynCodeReplace_MOV_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op(24:20)=1110x */)
    return MaskedBinary2RegisterImmediateOp_BIC_immediate_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01E00000 /* op(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary1RegisterImmediateOpDynCodeReplace_MVN_immediate_A1_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table data_processing_register.
 * Specified by: ('See Section A5.2.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_data_processing_register(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20)=0000x */)
    return Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20)=0001x */)
    return Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20)=0010x */)
    return Binary3RegisterImmedShiftedOp_Sub_Rule_213_A1_P422_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20)=0011x */)
    return Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20)=0100x */)
    return Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20)=0101x */)
    return Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20)=0110x */)
    return Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20)=0111x */)
    return Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20)=1100x */)
    return Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op3(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */ &&
      (inst.Bits() & 0x00000060) == 0x00000060 /* op3(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op3(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */ &&
      (inst.Bits() & 0x00000060) == 0x00000060 /* op3(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000020 /* op3(6:5)=01 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op3(6:5)=10 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */)
    return Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table data_processing_register_shifted_register.
 * Specified by: ('See Section A5.2.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_data_processing_register_shifted_register(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary3RegisterShiftedTest_TST_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20)=10011 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary3RegisterShiftedTest_TEQ_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary3RegisterShiftedTest_CMP_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20)=10111 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary3RegisterShiftedTest_CMN_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20)=0000x */)
    return Binary4RegisterShiftedOp_AND_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20)=0001x */)
    return Binary4RegisterShiftedOp_EOR_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20)=0010x */)
    return Binary4RegisterShiftedOp_SUB_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20)=0011x */)
    return Binary4RegisterShiftedOp_RSB_register_shfited_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20)=0100x */)
    return Binary4RegisterShiftedOp_ADD_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20)=0101x */)
    return Binary4RegisterShiftedOp_ADC_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20)=0110x */)
    return Binary4RegisterShiftedOp_SBC_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20)=0111x */)
    return Binary4RegisterShiftedOp_RSC_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20)=1100x */)
    return Binary4RegisterShiftedOp_ORR_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op2(6:5)=00 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Binary3RegisterOp_LSL_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Binary3RegisterOp_LSR_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Binary3RegisterOp_ASR_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Binary3RegisterOp_ROR_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */)
    return Binary4RegisterShiftedOp_BIC_register_shifted_register_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */)
    return Unary3RegisterShiftedOp_MVN_register_shifted_register_A1_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table extension_register_load_store_instructions.
 * Specified by: ('A7.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_extension_register_load_store_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x01B00000) == 0x00900000 /* opcode(24:20)=01x01 */)
    return LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_;

  if ((inst.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20)=01x11 */ &&
      (inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */)
    return LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_;

  if ((inst.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20)=01x11 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */)
    return LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694_instance_;

  if ((inst.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20)=10x10 */ &&
      (inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */)
    return StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_;

  if ((inst.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20)=10x10 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */)
    return StoreVectorRegisterList_Vpush_355_A1_A2_P696_instance_;

  if ((inst.Bits() & 0x01B00000) == 0x01300000 /* opcode(24:20)=10x11 */)
    return LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* opcode(24:20)=0010x */)
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);

  if ((inst.Bits() & 0x01300000) == 0x01000000 /* opcode(24:20)=1xx00 */)
    return StoreVectorRegister_Vstr_Rule_400_A1_A2_P786_instance_;

  if ((inst.Bits() & 0x01300000) == 0x01100000 /* opcode(24:20)=1xx01 */)
    return LoadVectorRegister_Vldr_Rule_320_A1_A2_P628_instance_;

  if ((inst.Bits() & 0x01900000) == 0x00800000 /* opcode(24:20)=01xx0 */)
    return StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table extra_load_store_instructions.
 * Specified by: ('See Section A5.2.8',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_extra_load_store_instructions(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Store3RegisterOp_Strh_Rule_208_A1_P412_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Load3RegisterOp_Ldrh_Rule_76_A1_P156_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */)
    return Store2RegisterImm8Op_Strh_Rule_207_A1_P410_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */)
    return Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Load3RegisterOp_Ldrsb_Rule_80_A1_P164_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */)
    return Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */)
    return Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm8Op_ldrsb_Rule_79_A1_162_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Store3RegisterDoubleOp_Strd_Rule_201_A1_P398_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Load3RegisterOp_Ldrsh_Rule_84_A1_P172_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */)
    return Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */)
    return Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168_instance_;

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table floating_point_data_processing_instructions.
 * Specified by: ('A7.5 Table A7 - 16',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_floating_point_data_processing_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x00B00000) == 0x00000000 /* opc1(23:20)=0x00 */)
    return CondVfpOp_Vmla_vmls_Rule_423_A2_P636_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00100000 /* opc1(23:20)=0x01 */)
    return CondVfpOp_Vnmla_vnmls_Rule_343_A1_P674_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00200000 /* opc1(23:20)=0x10 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */)
    return CondVfpOp_Vmul_Rule_338_A2_P664_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00200000 /* opc1(23:20)=0x10 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */)
    return CondVfpOp_Vnmul_Rule_343_A2_P674_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20)=0x11 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */)
    return CondVfpOp_Vadd_Rule_271_A2_P536_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20)=0x11 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */)
    return CondVfpOp_Vsub_Rule_402_A2_P790_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00800000 /* opc1(23:20)=1x00 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */)
    return CondVfpOp_Vdiv_Rule_301_A1_P590_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00900000 /* opc1(23:20)=1x01 */)
    return CondVfpOp_Vfnma_vfnms_Rule_A1_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00A00000 /* opc1(23:20)=1x10 */)
    return CondVfpOp_Vfma_vfms_Rule_A1_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00B00000 /* opc1(23:20)=1x11 */)
    return decode_other_floating_point_data_processing_instructions(inst);

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table halfword_multiply_and_multiply_accumulate.
 * Specified by: ('See Section A5.2.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_halfword_multiply_and_multiply_accumulate(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00600000) == 0x00000000 /* op1(22:21)=00 */)
    return Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op(5)=0 */)
    return Binary4RegisterDualOp_Smlawx_Rule_171_A1_340_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00400000 /* op1(22:21)=10 */)
    return Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00600000 /* op1(22:21)=11 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table load_store_word_and_unsigned_byte.
 * Specified by: ('See Section A5.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_load_store_word_and_unsigned_byte(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000) == 0x00200000 /* op1(24:20)=0x010 */)
    return ForbiddenCondDecoder_Strt_Rule_A1_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000) == 0x00300000 /* op1(24:20)=0x011 */)
    return ForbiddenCondDecoder_Ldrt_Rule_A1_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000) == 0x00600000 /* op1(24:20)=0x110 */)
    return ForbiddenCondDecoder_Strtb_Rule_A1_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01700000) == 0x00700000 /* op1(24:20)=0x111 */)
    return ForbiddenCondDecoder_Ldrtb_Rule_A1_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x01700000) != 0x00200000 /* op1_repeated(24:20)=~0x010 */)
    return decode_load_store_word_and_unsigned_byte_str_or_push(inst);

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */)
    return Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x01700000) != 0x00600000 /* op1_repeated(24:20)=~0x110 */)
    return Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */)
    return Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_;

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */)
    return Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000) == 0x00200000 /* op1(24:20)=0x010 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */)
    return ForbiddenCondDecoder_Strt_Rule_A2_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000) == 0x00300000 /* op1(24:20)=0x011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */)
    return ForbiddenCondDecoder_Ldrt_Rule_A2_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000) == 0x00600000 /* op1(24:20)=0x110 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */)
    return ForbiddenCondDecoder_Strtb_Rule_A2_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01700000) == 0x00700000 /* op1(24:20)=0x111 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */)
    return ForbiddenCondDecoder_Ldrtb_Rule_A2_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00200000 /* op1_repeated(24:20)=~0x010 */)
    return Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */)
    return Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00600000 /* op1_repeated(24:20)=~0x110 */)
    return Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_;

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */)
    return Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table load_store_word_and_unsigned_byte_str_or_push.
 * Specified by: ('See Section A5.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_load_store_word_and_unsigned_byte_str_or_push(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01E00000) == 0x01200000 /* Flags(24:21)=1001 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */ &&
      (inst.Bits() & 0x00000FFF) == 0x00000004 /* Imm12(11:0)=000000000100 */)
    return Store2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table media_instructions.
 * Specified by: ('See Section A5.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_media_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=~1111 */)
    return Binary4RegisterDualOp_Usada8_Rule_254_A1_P502_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12)=1111 */)
    return Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500_instance_;

  if ((inst.Bits() & 0x01F00000) == 0x01F00000 /* op1(24:20)=11111 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */)
    return Roadblock_Udf_Rule_A1_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20)=1101x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op2(7:5)=x10 */)
    return Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0)=~1111 */)
    return Binary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0)=1111 */)
    return Unary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46_instance_;

  if ((inst.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20)=1111x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op2(7:5)=x10 */)
    return Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466_instance_;

  if ((inst.Bits() & 0x01C00000) == 0x00000000 /* op1(24:20)=000xx */)
    return decode_parallel_addition_and_subtraction_signed(inst);

  if ((inst.Bits() & 0x01C00000) == 0x00400000 /* op1(24:20)=001xx */)
    return decode_parallel_addition_and_subtraction_unsigned(inst);

  if ((inst.Bits() & 0x01800000) == 0x00800000 /* op1(24:20)=01xxx */)
    return decode_packing_unpacking_saturation_and_reversal(inst);

  if ((inst.Bits() & 0x01800000) == 0x01000000 /* op1(24:20)=10xxx */)
    return decode_signed_multiply_signed_and_unsigned_divide(inst);

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table memory_hints_advanced_simd_instructions_and_miscellaneous_instructions.
 * Specified by: ('See Section A5.7.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000000 /* op2(7:4)=0000 */ &&
      (inst.Bits() & 0x00010000) == 0x00010000 /* Rn(19:16)=xxx1 */ &&
      (inst.Bits() & 0x000EFD0F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000 */)
    return ForbiddenUncondDecoder_Setend_Rule_157_P314_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:4)=xx0x */ &&
      (inst.Bits() & 0x00010000) == 0x00000000 /* Rn(19:16)=xxx0 */ &&
      (inst.Bits() & 0x0000FE00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx */)
    return ForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05300000 /* op1(26:20)=1010011 */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000000 /* op2(7:4)=0000 */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000010 /* op2(7:4)=0001 */ &&
      (inst.Bits() & 0x000FFF0F) == 0x000FF00F /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111 */)
    return ForbiddenUncondDecoder_Clrex_Rule_30_A1_P70_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000040 /* op2(7:4)=0100 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */)
    return DataBarrier_Dsb_Rule_42_A1_P92_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000050 /* op2(7:4)=0101 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */)
    return DataBarrier_Dmb_Rule_41_A1_P90_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000060 /* op2(7:4)=0110 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */)
    return InstructionBarrier_Isb_Rule_49_A1_P102_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000070 /* op2(7:4)=0111 */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:4)=001x */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x00000080) == 0x00000080 /* op2(7:4)=1xxx */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x07700000) == 0x04100000 /* op1(26:20)=100x001 */)
    return ForbiddenUncondDecoder_Unallocated_hints_instance_;

  if ((inst.Bits() & 0x07700000) == 0x04500000 /* op1(26:20)=100x101 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return PreloadRegisterImm12Op_Pli_Rule_120_A1_P242_instance_;

  if ((inst.Bits() & 0x07700000) == 0x05100000 /* op1(26:20)=101x001 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return PreloadRegisterImm12Op_Pldw_Rule_117_A1_P236_instance_;

  if ((inst.Bits() & 0x07700000) == 0x05100000 /* op1(26:20)=101x001 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x07700000) == 0x05500000 /* op1(26:20)=101x101 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return PreloadRegisterImm12Op_Pld_Rule_117_A1_P236_instance_;

  if ((inst.Bits() & 0x07700000) == 0x05500000 /* op1(26:20)=101x101 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return PreloadRegisterImm12Op_Pld_Rule_118_A1_P238_instance_;

  if ((inst.Bits() & 0x07700000) == 0x06100000 /* op1(26:20)=110x001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */)
    return ForbiddenUncondDecoder_Unallocated_hints_instance_;

  if ((inst.Bits() & 0x07700000) == 0x06500000 /* op1(26:20)=110x101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return PreloadRegisterPairOp_Pli_Rule_121_A1_P244_instance_;

  if ((inst.Bits() & 0x07700000) == 0x07100000 /* op1(26:20)=111x001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return PreloadRegisterPairOpWAndRnNotPc_Pldw_Rule_119_A1_P240_instance_;

  if ((inst.Bits() & 0x07700000) == 0x07500000 /* op1(26:20)=111x101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return PreloadRegisterPairOpWAndRnNotPc_Pld_Rule_119_A1_P240_instance_;

  if ((inst.Bits() & 0x07B00000) == 0x05B00000 /* op1(26:20)=1011x11 */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x07300000) == 0x04300000 /* op1(26:20)=100xx11 */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x06300000) == 0x06300000 /* op1(26:20)=11xxx11 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */)
    return UnpredictableUncondDecoder_Unpredictable_instance_;

  if ((inst.Bits() & 0x07100000) == 0x04000000 /* op1(26:20)=100xxx0 */)
    return NotImplemented_None_instance_;

  if ((inst.Bits() & 0x06000000) == 0x02000000 /* op1(26:20)=01xxxxx */)
    return NotImplemented_None_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table miscellaneous_instructions.
 * Specified by: ('See Section A5.2.12',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_miscellaneous_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00030000) == 0x00000000 /* op1(19:16)=xx00 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */)
    return Unary1RegisterUse_Msr_Rule_104_A1_P210_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00030000) == 0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */)
    return ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00020000) == 0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */)
    return ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */)
    return ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x000F0D0F) == 0x000F0000 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000 */)
    return Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x00000C0F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000 */)
    return ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* op(22:21)=x1 */ &&
      (inst.Bits() & 0x0000FC00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx */)
    return ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000010 /* op2(6:4)=001 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */)
    return BranchToRegister_Bx_Rule_25_A1_P62_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000010 /* op2(6:4)=001 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000020 /* op2(6:4)=010 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */)
    return ForbiddenCondDecoder_Bxj_Rule_26_A1_P64_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000030 /* op2(6:4)=011 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */)
    return BranchToRegister_Blx_Rule_24_A1_P60_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000050 /* op2(6:4)=101 */)
    return decode_saturating_addition_and_subtraction(inst);

  if ((inst.Bits() & 0x00000070) == 0x00000060 /* op2(6:4)=110 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF0F) == 0x0000000E /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110 */)
    return ForbiddenCondDecoder_Eret_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */)
    return BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00400000 /* op(22:21)=10 */)
    return ForbiddenCondDecoder_Hvc_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx */)
    return ForbiddenCondDecoder_Smc_Rule_B6_1_9_P18_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table msr_immediate_and_hints.
 * Specified by: ('See Section A5.2.11',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_msr_immediate_and_hints(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000000 /* op2(7:0)=00000000 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */)
    return CondDecoder_Nop_Rule_110_A1_P222_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000001 /* op2(7:0)=00000001 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */)
    return CondDecoder_Yield_Rule_413_A1_P812_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000002 /* op2(7:0)=00000010 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */)
    return ForbiddenCondDecoder_Wfe_Rule_411_A1_P808_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000003 /* op2(7:0)=00000011 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */)
    return ForbiddenCondDecoder_Wfi_Rule_412_A1_P810_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000004 /* op2(7:0)=00000100 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */)
    return ForbiddenCondDecoder_Sev_Rule_158_A1_P316_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000F0) == 0x000000F0 /* op2(7:0)=1111xxxx */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */)
    return ForbiddenCondDecoder_Dbg_Rule_40_A1_P88_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00040000 /* op1(19:16)=0100 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000B0000) == 0x00080000 /* op1(19:16)=1x00 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00030000) == 0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00020000) == 0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if ((inst.Bits() & 0x00400000) == 0x00400000 /* op(22)=1 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Forbidden_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table multiply_and_multiply_accumulate.
 * Specified by: ('See Section A5.2.5',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_multiply_and_multiply_accumulate(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00F00000) == 0x00400000 /* op(23:20)=0100 */)
    return Binary4RegisterDualResult_UMAAL_A1_instance_;

  if ((inst.Bits() & 0x00F00000) == 0x00600000 /* op(23:20)=0110 */)
    return Binary4RegisterDualOp_MLS_A1_instance_;

  if ((inst.Bits() & 0x00D00000) == 0x00500000 /* op(23:20)=01x1 */)
    return UndefinedCondDecoder_None_instance_;

  if ((inst.Bits() & 0x00E00000) == 0x00000000 /* op(23:20)=000x */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */)
    return Binary3RegisterOpAltA_MUL_A1_instance_;

  if ((inst.Bits() & 0x00E00000) == 0x00200000 /* op(23:20)=001x */)
    return Binary4RegisterDualOp_MLA_A1_instance_;

  if ((inst.Bits() & 0x00E00000) == 0x00800000 /* op(23:20)=100x */)
    return Binary4RegisterDualResult_UMULL_A1_instance_;

  if ((inst.Bits() & 0x00E00000) == 0x00A00000 /* op(23:20)=101x */)
    return Binary4RegisterDualResult_UMLAL_A1_instance_;

  if ((inst.Bits() & 0x00E00000) == 0x00C00000 /* op(23:20)=110x */)
    return Binary4RegisterDualResult_SMULL_A1_instance_;

  if ((inst.Bits() & 0x00E00000) == 0x00E00000 /* op(23:20)=111x */)
    return Binary4RegisterDualResult_SMLAL_A1_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table other_floating_point_data_processing_instructions.
 * Specified by: ('A7.5 Table A7 - 17',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_other_floating_point_data_processing_instructions(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x000F0000) == 0x00000000 /* opc2(19:16)=0000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* opc3(7:6)=01 */)
    return CondVfpOp_Vmov_Rule_327_A2_P642_instance_;

  if ((inst.Bits() & 0x000F0000) == 0x00000000 /* opc2(19:16)=0000 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6)=11 */)
    return CondVfpOp_Vabs_Rule_269_A2_P532_instance_;

  if ((inst.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16)=0001 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* opc3(7:6)=01 */)
    return CondVfpOp_Vneg_Rule_342_A2_P672_instance_;

  if ((inst.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16)=0001 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6)=11 */)
    return CondVfpOp_Vsqrt_Rule_388_A1_P762_instance_;

  if ((inst.Bits() & 0x000F0000) == 0x00040000 /* opc2(19:16)=0100 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */)
    return CondVfpOp_Vcmp_Vcmpe_Rule_A1_instance_;

  if ((inst.Bits() & 0x000F0000) == 0x00050000 /* opc2(19:16)=0101 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x0000002F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000 */)
    return CondVfpOp_Vcmp_Vcmpe_Rule_A2_instance_;

  if ((inst.Bits() & 0x000F0000) == 0x00070000 /* opc2(19:16)=0111 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6)=11 */)
    return CondVfpOp_Vcvt_Rule_298_A1_P584_instance_;

  if ((inst.Bits() & 0x000F0000) == 0x00080000 /* opc2(19:16)=1000 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */)
    return CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_;

  if ((inst.Bits() & 0x000E0000) == 0x00020000 /* opc2(19:16)=001x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx */)
    return CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588_instance_;

  if ((inst.Bits() & 0x000E0000) == 0x000C0000 /* opc2(19:16)=110x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */)
    return CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_;

  if ((inst.Bits() & 0x000A0000) == 0x000A0000 /* opc2(19:16)=1x1x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */)
    return CondVfpOp_Vcvt_Rule_297_A1_P582_instance_;

  if ((inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */ &&
      (inst.Bits() & 0x000000A0) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx */)
    return CondVfpOp_Vmov_Rule_326_A2_P640_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table packing_unpacking_saturation_and_reversal.
 * Specified by: ('See Section A5.4.3',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_packing_unpacking_saturation_and_reversal(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:5)=xx0 */)
    return Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00200000 /* op1(22:20)=010 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00600000 /* op1(22:20)=110 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00700000 /* op1(22:20)=111 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */)
    return Unary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op1(22:20)=01x */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:5)=xx0 */)
    return Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00600000 /* op1(22:20)=11x */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:5)=xx0 */)
    return Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table parallel_addition_and_subtraction_signed.
 * Specified by: ('See Section A5.4.1',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_addition_and_subtraction_signed(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table parallel_addition_and_subtraction_unsigned.
 * Specified by: ('See Section A5.4.2',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_parallel_addition_and_subtraction_unsigned(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00100000 /* op1(21:20)=01 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5)=010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472_instance_;

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(21:20)=11 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table saturating_addition_and_subtraction.
 * Specified by: ('See Section A5.2.6',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_saturating_addition_and_subtraction(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00600000) == 0x00000000 /* op(22:21)=00 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qadd_Rule_124_A1_P250_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qsub_Rule_131_A1_P264_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00400000 /* op(22:21)=10 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qdadd_Rule_128_A1_P258_instance_;

  if ((inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Binary3RegisterOpAltBNoCondUpdates_Qdsub_Rule_129_A1_P260_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table signed_multiply_signed_and_unsigned_divide.
 * Specified by: ('See Section A5.4.4',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_signed_multiply_signed_and_unsigned_divide(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */)
    return Binary4RegisterDualOp_Smlad_Rule_167_P332_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */)
    return Binary3RegisterOpAltA_Smuad_Rule_177_P352_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5)=01x */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */)
    return Binary4RegisterDualOp_Smlsd_Rule_172_P342_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5)=01x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */)
    return Binary3RegisterOpAltA_Smusd_Rule_181_P360_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00100000 /* op1(22:20)=001 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return Binary3RegisterOpAltA_Sdiv_Rule_A1_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00300000 /* op1(22:20)=011 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */)
    return Binary3RegisterOpAltA_Udiv_Rule_A1_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */)
    return Binary4RegisterDualResult_Smlald_Rule_170_P336_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5)=01x */)
    return Binary4RegisterDualResult_Smlsld_Rule_173_P344_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */)
    return Binary4RegisterDualOp_Smmla_Rule_174_P346_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */)
    return Binary3RegisterOpAltA_Smmul_Rule_176_P350_instance_;

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* op2(7:5)=11x */)
    return Binary4RegisterDualOp_Smmls_Rule_175_P348_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table synchronization_primitives.
 * Specified by: ('See Section A5.2.10',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_synchronization_primitives(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00F00000) == 0x00800000 /* op(23:20)=1000 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400_instance_;

  if ((inst.Bits() & 0x00F00000) == 0x00900000 /* op(23:20)=1001 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */)
    return LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142_instance_;

  if ((inst.Bits() & 0x00F00000) == 0x00A00000 /* op(23:20)=1010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404_instance_;

  if ((inst.Bits() & 0x00F00000) == 0x00B00000 /* op(23:20)=1011 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */)
    return LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146_instance_;

  if ((inst.Bits() & 0x00F00000) == 0x00C00000 /* op(23:20)=1100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402_instance_;

  if ((inst.Bits() & 0x00F00000) == 0x00D00000 /* op(23:20)=1101 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */)
    return LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144_instance_;

  if ((inst.Bits() & 0x00F00000) == 0x00E00000 /* op(23:20)=1110 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */)
    return StoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406_instance_;

  if ((inst.Bits() & 0x00F00000) == 0x00F00000 /* op(23:20)=1111 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */)
    return LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148_instance_;

  if ((inst.Bits() & 0x00B00000) == 0x00000000 /* op(23:20)=0x00 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */)
    return Deprecated_Swp_Swpb_Rule_A1_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table transfer_between_arm_core_and_extension_register_8_16_and_32_bit.
 * Specified by: ('A7.8',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00000000 /* A(23:21)=000 */ &&
      (inst.Bits() & 0x0000006F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000 */)
    return MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_;

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF) == 0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */)
    return VfpUsesRegOp_Vmsr_Rule_336_A1_P660_instance_;

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23:21)=0xx */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */)
    return MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644_instance_;

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23:21)=1xx */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* B(6:5)=0x */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */)
    return DuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594_instance_;

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF) == 0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */)
    return VfpMrsOp_Vmrs_Rule_335_A1_P658_instance_;

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */)
    return MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table transfer_between_arm_core_and_extension_registers_64_bit.
 * Specified by: ('A7.9',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_transfer_between_arm_core_and_extension_registers_64_bit(
     const nacl_arm_dec::Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x000000D0) == 0x00000010 /* op(7:4)=00x1 */)
    return MoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1_instance_;

  if ((inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x000000D0) == 0x00000010 /* op(7:4)=00x1 */)
    return MoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1_instance_;

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


/*
 * Implementation of table unconditional_instructions.
 * Specified by: ('See Section A5.7',)
 */
const NamedClassDecoder& NamedArm32DecoderState::decode_unconditional_instructions(
     const nacl_arm_dec::Instruction inst) const {

  if ((inst.Bits() & 0x0FF00000) == 0x0C400000 /* op1(27:20)=11000100 */)
    return ForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188_instance_;

  if ((inst.Bits() & 0x0FF00000) == 0x0C500000 /* op1(27:20)=11000101 */)
    return ForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204_instance_;

  if ((inst.Bits() & 0x0E500000) == 0x08100000 /* op1(27:20)=100xx0x1 */ &&
      (inst.Bits() & 0x0000FFFF) == 0x00000A00 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000 */)
    return ForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16_instance_;

  if ((inst.Bits() & 0x0E500000) == 0x08400000 /* op1(27:20)=100xx1x0 */ &&
      (inst.Bits() & 0x000FFFE0) == 0x000D0500 /* $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx */)
    return ForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20_instance_;

  if ((inst.Bits() & 0x0F100000) == 0x0E000000 /* op1(27:20)=1110xxx0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */)
    return ForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186_instance_;

  if ((inst.Bits() & 0x0F100000) == 0x0E100000 /* op1(27:20)=1110xxx1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */)
    return ForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202_instance_;

  if ((inst.Bits() & 0x0E100000) == 0x0C000000 /* op1(27:20)=110xxxx0 */ &&
      (inst.Bits() & 0x0FB00000) != 0x0C000000 /* op1_repeated(27:20)=~11000x00 */)
    return ForbiddenUncondDecoder_Stc2_Rule_188_A2_P372_instance_;

  if ((inst.Bits() & 0x0E100000) == 0x0C100000 /* op1(27:20)=110xxxx1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0FB00000) != 0x0C100000 /* op1_repeated(27:20)=~11000x01 */)
    return ForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106_instance_;

  if ((inst.Bits() & 0x0E100000) == 0x0C100000 /* op1(27:20)=110xxxx1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x0FB00000) != 0x0C100000 /* op1_repeated(27:20)=~11000x01 */)
    return ForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108_instance_;

  if ((inst.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20)=1110xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */)
    return ForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68_instance_;

  if ((inst.Bits() & 0x0E000000) == 0x0A000000 /* op1(27:20)=101xxxxx */)
    return ForbiddenUncondDecoder_Blx_Rule_23_A2_P58_instance_;

  if ((inst.Bits() & 0x08000000) == 0x00000000 /* op1(27:20)=0xxxxxxx */)
    return decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(inst);

  if (true &&
      true /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */)
    return Undefined_None_instance_;

  // Catch any attempt to fall through...
  return not_implemented_;
}


const NamedClassDecoder& NamedArm32DecoderState::
decode_named(const nacl_arm_dec::Instruction inst) const {
  return decode_ARMv7(inst);
}

const nacl_arm_dec::ClassDecoder& NamedArm32DecoderState::
decode(const nacl_arm_dec::Instruction inst) const {
  return decode_named(inst).named_decoder();
}

}  // namespace nacl_arm_test
