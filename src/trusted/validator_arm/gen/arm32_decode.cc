/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE


#include "native_client/src/trusted/validator_arm/gen/arm32_decode.h"

namespace nacl_arm_dec {


Arm32DecoderState::Arm32DecoderState() : DecoderState()
  , Branch_instance_()
  , Breakpoint_instance_()
  , BxBlx_instance_()
  , DataBarrier_instance_()
  , Defs12To15_instance_()
  , Defs12To15CondsDontCareMsbGeLsb_instance_()
  , Defs12To15CondsDontCareRdRnNotPc_instance_()
  , Defs12To15CondsDontCareRdRnNotPcBitfieldExtract_instance_()
  , Defs12To15CondsDontCareRnRdRmNotPc_instance_()
  , Defs12To15RdRmRnNotPc_instance_()
  , Defs12To15RdRnNotPc_instance_()
  , Defs12To15RdRnRsRmNotPc_instance_()
  , Defs12To19CondsDontCareRdRmRnNotPc_instance_()
  , Defs16To19CondsDontCareRdRaRmRnNotPc_instance_()
  , Defs16To19CondsDontCareRdRmRnNotPc_instance_()
  , Deprecated_instance_()
  , DontCareInst_instance_()
  , DontCareInstRdNotPc_instance_()
  , DontCareInstRnRsRmNotPc_instance_()
  , DuplicateToAdvSIMDRegisters_instance_()
  , Forbidden_instance_()
  , InstructionBarrier_instance_()
  , LoadBasedImmedMemory_instance_()
  , LoadBasedImmedMemoryDouble_instance_()
  , LoadBasedMemory_instance_()
  , LoadBasedMemoryDouble_instance_()
  , LoadBasedOffsetMemory_instance_()
  , LoadBasedOffsetMemoryDouble_instance_()
  , LoadMultiple_instance_()
  , LoadVectorRegister_instance_()
  , LoadVectorRegisterList_instance_()
  , MaskAddress_instance_()
  , MoveDoubleVfpRegisterOp_instance_()
  , MoveVfpRegisterOp_instance_()
  , MoveVfpRegisterOpWithTypeSel_instance_()
  , NotImplemented_instance_()
  , PreloadRegisterPairOp_instance_()
  , PreloadRegisterPairOpWAndRnNotPc_instance_()
  , Roadblock_instance_()
  , Store2RegisterImm12OpRnNotRtOnWriteback_instance_()
  , StoreBasedImmedMemory_instance_()
  , StoreBasedImmedMemoryDouble_instance_()
  , StoreBasedMemoryDoubleRtBits0To3_instance_()
  , StoreBasedMemoryRtBits0To3_instance_()
  , StoreBasedOffsetMemory_instance_()
  , StoreBasedOffsetMemoryDouble_instance_()
  , StoreRegisterList_instance_()
  , StoreVectorRegister_instance_()
  , StoreVectorRegisterList_instance_()
  , TestIfAddressMasked_instance_()
  , Unary1RegisterBitRangeMsbGeLsb_instance_()
  , Unary1RegisterSet_instance_()
  , Unary1RegisterUse_instance_()
  , Undefined_instance_()
  , Unpredictable_instance_()
  , VfpMrsOp_instance_()
  , VfpOp_instance_()
  , not_implemented_()
{}

// Implementation of table: ARMv7.
// Specified by: See Section A5.1
const ClassDecoder& Arm32DecoderState::decode_ARMv7(
     const Instruction inst) const
{
  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x04000000 /* op1(27:25)=010 */) {
    return decode_load_store_word_and_unsigned_byte(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25)=011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */) {
    return decode_load_store_word_and_unsigned_byte(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25)=011 */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */) {
    return decode_media_instructions(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x00000000 /* op1(27:25)=00x */) {
    return decode_data_processing_and_miscellaneous_instructions(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x08000000 /* op1(27:25)=10x */) {
    return decode_branch_branch_with_link_and_block_data_transfer(inst);
  }

  if ((inst.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28)=~1111 */ &&
      (inst.Bits() & 0x0C000000) == 0x0C000000 /* op1(27:25)=11x */) {
    return decode_coprocessor_instructions_and_supervisor_call(inst);
  }

  if ((inst.Bits() & 0xF0000000) == 0xF0000000 /* cond(31:28)=1111 */) {
    return decode_unconditional_instructions(inst);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: branch_branch_with_link_and_block_data_transfer.
// Specified by: See Section A5.5
const ClassDecoder& Arm32DecoderState::decode_branch_branch_with_link_and_block_data_transfer(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x02500000) == 0x00000000 /* op(25:20)=0xx0x0 */) {
    return StoreRegisterList_instance_;
  }

  if ((inst.Bits() & 0x02500000) == 0x00100000 /* op(25:20)=0xx0x1 */) {
    return LoadMultiple_instance_;
  }

  if ((inst.Bits() & 0x02500000) == 0x00400000 /* op(25:20)=0xx1x0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x02500000) == 0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000) == 0x00000000 /* R(15)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x02500000) == 0x00500000 /* op(25:20)=0xx1x1 */ &&
      (inst.Bits() & 0x00008000) == 0x00008000 /* R(15)=1 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25:20)=1xxxxx */) {
    return Branch_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: coprocessor_instructions_and_supervisor_call.
// Specified by: See Section A5.6
const ClassDecoder& Arm32DecoderState::decode_coprocessor_instructions_and_supervisor_call(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x03E00000) == 0x00000000 /* op1(25:20)=00000x */) {
    return Undefined_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20)=00010x */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000) == 0x00000000 /* op1(25:20)=0xxxx0 */ &&
      (inst.Bits() & 0x03B00000) != 0x00000000 /* op1_repeated(25:20)=~000x00 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x02100000) == 0x00100000 /* op1(25:20)=0xxxx1 */ &&
      (inst.Bits() & 0x03B00000) != 0x00100000 /* op1_repeated(25:20)=~000x01 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8)=~101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20)=00010x */) {
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op(4)=0 */) {
    return decode_floating_point_data_processing_instructions(inst);
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x03000000) == 0x02000000 /* op1(25:20)=10xxxx */ &&
      (inst.Bits() & 0x00000010) == 0x00000010 /* op(4)=1 */) {
    return decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(inst);
  }

  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */ &&
      (inst.Bits() & 0x02000000) == 0x00000000 /* op1(25:20)=0xxxxx */ &&
      (inst.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20)=~000x0x */) {
    return decode_extension_register_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x03000000) == 0x03000000 /* op1(25:20)=11xxxx */) {
    return Forbidden_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: data_processing_and_miscellaneous_instructions.
// Specified by: See Section A5.2
const ClassDecoder& Arm32DecoderState::decode_data_processing_and_miscellaneous_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */ &&
      (inst.Bits() & 0x00000090) == 0x00000010 /* op2(7:4)=0xx1 */) {
    return decode_data_processing_register_shifted_register(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */) {
    return decode_data_processing_register(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) == 0x01000000 /* op1(24:20)=10xx0 */ &&
      (inst.Bits() & 0x00000090) == 0x00000080 /* op2(7:4)=1xx0 */) {
    return decode_halfword_multiply_and_multiply_accumulate(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01900000) == 0x01000000 /* op1(24:20)=10xx0 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:4)=0xxx */) {
    return decode_miscellaneous_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20)=~0xx1x */ &&
      (inst.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4)=1011 */) {
    return decode_extra_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20)=~0xx1x */ &&
      (inst.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4)=11x1 */) {
    return decode_extra_load_store_instructions(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) == 0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4)=1011 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01200000) == 0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4)=11x1 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x00000000 /* op1(24:20)=0xxxx */ &&
      (inst.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4)=1001 */) {
    return decode_multiply_and_multiply_accumulate(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* op(25)=0 */ &&
      (inst.Bits() & 0x01000000) == 0x01000000 /* op1(24:20)=1xxxx */ &&
      (inst.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4)=1001 */) {
    return decode_synchronization_primitives(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01B00000) == 0x01000000 /* op1(24:20)=10x00 */) {
    return Defs12To15_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01B00000) == 0x01200000 /* op1(24:20)=10x10 */) {
    return decode_msr_immediate_and_hints(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* op(25)=1 */ &&
      (inst.Bits() & 0x01900000) != 0x01000000 /* op1(24:20)=~10xx0 */) {
    return decode_data_processing_immediate(inst);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: data_processing_immediate.
// Specified by: See Section A5.2.3
const ClassDecoder& Arm32DecoderState::decode_data_processing_immediate(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01F00000) == 0x01100000 /* op(24:20)=10001 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return TestIfAddressMasked_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01500000 /* op(24:20)=10101 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x01B00000) == 0x01300000 /* op(24:20)=10x11 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01800000 /* op(24:20)=1100x */) {
    return Defs12To15_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op(24:20)=1110x */) {
    return MaskAddress_instance_;
  }

  if ((inst.Bits() & 0x01A00000) == 0x01A00000 /* op(24:20)=11x1x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((inst.Bits() & 0x01000000) == 0x00000000 /* op(24:20)=0xxxx */) {
    return Defs12To15_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: data_processing_register.
// Specified by: See Section A5.2.1
const ClassDecoder& Arm32DecoderState::decode_data_processing_register(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01900000) == 0x01100000 /* op1(24:20)=10xx1 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x01A00000) == 0x01800000 /* op1(24:20)=11x0x */) {
    return Defs12To15_instance_;
  }

  if ((inst.Bits() & 0x01A00000) == 0x01A00000 /* op1(24:20)=11x1x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((inst.Bits() & 0x01000000) == 0x00000000 /* op1(24:20)=0xxxx */) {
    return Defs12To15_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: data_processing_register_shifted_register.
// Specified by: See Section A5.2.2
const ClassDecoder& Arm32DecoderState::decode_data_processing_register_shifted_register(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01900000) == 0x01100000 /* op1(24:20)=10xx1 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return DontCareInstRnRsRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x01A00000) == 0x01800000 /* op1(24:20)=11x0x */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x01A00000) == 0x01A00000 /* op1(24:20)=11x1x */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) {
    return Defs12To15RdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x01000000) == 0x00000000 /* op1(24:20)=0xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: extension_register_load_store_instructions.
// Specified by: A7.6
const ClassDecoder& Arm32DecoderState::decode_extension_register_load_store_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20)=10x10 */) {
    return StoreVectorRegisterList_instance_;
  }

  if ((inst.Bits() & 0x01B00000) == 0x01300000 /* opcode(24:20)=10x11 */) {
    return LoadVectorRegisterList_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x00400000 /* opcode(24:20)=0010x */) {
    return decode_transfer_between_arm_core_and_extension_registers_64_bit(inst);
  }

  if ((inst.Bits() & 0x01300000) == 0x01000000 /* opcode(24:20)=1xx00 */) {
    return StoreVectorRegister_instance_;
  }

  if ((inst.Bits() & 0x01300000) == 0x01100000 /* opcode(24:20)=1xx01 */) {
    return LoadVectorRegister_instance_;
  }

  if ((inst.Bits() & 0x01900000) == 0x00800000 /* opcode(24:20)=01xx0 */) {
    return StoreVectorRegisterList_instance_;
  }

  if ((inst.Bits() & 0x01900000) == 0x00900000 /* opcode(24:20)=01xx1 */) {
    return LoadVectorRegisterList_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: extra_load_store_instructions.
// Specified by: See Section A5.2.8
const ClassDecoder& Arm32DecoderState::decode_extra_load_store_instructions(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return StoreBasedOffsetMemory_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000020 /* op2(6:5)=01 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */) {
    return StoreBasedImmedMemory_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return LoadBasedOffsetMemoryDouble_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */) {
    return LoadBasedImmedMemoryDouble_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000040 /* op2(6:5)=10 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return StoreBasedOffsetMemoryDouble_instance_;
  }

  if ((inst.Bits() & 0x00000060) == 0x00000060 /* op2(6:5)=11 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */) {
    return StoreBasedImmedMemoryDouble_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op2(6:5)=x1 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((inst.Bits() & 0x00000020) == 0x00000020 /* op2(6:5)=x1 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */) {
    return LoadBasedImmedMemory_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: floating_point_data_processing_instructions.
// Specified by: A7.5 Table A7 - 16
const ClassDecoder& Arm32DecoderState::decode_floating_point_data_processing_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20)=0x11 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00800000 /* opc1(23:20)=1x00 */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00900000 /* opc1(23:20)=1x01 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00B00000 /* opc1(23:20)=1x11 */) {
    return decode_other_floating_point_data_processing_instructions(inst);
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* opc1(23:20)=xx10 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00A00000) == 0x00000000 /* opc1(23:20)=0x0x */) {
    return VfpOp_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: halfword_multiply_and_multiply_accumulate.
// Specified by: See Section A5.2.7
const ClassDecoder& Arm32DecoderState::decode_halfword_multiply_and_multiply_accumulate(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00600000) == 0x00000000 /* op1(22:21)=00 */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op(5)=0 */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00200000 /* op1(22:21)=01 */ &&
      (inst.Bits() & 0x00000020) == 0x00000020 /* op(5)=1 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00400000 /* op1(22:21)=10 */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00600000) == 0x00600000 /* op1(22:21)=11 */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: load_store_word_and_unsigned_byte.
// Specified by: See Section A5.3
const ClassDecoder& Arm32DecoderState::decode_load_store_word_and_unsigned_byte(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x01700000) != 0x00200000 /* op1_repeated(24:20)=~0x010 */) {
    return decode_load_store_word_and_unsigned_byte_str_or_push(inst);
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x01700000) != 0x00600000 /* op1_repeated(24:20)=~0x110 */) {
    return StoreBasedImmedMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */ &&
      (inst.Bits() & 0x01200000) == 0x01000000 /* $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x00000000 /* A(25)=0 */ &&
      (inst.Bits() & 0x01200000) == 0x00200000 /* op1(24:20)=0xx1x */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00000000 /* op1(24:20)=xx0x0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00200000 /* op1_repeated(24:20)=~0x010 */) {
    return StoreBasedOffsetMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00100000 /* op1(24:20)=xx0x1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00300000 /* op1_repeated(24:20)=~0x011 */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00400000 /* op1(24:20)=xx1x0 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00600000 /* op1_repeated(24:20)=~0x110 */) {
    return StoreBasedOffsetMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x00500000) == 0x00500000 /* op1(24:20)=xx1x1 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */ &&
      (inst.Bits() & 0x01700000) != 0x00700000 /* op1_repeated(24:20)=~0x111 */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((inst.Bits() & 0x02000000) == 0x02000000 /* A(25)=1 */ &&
      (inst.Bits() & 0x01200000) == 0x00200000 /* op1(24:20)=0xx1x */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* B(4)=0 */) {
    return Forbidden_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: load_store_word_and_unsigned_byte_str_or_push.
// Specified by: See Section A5.3
const ClassDecoder& Arm32DecoderState::decode_load_store_word_and_unsigned_byte_str_or_push(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x01E00000) == 0x01200000 /* Flags(24:21)=1001 */ &&
      (inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */ &&
      (inst.Bits() & 0x00000FFF) == 0x00000004 /* Imm12(11:0)=000000000100 */) {
    return Store2RegisterImm12OpRnNotRtOnWriteback_instance_;
  }

  if (true) {
    return StoreBasedImmedMemory_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: media_instructions.
// Specified by: See Section A5.4
const ClassDecoder& Arm32DecoderState::decode_media_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=~1111 */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20)=11000 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12)=1111 */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x01F00000) == 0x01F00000 /* op1(24:20)=11111 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */) {
    return Roadblock_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0)=~1111 */) {
    return Defs12To15CondsDontCareMsbGeLsb_instance_;
  }

  if ((inst.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20)=1110x */ &&
      (inst.Bits() & 0x00000060) == 0x00000000 /* op2(7:5)=x00 */ &&
      (inst.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0)=1111 */) {
    return Unary1RegisterBitRangeMsbGeLsb_instance_;
  }

  if ((inst.Bits() & 0x01A00000) == 0x01A00000 /* op1(24:20)=11x1x */ &&
      (inst.Bits() & 0x00000060) == 0x00000040 /* op2(7:5)=x10 */) {
    return Defs12To15CondsDontCareRdRnNotPcBitfieldExtract_instance_;
  }

  if ((inst.Bits() & 0x01C00000) == 0x00000000 /* op1(24:20)=000xx */) {
    return decode_parallel_addition_and_subtraction_signed(inst);
  }

  if ((inst.Bits() & 0x01C00000) == 0x00400000 /* op1(24:20)=001xx */) {
    return decode_parallel_addition_and_subtraction_unsigned(inst);
  }

  if ((inst.Bits() & 0x01800000) == 0x00800000 /* op1(24:20)=01xxx */) {
    return decode_packing_unpacking_saturation_and_reversal(inst);
  }

  if ((inst.Bits() & 0x01800000) == 0x01000000 /* op1(24:20)=10xxx */) {
    return decode_signed_multiply_signed_and_unsigned_divide(inst);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: memory_hints_advanced_simd_instructions_and_miscellaneous_instructions.
// Specified by: See Section A5.7.1
const ClassDecoder& Arm32DecoderState::decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000000 /* op2(7:4)=0000 */ &&
      (inst.Bits() & 0x00010000) == 0x00010000 /* Rn(19:16)=xxx1 */ &&
      (inst.Bits() & 0x000EFD0F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20)=0010000 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:4)=xx0x */ &&
      (inst.Bits() & 0x00010000) == 0x00000000 /* Rn(19:16)=xxx0 */ &&
      (inst.Bits() & 0x0000FE00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05300000 /* op1(26:20)=1010011 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000000 /* op2(7:4)=0000 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000010 /* op2(7:4)=0001 */ &&
      (inst.Bits() & 0x000FFF0F) == 0x000FF00F /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000060 /* op2(7:4)=0110 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return InstructionBarrier_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000F0) == 0x00000070 /* op2(7:4)=0111 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:4)=001x */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000040 /* op2(7:4)=010x */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FF000 /* $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx */) {
    return DataBarrier_instance_;
  }

  if ((inst.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20)=1010111 */ &&
      (inst.Bits() & 0x00000080) == 0x00000080 /* op2(7:4)=1xxx */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x04100000 /* op1(26:20)=100x001 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x04500000 /* op1(26:20)=100x101 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x05100000 /* op1(26:20)=101x001 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x05500000 /* op1(26:20)=101x101 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x06100000 /* op1(26:20)=110x001 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x07700000) == 0x06500000 /* op1(26:20)=110x101 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOp_instance_;
  }

  if ((inst.Bits() & 0x07B00000) == 0x05B00000 /* op1(26:20)=1011x11 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07300000) == 0x04300000 /* op1(26:20)=100xx11 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07300000) == 0x05100000 /* op1(26:20)=101xx01 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x07300000) == 0x07100000 /* op1(26:20)=111xx01 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return PreloadRegisterPairOpWAndRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x06300000) == 0x06300000 /* op1(26:20)=11xxx11 */ &&
      (inst.Bits() & 0x00000010) == 0x00000000 /* op2(7:4)=xxx0 */) {
    return Unpredictable_instance_;
  }

  if ((inst.Bits() & 0x07100000) == 0x04000000 /* op1(26:20)=100xxx0 */) {
    return NotImplemented_instance_;
  }

  if ((inst.Bits() & 0x06000000) == 0x02000000 /* op1(26:20)=01xxxxx */) {
    return NotImplemented_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: miscellaneous_instructions.
// Specified by: See Section A5.2.12
const ClassDecoder& Arm32DecoderState::decode_miscellaneous_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00030000) == 0x00000000 /* op1(19:16)=xx00 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Unary1RegisterUse_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00030000) == 0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x00020000) == 0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x0000FD00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000000 /* B(9)=0 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x000F0D0F) == 0x000F0000 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000 */) {
    return Unary1RegisterSet_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00000000 /* op(22:21)=x0 */ &&
      (inst.Bits() & 0x00000C0F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000000 /* op2(6:4)=000 */ &&
      (inst.Bits() & 0x00000200) == 0x00000200 /* B(9)=1 */ &&
      (inst.Bits() & 0x00200000) == 0x00200000 /* op(22:21)=x1 */ &&
      (inst.Bits() & 0x0000FC00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000010 /* op2(6:4)=001 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Defs12To15RdRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000020 /* op2(6:4)=010 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000050 /* op2(6:4)=101 */) {
    return decode_saturating_addition_and_subtraction(inst);
  }

  if ((inst.Bits() & 0x00000070) == 0x00000060 /* op2(6:4)=110 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF0F) == 0x0000000E /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */) {
    return Breakpoint_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00400000 /* op(22:21)=10 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000070) == 0x00000070 /* op2(6:4)=111 */ &&
      (inst.Bits() & 0x00600000) == 0x00600000 /* op(22:21)=11 */ &&
      (inst.Bits() & 0x000FFF00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00000050) == 0x00000010 /* op2(6:4)=0x1 */ &&
      (inst.Bits() & 0x00600000) == 0x00200000 /* op(22:21)=01 */ &&
      (inst.Bits() & 0x000FFF00) == 0x000FFF00 /* $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx */) {
    return BxBlx_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: msr_immediate_and_hints.
// Specified by: See Section A5.2.11
const ClassDecoder& Arm32DecoderState::decode_msr_immediate_and_hints(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FF) == 0x00000004 /* op2(7:0)=00000100 */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FE) == 0x00000000 /* op2(7:0)=0000000x */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000FE) == 0x00000002 /* op2(7:0)=0000001x */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16)=0000 */ &&
      (inst.Bits() & 0x000000F0) == 0x000000F0 /* op2(7:0)=1111xxxx */ &&
      (inst.Bits() & 0x0000FF00) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000F0000) == 0x00040000 /* op1(19:16)=0100 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x000B0000) == 0x00080000 /* op1(19:16)=1x00 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00030000) == 0x00010000 /* op1(19:16)=xx01 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00000000 /* op(22)=0 */ &&
      (inst.Bits() & 0x00020000) == 0x00020000 /* op1(19:16)=xx1x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x00400000) == 0x00400000 /* op(22)=1 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if (true) {
    return Forbidden_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: multiply_and_multiply_accumulate.
// Specified by: See Section A5.2.5
const ClassDecoder& Arm32DecoderState::decode_multiply_and_multiply_accumulate(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00F00000) == 0x00400000 /* op(23:20)=0100 */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00600000 /* op(23:20)=0110 */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00D00000) == 0x00500000 /* op(23:20)=01x1 */) {
    return Undefined_instance_;
  }

  if ((inst.Bits() & 0x00E00000) == 0x00000000 /* op(23:20)=000x */ &&
      (inst.Bits() & 0x0000F000) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00E00000) == 0x00200000 /* op(23:20)=001x */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00800000) == 0x00800000 /* op(23:20)=1xxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: other_floating_point_data_processing_instructions.
// Specified by: A7.5 Table A7 - 17
const ClassDecoder& Arm32DecoderState::decode_other_floating_point_data_processing_instructions(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16)=0001 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00050000 /* opc2(19:16)=0101 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x0000002F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00070000 /* opc2(19:16)=0111 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6)=11 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000F0000) == 0x00080000 /* opc2(19:16)=1000 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000B0000) == 0x00000000 /* opc2(19:16)=0x00 */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000E0000) == 0x00020000 /* opc2(19:16)=001x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000E0000) == 0x000C0000 /* opc2(19:16)=110x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x000A0000) == 0x000A0000 /* opc2(19:16)=1x1x */ &&
      (inst.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6)=x1 */) {
    return VfpOp_instance_;
  }

  if ((inst.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6)=x0 */ &&
      (inst.Bits() & 0x000000A0) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx */) {
    return VfpOp_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: packing_unpacking_saturation_and_reversal.
// Specified by: See Section A5.4.3
const ClassDecoder& Arm32DecoderState::decode_packing_unpacking_saturation_and_reversal(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5)=101 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:5)=xx0 */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(22:20)=x10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5)=001 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(22:20)=x11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(22:20)=x11 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00300000 /* op1(22:20)=x11 */ &&
      (inst.Bits() & 0x00000060) == 0x00000020 /* op2(7:5)=x01 */ &&
      (inst.Bits() & 0x000F0F00) == 0x000F0F00 /* $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* op1(22:20)=xx0 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* op1(22:20)=xx0 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5)=011 */ &&
      (inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */ &&
      (inst.Bits() & 0x00000300) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00200000) == 0x00200000 /* op1(22:20)=x1x */ &&
      (inst.Bits() & 0x00000020) == 0x00000000 /* op2(7:5)=xx0 */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: parallel_addition_and_subtraction_signed.
// Specified by: See Section A5.4.1
const ClassDecoder& Arm32DecoderState::decode_parallel_addition_and_subtraction_signed(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: parallel_addition_and_subtraction_unsigned.
// Specified by: See Section A5.4.2
const ClassDecoder& Arm32DecoderState::decode_parallel_addition_and_subtraction_unsigned(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00300000) == 0x00200000 /* op1(21:20)=10 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5)=100 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5)=111 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* op1(21:20)=x1 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: saturating_addition_and_subtraction.
// Specified by: See Section A5.2.6
const ClassDecoder& Arm32DecoderState::decode_saturating_addition_and_subtraction(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: signed_multiply_signed_and_unsigned_divide.
// Specified by: See Section A5.4.4
const ClassDecoder& Arm32DecoderState::decode_signed_multiply_signed_and_unsigned_divide(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00000000 /* op1(22:20)=000 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:5)=0xx */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00400000 /* op1(22:20)=100 */ &&
      (inst.Bits() & 0x00000080) == 0x00000000 /* op2(7:5)=0xx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5)=00x */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00700000) == 0x00500000 /* op1(22:20)=101 */ &&
      (inst.Bits() & 0x000000C0) == 0x000000C0 /* op2(7:5)=11x */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((inst.Bits() & 0x00500000) == 0x00100000 /* op1(22:20)=0x1 */ &&
      (inst.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5)=000 */ &&
      (inst.Bits() & 0x0000F000) == 0x0000F000 /* $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: synchronization_primitives.
// Specified by: See Section A5.2.10
const ClassDecoder& Arm32DecoderState::decode_synchronization_primitives(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00F00000) == 0x00A00000 /* op(23:20)=1010 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreBasedMemoryDoubleRtBits0To3_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00B00000 /* op(23:20)=1011 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadBasedMemoryDouble_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00E00000 /* op(23:20)=1110 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreBasedMemoryRtBits0To3_instance_;
  }

  if ((inst.Bits() & 0x00F00000) == 0x00F00000 /* op(23:20)=1111 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadBasedMemory_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00000000 /* op(23:20)=0x00 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) {
    return Deprecated_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00800000 /* op(23:20)=1x00 */ &&
      (inst.Bits() & 0x00000F00) == 0x00000F00 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) {
    return StoreBasedMemoryRtBits0To3_instance_;
  }

  if ((inst.Bits() & 0x00B00000) == 0x00900000 /* op(23:20)=1x01 */ &&
      (inst.Bits() & 0x00000F0F) == 0x00000F0F /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) {
    return LoadBasedMemory_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: transfer_between_arm_core_and_extension_register_8_16_and_32_bit.
// Specified by: A7.8
const ClassDecoder& Arm32DecoderState::decode_transfer_between_arm_core_and_extension_register_8_16_and_32_bit(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00000000 /* A(23:21)=000 */ &&
      (inst.Bits() & 0x0000006F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000 */) {
    return MoveVfpRegisterOp_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF) == 0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */) {
    return DontCareInstRdNotPc_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00000000 /* A(23:21)=0xx */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return MoveVfpRegisterOpWithTypeSel_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00000000 /* L(20)=0 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x00800000) == 0x00800000 /* A(23:21)=1xx */ &&
      (inst.Bits() & 0x00000040) == 0x00000000 /* B(6:5)=0x */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return DuplicateToAdvSIMDRegisters_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000000 /* C(8)=0 */ &&
      (inst.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21)=111 */ &&
      (inst.Bits() & 0x000F00EF) == 0x00010000 /* $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000 */) {
    return VfpMrsOp_instance_;
  }

  if ((inst.Bits() & 0x00100000) == 0x00100000 /* L(20)=1 */ &&
      (inst.Bits() & 0x00000100) == 0x00000100 /* C(8)=1 */ &&
      (inst.Bits() & 0x0000000F) == 0x00000000 /* $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) {
    return MoveVfpRegisterOpWithTypeSel_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: transfer_between_arm_core_and_extension_registers_64_bit.
// Specified by: A7.9
const ClassDecoder& Arm32DecoderState::decode_transfer_between_arm_core_and_extension_registers_64_bit(
     const Instruction inst) const
{
  UNREFERENCED_PARAMETER(inst);
  if ((inst.Bits() & 0x000000D0) == 0x00000010 /* op(7:4)=00x1 */) {
    return MoveDoubleVfpRegisterOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: unconditional_instructions.
// Specified by: See Section A5.7
const ClassDecoder& Arm32DecoderState::decode_unconditional_instructions(
     const Instruction inst) const
{
  if ((inst.Bits() & 0x0FE00000) == 0x0C400000 /* op1(27:20)=1100010x */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E500000) == 0x08100000 /* op1(27:20)=100xx0x1 */ &&
      (inst.Bits() & 0x0000FFFF) == 0x00000A00 /* $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E500000) == 0x08400000 /* op1(27:20)=100xx1x0 */ &&
      (inst.Bits() & 0x000FFFE0) == 0x000D0500 /* $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E100000) == 0x0C000000 /* op1(27:20)=110xxxx0 */ &&
      (inst.Bits() & 0x0FB00000) != 0x0C000000 /* op1_repeated(27:20)=~11000x00 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E100000) == 0x0C100000 /* op1(27:20)=110xxxx1 */ &&
      (inst.Bits() & 0x0FB00000) != 0x0C100000 /* op1_repeated(27:20)=~11000x01 */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20)=1110xxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x0E000000) == 0x0A000000 /* op1(27:20)=101xxxxx */) {
    return Forbidden_instance_;
  }

  if ((inst.Bits() & 0x08000000) == 0x00000000 /* op1(27:20)=0xxxxxxx */) {
    return decode_memory_hints_advanced_simd_instructions_and_miscellaneous_instructions(inst);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

const ClassDecoder& Arm32DecoderState::decode(const Instruction inst) const {
  return decode_ARMv7(inst);
}

}  // namespace nacl_arm_dec
