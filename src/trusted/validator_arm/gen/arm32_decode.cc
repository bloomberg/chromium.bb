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
  , CoprocessorOp_instance_()
  , DataProc_instance_()
  , Defs12To15_instance_()
  , Defs12To15CondsDontCare_instance_()
  , Defs12To15CondsDontCareRdRnNotPc_instance_()
  , Defs12To15CondsDontCareRdRnRsRmNotPc_instance_()
  , Defs12To15CondsDontCareRnRdRmNotPc_instance_()
  , Defs12To15RdRmRnNotPc_instance_()
  , Defs12To15RdRnNotPc_instance_()
  , Defs12To15RdRnRsRmNotPc_instance_()
  , Defs12To19CondsDontCareRdRmRnNotPc_instance_()
  , Defs16To19CondsDontCareRdRaRmRnNotPc_instance_()
  , Defs16To19CondsDontCareRdRmRnNotPc_instance_()
  , Deprecated_instance_()
  , DontCareInst_instance_()
  , DontCareInstRnRsRmNotPc_instance_()
  , EffectiveNoOp_instance_()
  , Forbidden_instance_()
  , LoadBasedImmedMemory_instance_()
  , LoadBasedImmedMemoryDouble_instance_()
  , LoadBasedMemory_instance_()
  , LoadBasedMemoryDouble_instance_()
  , LoadBasedOffsetMemory_instance_()
  , LoadBasedOffsetMemoryDouble_instance_()
  , LoadCoprocessor_instance_()
  , LoadMultiple_instance_()
  , LoadVectorRegister_instance_()
  , LoadVectorRegisterList_instance_()
  , MaskAddress_instance_()
  , MoveDoubleFromCoprocessor_instance_()
  , MoveFromCoprocessor_instance_()
  , Roadblock_instance_()
  , StoreBasedImmedMemory_instance_()
  , StoreBasedImmedMemoryDouble_instance_()
  , StoreBasedMemoryDoubleRtBits0To3_instance_()
  , StoreBasedMemoryRtBits0To3_instance_()
  , StoreBasedOffsetMemory_instance_()
  , StoreBasedOffsetMemoryDouble_instance_()
  , StoreCoprocessor_instance_()
  , StoreRegisterList_instance_()
  , StoreVectorRegister_instance_()
  , StoreVectorRegisterList_instance_()
  , TestIfAddressMasked_instance_()
  , Unary1RegisterBitRange_instance_()
  , Unary1RegisterSet_instance_()
  , Unary1RegisterUse_instance_()
  , Undefined_instance_()
  , Unpredictable_instance_()
  , VectorLoad_instance_()
  , VectorStore_instance_()
  , VfpOp_instance_()
  , not_implemented_()
{}

Arm32DecoderState::~Arm32DecoderState() {}

// Implementation of table: ARMv7.
// Specified by: See Section A5.1
const ClassDecoder& Arm32DecoderState::decode_ARMv7(
     const Instruction insn) const
{
  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x04000000 /* op1(27:25) == 010 */) {
    return decode_load_store_word_byte(insn);
  }

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25) == 011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */) {
    return decode_load_store_word_byte(insn);
  }

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25) == 011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */) {
    return decode_media(insn);
  }

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x00000000 /* op1(27:25) == 00x */) {
    return decode_dp_misc(insn);
  }

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x08000000 /* op1(27:25) == 10x */) {
    return decode_branch_block_xfer(insn);
  }

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x0C000000 /* op1(27:25) == 11x */) {
    return decode_super_cop(insn);
  }

  if ((insn.Bits() & 0xF0000000) == 0xF0000000 /* cond(31:28) == 1111 */) {
    return decode_unconditional(insn);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: branch_block_xfer.
// Specified by: See Section A5.5
const ClassDecoder& Arm32DecoderState::decode_branch_block_xfer(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x03D00000) == 0x00000000 /* op(25:20) == 0000x0 */ &&
      (insn.Bits() & 0x0FD00000) == 0x08000000 /* $pattern(31:0) == xxxx100000x0xxxxxxxxxxxxxxxxxxxx */) {
    return StoreRegisterList_instance_;
  }

  if ((insn.Bits() & 0x03D00000) == 0x00100000 /* op(25:20) == 0000x1 */ &&
      (insn.Bits() & 0x0FD00000) == 0x08100000 /* $pattern(31:0) == xxxx100000x1xxxxxxxxxxxxxxxxxxxx */) {
    return LoadMultiple_instance_;
  }

  if ((insn.Bits() & 0x03D00000) == 0x00800000 /* op(25:20) == 0010x0 */ &&
      (insn.Bits() & 0x0FD00000) == 0x08800000 /* $pattern(31:0) == xxxx100010x0xxxxxxxxxxxxxxxxxxxx */) {
    return StoreRegisterList_instance_;
  }

  if ((insn.Bits() & 0x03D00000) == 0x00900000 /* op(25:20) == 0010x1 */ &&
      (insn.Bits() & 0x0FD00000) == 0x08900000 /* $pattern(31:0) == xxxx100010x1xxxxxxxxxxxxxxxxxxxx */) {
    return LoadMultiple_instance_;
  }

  if ((insn.Bits() & 0x03D00000) == 0x01000000 /* op(25:20) == 0100x0 */ &&
      (insn.Bits() & 0x0FD00000) == 0x09000000 /* $pattern(31:0) == xxxx100100x0xxxxxxxxxxxxxxxxxxxx */) {
    return StoreRegisterList_instance_;
  }

  if ((insn.Bits() & 0x03D00000) == 0x01100000 /* op(25:20) == 0100x1 */ &&
      (insn.Bits() & 0x0FD00000) == 0x09100000 /* $pattern(31:0) == xxxx100100x1xxxxxxxxxxxxxxxxxxxx */) {
    return LoadMultiple_instance_;
  }

  if ((insn.Bits() & 0x03D00000) == 0x01800000 /* op(25:20) == 0110x0 */ &&
      (insn.Bits() & 0x0FD00000) == 0x09800000 /* $pattern(31:0) == xxxx100110x0xxxxxxxxxxxxxxxxxxxx */) {
    return StoreRegisterList_instance_;
  }

  if ((insn.Bits() & 0x03D00000) == 0x01900000 /* op(25:20) == 0110x1 */ &&
      (insn.Bits() & 0x0FD00000) == 0x09900000 /* $pattern(31:0) == xxxx100110x1xxxxxxxxxxxxxxxxxxxx */) {
    return LoadMultiple_instance_;
  }

  if ((insn.Bits() & 0x02500000) == 0x00400000 /* op(25:20) == 0xx1x0 */ &&
      (insn.Bits() & 0x0E700000) == 0x08400000 /* $pattern(31:0) == xxxx100xx100xxxxxxxxxxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02500000) == 0x00500000 /* op(25:20) == 0xx1x1 */ &&
      (insn.Bits() & 0x00008000) == 0x00000000 /* R(15:15) == 0 */ &&
      (insn.Bits() & 0x0E708000) == 0x08500000 /* $pattern(31:0) == xxxx100xx101xxxx0xxxxxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02500000) == 0x00500000 /* op(25:20) == 0xx1x1 */ &&
      (insn.Bits() & 0x00008000) == 0x00008000 /* R(15:15) == 1 */ &&
      (insn.Bits() & 0x0E508000) == 0x08508000 /* $pattern(31:0) == xxxx100xx1x1xxxx1xxxxxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x0F000000) == 0x0A000000 /* $pattern(31:0) == xxxx1010xxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Branch_instance_;
  }

  if ((insn.Bits() & 0x03000000) == 0x03000000 /* op(25:20) == 11xxxx */ &&
      (insn.Bits() & 0x0F000000) == 0x0B000000 /* $pattern(31:0) == xxxx1011xxxxxxxxxxxxxxxxxxxxxxxx */) {
    return Branch_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: dp_immed.
// Specified by: See Section A5.2.3
const ClassDecoder& Arm32DecoderState::decode_dp_immed(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x00400000 /* op(24:20) == 00100 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF0000) == 0x024F0000 /* $pattern(31:0) == xxxx001001001111xxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x00500000 /* op(24:20) == 00101 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF0000) == 0x025F0000 /* $pattern(31:0) == xxxx001001011111xxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x00800000 /* op(24:20) == 01000 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF0000) == 0x028F0000 /* $pattern(31:0) == xxxx001010001111xxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x00900000 /* op(24:20) == 01001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF0000) == 0x029F0000 /* $pattern(31:0) == xxxx001010011111xxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op(24:20) == 10001 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x03100000 /* $pattern(31:0) == xxxx00110001xxxx0000xxxxxxxxxxxx */) {
    return TestIfAddressMasked_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op(24:20) == 10011 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x03300000 /* $pattern(31:0) == xxxx00110011xxxx0000xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op(24:20) == 10101 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x03500000 /* $pattern(31:0) == xxxx00110101xxxx0000xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op(24:20) == 10111 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x03700000 /* $pattern(31:0) == xxxx00110111xxxx0000xxxxxxxxxxxx */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op(24:20) == 0000x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02000000 /* $pattern(31:0) == xxxx0010000xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op(24:20) == 0001x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02200000 /* $pattern(31:0) == xxxx0010001xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op(24:20) == 0010x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FE00000) == 0x02400000 /* $pattern(31:0) == xxxx0010010xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op(24:20) == 0011x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02600000 /* $pattern(31:0) == xxxx0010011xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op(24:20) == 0100x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FE00000) == 0x02800000 /* $pattern(31:0) == xxxx0010100xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op(24:20) == 0101x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02A00000 /* $pattern(31:0) == xxxx0010101xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op(24:20) == 0110x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02C00000 /* $pattern(31:0) == xxxx0010110xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op(24:20) == 0111x */ &&
      (insn.Bits() & 0x0FE00000) == 0x02E00000 /* $pattern(31:0) == xxxx0010111xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op(24:20) == 1100x */ &&
      (insn.Bits() & 0x0FE00000) == 0x03800000 /* $pattern(31:0) == xxxx0011100xxxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op(24:20) == 1101x */ &&
      (insn.Bits() & 0x0FEF0000) == 0x03A00000 /* $pattern(31:0) == xxxx0011101x0000xxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op(24:20) == 1110x */ &&
      (insn.Bits() & 0x0FE00000) == 0x03C00000 /* $pattern(31:0) == xxxx0011110xxxxxxxxxxxxxxxxxxxxx */) {
    return MaskAddress_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op(24:20) == 1111x */ &&
      (insn.Bits() & 0x0FEF0000) == 0x03E00000 /* $pattern(31:0) == xxxx0011111x0000xxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: dp_misc.
// Specified by: See Section A5.2
const ClassDecoder& Arm32DecoderState::decode_dp_misc(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* op2(7:4) == 0xx1 */) {
    return decode_dp_reg_shifted(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */) {
    return decode_dp_reg(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) == 0x01000000 /* op1(24:20) == 10xx0 */ &&
      (insn.Bits() & 0x00000090) == 0x00000080 /* op2(7:4) == 1xx0 */) {
    return decode_half_mult(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) == 0x01000000 /* op1(24:20) == 10xx0 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:4) == 0xxx */) {
    return decode_misc(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) != 0x00200000 /* op1(24:20) == ~0xx1x */ &&
      (insn.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4) == 1011 */) {
    return decode_extra_load_store(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) != 0x00200000 /* op1(24:20) == ~0xx1x */ &&
      (insn.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4) == 11x1 */) {
    return decode_extra_load_store(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4) == 1011 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4) == 11x1 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* op1(24:20) == 0xxxx */ &&
      (insn.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4) == 1001 */) {
    return decode_mult(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* op1(24:20) == 1xxxx */ &&
      (insn.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4) == 1001 */) {
    return decode_sync(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01F00000) == 0x01000000 /* op1(24:20) == 10000 */ &&
      (insn.Bits() & 0x0FF00000) == 0x03000000 /* $pattern(31:0) == xxxx00110000xxxxxxxxxxxxxxxxxxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01F00000) == 0x01400000 /* op1(24:20) == 10100 */) {
    return DataProc_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01B00000) == 0x01200000 /* op1(24:20) == 10x10 */) {
    return decode_msr_and_hints(insn);
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */) {
    return decode_dp_immed(insn);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: dp_reg.
// Specified by: See Section A5.2.1
const ClassDecoder& Arm32DecoderState::decode_dp_reg(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20) == 10001 */ &&
      (insn.Bits() & 0x0FF0F010) == 0x01100000 /* $pattern(31:0) == xxxx00010001xxxx0000xxxxxxx0xxxx */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20) == 10011 */ &&
      (insn.Bits() & 0x0FF0F010) == 0x01300000 /* $pattern(31:0) == xxxx00010011xxxx0000xxxxxxx0xxxx */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20) == 10101 */ &&
      (insn.Bits() & 0x0FF0F010) == 0x01500000 /* $pattern(31:0) == xxxx00010101xxxx0000xxxxxxx0xxxx */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20) == 10111 */ &&
      (insn.Bits() & 0x0FF0F010) == 0x01700000 /* $pattern(31:0) == xxxx00010111xxxx0000xxxxxxx0xxxx */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20) == 0000x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00000000 /* $pattern(31:0) == xxxx0000000xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20) == 0001x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00200000 /* $pattern(31:0) == xxxx0000001xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15CondsDontCare_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20) == 0010x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00400000 /* $pattern(31:0) == xxxx0000010xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20) == 0011x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00600000 /* $pattern(31:0) == xxxx0000011xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20) == 0100x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00800000 /* $pattern(31:0) == xxxx0000100xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20) == 0101x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00A00000 /* $pattern(31:0) == xxxx0000101xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20) == 0110x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00C00000 /* $pattern(31:0) == xxxx0000110xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20) == 0111x */ &&
      (insn.Bits() & 0x0FE00010) == 0x00E00000 /* $pattern(31:0) == xxxx0000111xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20) == 1100x */ &&
      (insn.Bits() & 0x0FE00010) == 0x01800000 /* $pattern(31:0) == xxxx0001100xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */ &&
      (insn.Bits() & 0x0FEF0070) == 0x01A00000 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxxx000xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */ &&
      (insn.Bits() & 0x0FEF0070) == 0x01A00060 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxxx110xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */ &&
      (insn.Bits() & 0x0FEF0FF0) == 0x01A00000 /* $pattern(31:0) == xxxx0001101x0000xxxx00000000xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */ &&
      (insn.Bits() & 0x0FEF0FF0) == 0x01A00060 /* $pattern(31:0) == xxxx0001101x0000xxxx00000110xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000020 /* op3(6:5) == 01 */ &&
      (insn.Bits() & 0x0FEF0070) == 0x01A00020 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxxx010xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op3(6:5) == 10 */ &&
      (insn.Bits() & 0x0FEF0070) == 0x01A00040 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxxx100xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x0FE00010) == 0x01C00000 /* $pattern(31:0) == xxxx0001110xxxxxxxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */ &&
      (insn.Bits() & 0x0FEF0010) == 0x01E00000 /* $pattern(31:0) == xxxx0001111x0000xxxxxxxxxxx0xxxx */) {
    return Defs12To15_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: dp_reg_shifted.
// Specified by: See Section A5.2.2
const ClassDecoder& Arm32DecoderState::decode_dp_reg_shifted(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op1(24:20) == 10001 */ &&
      (insn.Bits() & 0x0FF0F090) == 0x01100010 /* $pattern(31:0) == xxxx00010001xxxx0000xxxx0xx1xxxx */) {
    return DontCareInstRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01300000 /* op1(24:20) == 10011 */ &&
      (insn.Bits() & 0x0FF0F090) == 0x01300010 /* $pattern(31:0) == xxxx00010011xxxx0000xxxx0xx1xxxx */) {
    return DontCareInstRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op1(24:20) == 10101 */ &&
      (insn.Bits() & 0x0FF0F090) == 0x01500010 /* $pattern(31:0) == xxxx00010101xxxx0000xxxx0xx1xxxx */) {
    return DontCareInstRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01700000 /* op1(24:20) == 10111 */ &&
      (insn.Bits() & 0x0FF0F090) == 0x01700010 /* $pattern(31:0) == xxxx00010111xxxx0000xxxx0xx1xxxx */) {
    return DontCareInstRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00000000 /* op1(24:20) == 0000x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00000010 /* $pattern(31:0) == xxxx0000000xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20) == 0001x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00200010 /* $pattern(31:0) == xxxx0000001xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15CondsDontCareRdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op1(24:20) == 0010x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00400010 /* $pattern(31:0) == xxxx0000010xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20) == 0011x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00600010 /* $pattern(31:0) == xxxx0000011xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20) == 0100x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00800010 /* $pattern(31:0) == xxxx0000100xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op1(24:20) == 0101x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00A00010 /* $pattern(31:0) == xxxx0000101xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op1(24:20) == 0110x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00C00010 /* $pattern(31:0) == xxxx0000110xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00E00000 /* op1(24:20) == 0111x */ &&
      (insn.Bits() & 0x0FE00090) == 0x00E00010 /* $pattern(31:0) == xxxx0000111xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20) == 1100x */ &&
      (insn.Bits() & 0x0FE00090) == 0x01800010 /* $pattern(31:0) == xxxx0001100xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(6:5) == 00 */ &&
      (insn.Bits() & 0x0FEF00F0) == 0x01A00010 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxx0001xxxx */) {
    return Defs12To15RdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x0FEF00F0) == 0x01A00030 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxx0011xxxx */) {
    return Defs12To15RdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x0FEF00F0) == 0x01A00050 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxx0101xxxx */) {
    return Defs12To15RdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x0FEF00F0) == 0x01A00070 /* $pattern(31:0) == xxxx0001101x0000xxxxxxxx0111xxxx */) {
    return Defs12To15RdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x0FE00090) == 0x01C00010 /* $pattern(31:0) == xxxx0001110xxxxxxxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRnRsRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */ &&
      (insn.Bits() & 0x0FEF0090) == 0x01E00010 /* $pattern(31:0) == xxxx0001111x0000xxxxxxxx0xx1xxxx */) {
    return Defs12To15RdRmRnNotPc_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: ext_reg_load_store.
// Specified by: A7.6
const ClassDecoder& Arm32DecoderState::decode_ext_reg_load_store(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x01B00000) == 0x00800000 /* opcode(24:20) == 01x00 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0C800A00 /* $pattern(31:0) == xxxx11001x00xxxxxxxx101xxxxxxxxx */) {
    return StoreVectorRegisterList_instance_;
  }

  if ((insn.Bits() & 0x01B00000) == 0x00900000 /* opcode(24:20) == 01x01 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0C900A00 /* $pattern(31:0) == xxxx11001x01xxxxxxxx101xxxxxxxxx */) {
    return LoadVectorRegisterList_instance_;
  }

  if ((insn.Bits() & 0x01B00000) == 0x00A00000 /* opcode(24:20) == 01x10 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0CA00A00 /* $pattern(31:0) == xxxx11001x10xxxxxxxx101xxxxxxxxx */) {
    return StoreVectorRegisterList_instance_;
  }

  if ((insn.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20) == 01x11 */ &&
      (insn.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16) == ~1101 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0CB00A00 /* $pattern(31:0) == xxxx11001x11xxxxxxxx101xxxxxxxxx */) {
    return LoadVectorRegisterList_instance_;
  }

  if ((insn.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20) == 01x11 */ &&
      (insn.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16) == 1101 */ &&
      (insn.Bits() & 0x0FBF0E00) == 0x0CBD0A00 /* $pattern(31:0) == xxxx11001x111101xxxx101xxxxxxxxx */) {
    return LoadVectorRegisterList_instance_;
  }

  if ((insn.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20) == 10x10 */ &&
      (insn.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16) == ~1101 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0D200A00 /* $pattern(31:0) == xxxx11010x10xxxxxxxx101xxxxxxxxx */) {
    return StoreVectorRegisterList_instance_;
  }

  if ((insn.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20) == 10x10 */ &&
      (insn.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16) == 1101 */ &&
      (insn.Bits() & 0x0FBF0E00) == 0x0D2D0A00 /* $pattern(31:0) == xxxx11010x101101xxxx101xxxxxxxxx */) {
    return StoreVectorRegisterList_instance_;
  }

  if ((insn.Bits() & 0x01B00000) == 0x01300000 /* opcode(24:20) == 10x11 */ &&
      (insn.Bits() & 0x0FB00E00) == 0x0D300A00 /* $pattern(31:0) == xxxx11010x11xxxxxxxx101xxxxxxxxx */) {
    return LoadVectorRegisterList_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* opcode(24:20) == 0010x */) {
    return decode_ext_reg_transfers(insn);
  }

  if ((insn.Bits() & 0x01300000) == 0x01000000 /* opcode(24:20) == 1xx00 */ &&
      (insn.Bits() & 0x0F300E00) == 0x0D000A00 /* $pattern(31:0) == xxxx1101xx00xxxxxxxx101xxxxxxxxx */) {
    return StoreVectorRegister_instance_;
  }

  if ((insn.Bits() & 0x01300000) == 0x01100000 /* opcode(24:20) == 1xx01 */ &&
      (insn.Bits() & 0x0F300E00) == 0x0D100A00 /* $pattern(31:0) == xxxx1101xx01xxxxxxxx101xxxxxxxxx */) {
    return LoadVectorRegister_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: ext_reg_move.
// Specified by: A7.8 page A7 - 31
const ClassDecoder& Arm32DecoderState::decode_ext_reg_move(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00000000 /* A(23:21) == 000 */) {
    return CoprocessorOp_instance_;
  }

  if ((insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21) == 111 */) {
    return CoprocessorOp_instance_;
  }

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x00800000) == 0x00000000 /* A(23:21) == 0xx */) {
    return CoprocessorOp_instance_;
  }

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x00800000) == 0x00800000 /* A(23:21) == 1xx */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:5) == 0x */) {
    return CoprocessorOp_instance_;
  }

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* L(20:20) == 1 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */) {
    return CoprocessorOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: ext_reg_transfers.
// Specified by: A7.8 page A7 - 32
const ClassDecoder& Arm32DecoderState::decode_ext_reg_transfers(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x000000D0) == 0x00000010 /* op(7:4) == 00x1 */) {
    return MoveDoubleFromCoprocessor_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: extra_load_store.
// Specified by: See Section A5.2.8
const ClassDecoder& Arm32DecoderState::decode_extra_load_store(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x000000B0 /* $pattern(31:0) == xxxx000xx0x0xxxxxxxx00001011xxxx */) {
    return StoreBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x001000B0 /* $pattern(31:0) == xxxx000xx0x1xxxxxxxx00001011xxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x004000B0 /* $pattern(31:0) == xxxx000xx1x0xxxxxxxxxxxx1011xxxx */) {
    return StoreBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x005000B0 /* $pattern(31:0) == xxxx000xx1x1xxxxxxxxxxxx1011xxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F00F0) == 0x015F00B0 /* $pattern(31:0) == xxxx0001x1011111xxxxxxxx1011xxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x000000D0 /* $pattern(31:0) == xxxx000xx0x0xxxxxxxx00001101xxxx */) {
    return LoadBasedOffsetMemoryDouble_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x001000D0 /* $pattern(31:0) == xxxx000xx0x1xxxxxxxx00001101xxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x004000D0 /* $pattern(31:0) == xxxx000xx1x0xxxxxxxxxxxx1101xxxx */) {
    return LoadBasedImmedMemoryDouble_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F00F0) == 0x014F00D0 /* $pattern(31:0) == xxxx0001x1001111xxxxxxxx1101xxxx */) {
    return LoadBasedImmedMemoryDouble_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x005000D0 /* $pattern(31:0) == xxxx000xx1x1xxxxxxxxxxxx1101xxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F00F0) == 0x015F00D0 /* $pattern(31:0) == xxxx0001x1011111xxxxxxxx1101xxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x000000F0 /* $pattern(31:0) == xxxx000xx0x0xxxxxxxx00001111xxxx */) {
    return StoreBasedOffsetMemoryDouble_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */ &&
      (insn.Bits() & 0x0E500FF0) == 0x001000F0 /* $pattern(31:0) == xxxx000xx0x1xxxxxxxx00001111xxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x004000F0 /* $pattern(31:0) == xxxx000xx1x0xxxxxxxxxxxx1111xxxx */) {
    return StoreBasedImmedMemoryDouble_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E5000F0) == 0x005000F0 /* $pattern(31:0) == xxxx000xx1x1xxxxxxxxxxxx1111xxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F00F0) == 0x015F00F0 /* $pattern(31:0) == xxxx0001x1011111xxxxxxxx1111xxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: half_mult.
// Specified by: See Section A5.2.7
const ClassDecoder& Arm32DecoderState::decode_half_mult(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00600000) == 0x00000000 /* op1(22:21) == 00 */ &&
      (insn.Bits() & 0x0FF00090) == 0x01000080 /* $pattern(31:0) == xxxx00010000xxxxxxxxxxxx1xx0xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:21) == 01 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op(5:5) == 0 */ &&
      (insn.Bits() & 0x0FF000B0) == 0x01200080 /* $pattern(31:0) == xxxx00010010xxxxxxxxxxxx1x00xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:21) == 01 */ &&
      (insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x0FF000B0) == 0x012000A0 /* $pattern(31:0) == xxxx00010010xxxxxxxxxxxx1x10xxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00400000 /* op1(22:21) == 10 */ &&
      (insn.Bits() & 0x0FF00090) == 0x01400080 /* $pattern(31:0) == xxxx00010100xxxxxxxxxxxx1xx0xxxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op1(22:21) == 11 */ &&
      (insn.Bits() & 0x0FF00090) == 0x01600080 /* $pattern(31:0) == xxxx00010110xxxxxxxxxxxx1xx0xxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: load_store_word_byte.
// Specified by: See Section A5.3
const ClassDecoder& Arm32DecoderState::decode_load_store_word_byte(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00000000 /* op1(24:20) == 0x000 */ &&
      (insn.Bits() & 0x0E500000) == 0x04000000 /* $pattern(31:0) == xxxx010xx0x0xxxxxxxxxxxxxxxxxxxx */) {
    return StoreBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00100000 /* op1(24:20) == 0x001 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E500000) == 0x04100000 /* $pattern(31:0) == xxxx010xx0x1xxxxxxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00100000 /* op1(24:20) == 0x001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F0000) == 0x051F0000 /* $pattern(31:0) == xxxx0101x0011111xxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00400000 /* op1(24:20) == 0x100 */ &&
      (insn.Bits() & 0x0E500000) == 0x04400000 /* $pattern(31:0) == xxxx010xx1x0xxxxxxxxxxxxxxxxxxxx */) {
    return StoreBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E500000) == 0x04500000 /* $pattern(31:0) == xxxx010xx1x1xxxxxxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F0000) == 0x055F0000 /* $pattern(31:0) == xxxx0101x1011111xxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01000000 /* op1(24:20) == 1x0x0 */ &&
      (insn.Bits() & 0x0E500000) == 0x04000000 /* $pattern(31:0) == xxxx010xx0x0xxxxxxxxxxxxxxxxxxxx */) {
    return StoreBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E500000) == 0x04100000 /* $pattern(31:0) == xxxx010xx0x1xxxxxxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F0000) == 0x051F0000 /* $pattern(31:0) == xxxx0101x0011111xxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01400000 /* op1(24:20) == 1x1x0 */ &&
      (insn.Bits() & 0x0E500000) == 0x04400000 /* $pattern(31:0) == xxxx010xx1x0xxxxxxxxxxxxxxxxxxxx */) {
    return StoreBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0E500000) == 0x04500000 /* $pattern(31:0) == xxxx010xx1x1xxxxxxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */ &&
      (insn.Bits() & 0x0F7F0000) == 0x055F0000 /* $pattern(31:0) == xxxx0101x1011111xxxxxxxxxxxxxxxx */) {
    return LoadBasedImmedMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00000000 /* op1(24:20) == 0x000 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06000000 /* $pattern(31:0) == xxxx011xx0x0xxxxxxxxxxxxxxx0xxxx */) {
    return StoreBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00400000 /* op1(24:20) == 0x100 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06400000 /* $pattern(31:0) == xxxx011xx1x0xxxxxxxxxxxxxxx0xxxx */) {
    return StoreBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06500000 /* $pattern(31:0) == xxxx011xx1x1xxxxxxxxxxxxxxx0xxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x00700000) == 0x00100000 /* op1(24:20) == xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06100000 /* $pattern(31:0) == xxxx011xx0x1xxxxxxxxxxxxxxx0xxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01000000 /* op1(24:20) == 1x0x0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06000000 /* $pattern(31:0) == xxxx011xx0x0xxxxxxxxxxxxxxx0xxxx */) {
    return StoreBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01100000 /* op1(24:20) == 1x0x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06100000 /* $pattern(31:0) == xxxx011xx0x1xxxxxxxxxxxxxxx0xxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01400000 /* op1(24:20) == 1x1x0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06400000 /* $pattern(31:0) == xxxx011xx1x0xxxxxxxxxxxxxxx0xxxx */) {
    return StoreBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01500000) == 0x01500000 /* op1(24:20) == 1x1x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x0E500010) == 0x06500000 /* $pattern(31:0) == xxxx011xx1x1xxxxxxxxxxxxxxx0xxxx */) {
    return LoadBasedOffsetMemory_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */) {
    return Forbidden_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: media.
// Specified by: See Section A5.4
const ClassDecoder& Arm32DecoderState::decode_media(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20) == 11000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12) == ~1111 */ &&
      (insn.Bits() & 0x0FF000F0) == 0x07800010 /* $pattern(31:0) == xxxx01111000xxxxxxxxxxxx0001xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20) == 11000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12) == 1111 */ &&
      (insn.Bits() & 0x0FF0F0F0) == 0x0780F010 /* $pattern(31:0) == xxxx01111000xxxx1111xxxx0001xxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x01F00000) == 0x01F00000 /* op1(24:20) == 11111 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */) {
    return Roadblock_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(7:5) == x10 */ &&
      (insn.Bits() & 0x0FE00070) == 0x07A00050 /* $pattern(31:0) == xxxx0111101xxxxxxxxxxxxxx101xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0) == ~1111 */ &&
      (insn.Bits() & 0x0FE00070) == 0x07C00010 /* $pattern(31:0) == xxxx0111110xxxxxxxxxxxxxx001xxxx */) {
    return Defs12To15CondsDontCare_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0) == 1111 */ &&
      (insn.Bits() & 0x0FE0007F) == 0x07C0001F /* $pattern(31:0) == xxxx0111110xxxxxxxxxxxxxx0011111 */) {
    return Unary1RegisterBitRange_instance_;
  }

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(7:5) == x10 */ &&
      (insn.Bits() & 0x0FE00070) == 0x07E00050 /* $pattern(31:0) == xxxx0111111xxxxxxxxxxxxxx101xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x01C00000) == 0x00000000 /* op1(24:20) == 000xx */) {
    return decode_parallel_add_sub_signed(insn);
  }

  if ((insn.Bits() & 0x01C00000) == 0x00400000 /* op1(24:20) == 001xx */) {
    return decode_parallel_add_sub_unsigned(insn);
  }

  if ((insn.Bits() & 0x01800000) == 0x00800000 /* op1(24:20) == 01xxx */) {
    return decode_pack_sat_rev(insn);
  }

  if ((insn.Bits() & 0x01800000) == 0x01000000 /* op1(24:20) == 10xxx */) {
    return decode_signed_mult(insn);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: misc.
// Specified by: See Section A5.2.12
const ClassDecoder& Arm32DecoderState::decode_misc(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00000000 /* op1(19:16) == xx00 */ &&
      (insn.Bits() & 0x0FF3FFF0) == 0x0120F000 /* $pattern(31:0) == xxxx00010010xx00111100000000xxxx */) {
    return Unary1RegisterUse_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */ &&
      (insn.Bits() & 0x0FF3FFF0) == 0x0121F000 /* $pattern(31:0) == xxxx00010010xx01111100000000xxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */ &&
      (insn.Bits() & 0x0FF2FFF0) == 0x0122F000 /* $pattern(31:0) == xxxx00010010xx1x111100000000xxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      (insn.Bits() & 0x0FF0FFF0) == 0x0160F000 /* $pattern(31:0) == xxxx00010110xxxx111100000000xxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* op(22:21) == x0 */ &&
      (insn.Bits() & 0x0FBF0FFF) == 0x010F0000 /* $pattern(31:0) == xxxx00010x001111xxxx000000000000 */) {
    return Unary1RegisterSet_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x012FFF10 /* $pattern(31:0) == xxxx000100101111111111110001xxxx */) {
    return BxBlx_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x016F0F10 /* $pattern(31:0) == xxxx000101101111xxxx11110001xxxx */) {
    return Defs12To15RdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000020 /* op2(6:4) == 010 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x012FFF20 /* $pattern(31:0) == xxxx000100101111111111110010xxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000030 /* op2(6:4) == 011 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x012FFF30 /* $pattern(31:0) == xxxx000100101111111111110011xxxx */) {
    return BxBlx_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000050 /* op2(6:4) == 101 */) {
    return decode_sat_add_sub(insn);
  }

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FF000F0) == 0x01200070 /* $pattern(31:0) == xxxx00010010xxxxxxxxxxxx0111xxxx */) {
    return Breakpoint_instance_;
  }

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x01600070 /* $pattern(31:0) == xxxx000101100000000000000111xxxx */) {
    return Forbidden_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: misc_hints_simd.
// Specified by: See Section A5.7.1
const ClassDecoder& Arm32DecoderState::decode_misc_hints_simd(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20) == 0010000 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000000 /* op2(7:4) == 0000 */ &&
      (insn.Bits() & 0x00010000) == 0x00010000 /* Rn(19:16) == xxx1 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20) == 0010000 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:4) == xx0x */ &&
      (insn.Bits() & 0x00010000) == 0x00000000 /* Rn(19:16) == xxx0 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000010 /* op2(7:4) == 0001 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000050 /* op2(7:4) == 0101 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000D0) == 0x00000040 /* op2(7:4) == 01x0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x07700000) == 0x04100000 /* op1(26:20) == 100x001 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x07700000) == 0x04500000 /* op1(26:20) == 100x101 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x07700000) == 0x05100000 /* op1(26:20) == 101x001 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x07700000) == 0x05100000 /* op1(26:20) == 101x001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) {
    return Unpredictable_instance_;
  }

  if ((insn.Bits() & 0x07700000) == 0x05500000 /* op1(26:20) == 101x101 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x07700000) == 0x06500000 /* op1(26:20) == 110x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x07700000) == 0x07500000 /* op1(26:20) == 111x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x06700000) == 0x06100000 /* op1(26:20) == 11xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x06300000) == 0x04300000 /* op1(26:20) == 10xxx11 */) {
    return Unpredictable_instance_;
  }

  if ((insn.Bits() & 0x06300000) == 0x06300000 /* op1(26:20) == 11xxx11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */) {
    return Unpredictable_instance_;
  }

  if ((insn.Bits() & 0x07100000) == 0x04000000 /* op1(26:20) == 100xxx0 */) {
    return decode_simd_load_store(insn);
  }

  if ((insn.Bits() & 0x06000000) == 0x02000000 /* op1(26:20) == 01xxxxx */) {
    return decode_simd_dp(insn);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: msr_and_hints.
// Specified by: See Section A5.2.11
const ClassDecoder& Arm32DecoderState::decode_msr_and_hints(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000000 /* op2(7:0) == 00000000 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F000 /* $pattern(31:0) == xxxx0011001000001111000000000000 */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000001 /* op2(7:0) == 00000001 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F001 /* $pattern(31:0) == xxxx0011001000001111000000000001 */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000002 /* op2(7:0) == 00000010 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F002 /* $pattern(31:0) == xxxx0011001000001111000000000010 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000003 /* op2(7:0) == 00000011 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F003 /* $pattern(31:0) == xxxx0011001000001111000000000011 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000004 /* op2(7:0) == 00000100 */ &&
      (insn.Bits() & 0x0FFFFFFF) == 0x0320F004 /* $pattern(31:0) == xxxx0011001000001111000000000100 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000F0) == 0x000000F0 /* op2(7:0) == 1111xxxx */ &&
      (insn.Bits() & 0x0FFFFFF0) == 0x0320F0F0 /* $pattern(31:0) == xxxx001100100000111100001111xxxx */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00040000 /* op1(19:16) == 0100 */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000B0000) == 0x00080000 /* op1(19:16) == 1x00 */) {
    return DontCareInst_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */ &&
      (insn.Bits() & 0x0FF3F000) == 0x0321F000 /* $pattern(31:0) == xxxx00110010xx011111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */ &&
      (insn.Bits() & 0x0FF2F000) == 0x0322F000 /* $pattern(31:0) == xxxx00110010xx1x1111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x00400000) == 0x00400000 /* op(22:22) == 1 */ &&
      (insn.Bits() & 0x0FF0F000) == 0x0360F000 /* $pattern(31:0) == xxxx00110110xxxx1111xxxxxxxxxxxx */) {
    return Forbidden_instance_;
  }

  if (true) {
    return Forbidden_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: mult.
// Specified by: See Section A5.2.5
const ClassDecoder& Arm32DecoderState::decode_mult(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00400000 /* op(23:20) == 0100 */ &&
      (insn.Bits() & 0x0FF000F0) == 0x00400090 /* $pattern(31:0) == xxxx00000100xxxxxxxxxxxx1001xxxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00F00000) == 0x00600000 /* op(23:20) == 0110 */ &&
      (insn.Bits() & 0x0FF000F0) == 0x00600090 /* $pattern(31:0) == xxxx00000110xxxxxxxxxxxx1001xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00D00000) == 0x00500000 /* op(23:20) == 01x1 */) {
    return Undefined_instance_;
  }

  if ((insn.Bits() & 0x00E00000) == 0x00000000 /* op(23:20) == 000x */ &&
      (insn.Bits() & 0x0FE0F0F0) == 0x00000090 /* $pattern(31:0) == xxxx0000000xxxxx0000xxxx1001xxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00E00000) == 0x00200000 /* op(23:20) == 001x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00200090 /* $pattern(31:0) == xxxx0000001xxxxxxxxxxxxx1001xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00E00000) == 0x00800000 /* op(23:20) == 100x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00800090 /* $pattern(31:0) == xxxx0000100xxxxxxxxxxxxx1001xxxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00E00000) == 0x00A00000 /* op(23:20) == 101x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00A00090 /* $pattern(31:0) == xxxx0000101xxxxxxxxxxxxx1001xxxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00E00000) == 0x00C00000 /* op(23:20) == 110x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00C00090 /* $pattern(31:0) == xxxx0000110xxxxxxxxxxxxx1001xxxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00E00000) == 0x00E00000 /* op(23:20) == 111x */ &&
      (insn.Bits() & 0x0FE000F0) == 0x00E00090 /* $pattern(31:0) == xxxx0000111xxxxxxxxxxxxx1001xxxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: other_vfp_data_proc.
// Specified by: A7.5 Table A7 - 17, page 17 - 25
const ClassDecoder& Arm32DecoderState::decode_other_vfp_data_proc(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x000F0000) == 0x00000000 /* opc2(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* opc3(7:6) == 01 */ &&
      (insn.Bits() & 0x0FBF0ED0) == 0x0EB00A40 /* $pattern(31:0) == xxxx11101x110000xxxx101x01x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000F0000) == 0x00000000 /* opc2(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6) == 11 */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16) == 0001 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* opc3(7:6) == 01 */ &&
      (insn.Bits() & 0x0FBF0ED0) == 0x0EB10A40 /* $pattern(31:0) == xxxx11101x110001xxxx101x01x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16) == 0001 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6) == 11 */ &&
      (insn.Bits() & 0x0FBF0ED0) == 0x0EB10AC0 /* $pattern(31:0) == xxxx11101x110001xxxx101x11x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000F0000) == 0x00040000 /* opc2(19:16) == 0100 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBF0E50) == 0x0EB40A40 /* $pattern(31:0) == xxxx11101x110100xxxx101xx1x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000F0000) == 0x00050000 /* opc2(19:16) == 0101 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBF0E7F) == 0x0EB50A40 /* $pattern(31:0) == xxxx11101x110101xxxx101xx1000000 */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000F0000) == 0x00070000 /* opc2(19:16) == 0111 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6) == 11 */ &&
      (insn.Bits() & 0x0FBF0ED0) == 0x0EB70AC0 /* $pattern(31:0) == xxxx11101x110111xxxx101x11x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000F0000) == 0x00080000 /* opc2(19:16) == 1000 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBF0E50) == 0x0EB80A40 /* $pattern(31:0) == xxxx11101x111000xxxx101xx1x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000E0000) == 0x00020000 /* opc2(19:16) == 001x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBE0F50) == 0x0EB20A40 /* $pattern(31:0) == xxxx11101x11001xxxxx1010x1x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000E0000) == 0x000A0000 /* opc2(19:16) == 101x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBE0E50) == 0x0EBA0A40 /* $pattern(31:0) == xxxx11101x11101xxxxx101xx1x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000E0000) == 0x000C0000 /* opc2(19:16) == 110x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBE0E50) == 0x0EBC0A40 /* $pattern(31:0) == xxxx11101x11110xxxxx101xx1x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x000E0000) == 0x000E0000 /* opc2(19:16) == 111x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FBE0E50) == 0x0EBE0A40 /* $pattern(31:0) == xxxx11101x11111xxxxx101xx1x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */ &&
      (insn.Bits() & 0x0FB00EF0) == 0x0EB00A00 /* $pattern(31:0) == xxxx11101x11xxxxxxxx101x0000xxxx */) {
    return VfpOp_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: pack_sat_rev.
// Specified by: See Section A5.4.3
const ClassDecoder& Arm32DecoderState::decode_pack_sat_rev(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06800070 /* $pattern(31:0) == xxxx01101000xxxxxxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x068F0070 /* $pattern(31:0) == xxxx011010001111xxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06800FB0 /* $pattern(31:0) == xxxx01101000xxxxxxxx11111011xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */ &&
      (insn.Bits() & 0x0FF00030) == 0x06800010 /* $pattern(31:0) == xxxx01101000xxxxxxxxxxxxxx01xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00200000 /* op1(22:20) == 010 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06A00F30 /* $pattern(31:0) == xxxx01101010xxxxxxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00200000 /* op1(22:20) == 010 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06A00070 /* $pattern(31:0) == xxxx01101010xxxxxxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00200000 /* op1(22:20) == 010 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06AF0070 /* $pattern(31:0) == xxxx011010101111xxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00300000 /* op1(22:20) == 011 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x06BF0F30 /* $pattern(31:0) == xxxx011010111111xxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00300000 /* op1(22:20) == 011 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06B00070 /* $pattern(31:0) == xxxx01101011xxxxxxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00300000 /* op1(22:20) == 011 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06BF0070 /* $pattern(31:0) == xxxx011010111111xxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00300000 /* op1(22:20) == 011 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x06BF0FB0 /* $pattern(31:0) == xxxx011010111111xxxx11111011xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06C00070 /* $pattern(31:0) == xxxx01101100xxxxxxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06CF0070 /* $pattern(31:0) == xxxx011011001111xxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06E00F30 /* $pattern(31:0) == xxxx01101110xxxxxxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06E00070 /* $pattern(31:0) == xxxx01101110xxxxxxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06EF0070 /* $pattern(31:0) == xxxx011011101111xxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x06FF0F30 /* $pattern(31:0) == xxxx011011111111xxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */ &&
      (insn.Bits() & 0x0FF003F0) == 0x06F00070 /* $pattern(31:0) == xxxx01101111xxxxxxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */ &&
      (insn.Bits() & 0x0FFF03F0) == 0x06FF0070 /* $pattern(31:0) == xxxx011011111111xxxxxx000111xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */ &&
      (insn.Bits() & 0x0FFF0FF0) == 0x06FF0FB0 /* $pattern(31:0) == xxxx011011111111xxxx11111011xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:20) == 01x */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */ &&
      (insn.Bits() & 0x0FE00030) == 0x06A00010 /* $pattern(31:0) == xxxx0110101xxxxxxxxxxxxxxx01xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op1(22:20) == 11x */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */ &&
      (insn.Bits() & 0x0FE00030) == 0x06E00010 /* $pattern(31:0) == xxxx0110111xxxxxxxxxxxxxxx01xxxx */) {
    return Defs12To15CondsDontCareRdRnNotPc_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: parallel_add_sub_signed.
// Specified by: See Section A5.4.1
const ClassDecoder& Arm32DecoderState::decode_parallel_add_sub_signed(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F10 /* $pattern(31:0) == xxxx01100001xxxxxxxx11110001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F30 /* $pattern(31:0) == xxxx01100001xxxxxxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F50 /* $pattern(31:0) == xxxx01100001xxxxxxxx11110101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F70 /* $pattern(31:0) == xxxx01100001xxxxxxxx11110111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100F90 /* $pattern(31:0) == xxxx01100001xxxxxxxx11111001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06100FF0 /* $pattern(31:0) == xxxx01100001xxxxxxxx11111111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F10 /* $pattern(31:0) == xxxx01100010xxxxxxxx11110001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F30 /* $pattern(31:0) == xxxx01100010xxxxxxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F50 /* $pattern(31:0) == xxxx01100010xxxxxxxx11110101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F70 /* $pattern(31:0) == xxxx01100010xxxxxxxx11110111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200F90 /* $pattern(31:0) == xxxx01100010xxxxxxxx11111001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06200FF0 /* $pattern(31:0) == xxxx01100010xxxxxxxx11111111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F10 /* $pattern(31:0) == xxxx01100011xxxxxxxx11110001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F30 /* $pattern(31:0) == xxxx01100011xxxxxxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F50 /* $pattern(31:0) == xxxx01100011xxxxxxxx11110101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F70 /* $pattern(31:0) == xxxx01100011xxxxxxxx11110111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300F90 /* $pattern(31:0) == xxxx01100011xxxxxxxx11111001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06300FF0 /* $pattern(31:0) == xxxx01100011xxxxxxxx11111111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: parallel_add_sub_unsigned.
// Specified by: See Section A5.4.2
const ClassDecoder& Arm32DecoderState::decode_parallel_add_sub_unsigned(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F10 /* $pattern(31:0) == xxxx01100101xxxxxxxx11110001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F30 /* $pattern(31:0) == xxxx01100101xxxxxxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F50 /* $pattern(31:0) == xxxx01100101xxxxxxxx11110101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F70 /* $pattern(31:0) == xxxx01100101xxxxxxxx11110111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500F90 /* $pattern(31:0) == xxxx01100101xxxxxxxx11111001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00100000 /* op1(21:20) == 01 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06500FF0 /* $pattern(31:0) == xxxx01100101xxxxxxxx11111111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F10 /* $pattern(31:0) == xxxx01100110xxxxxxxx11110001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F30 /* $pattern(31:0) == xxxx01100110xxxxxxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F50 /* $pattern(31:0) == xxxx01100110xxxxxxxx11110101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F70 /* $pattern(31:0) == xxxx01100110xxxxxxxx11110111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600F90 /* $pattern(31:0) == xxxx01100110xxxxxxxx11111001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06600FF0 /* $pattern(31:0) == xxxx01100110xxxxxxxx11111111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F10 /* $pattern(31:0) == xxxx01100111xxxxxxxx11110001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F30 /* $pattern(31:0) == xxxx01100111xxxxxxxx11110011xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000040 /* op2(7:5) == 010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F50 /* $pattern(31:0) == xxxx01100111xxxxxxxx11110101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F70 /* $pattern(31:0) == xxxx01100111xxxxxxxx11110111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700F90 /* $pattern(31:0) == xxxx01100111xxxxxxxx11111001xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(21:20) == 11 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x06700FF0 /* $pattern(31:0) == xxxx01100111xxxxxxxx11111111xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: sat_add_sub.
// Specified by: See Section A5.2.6
const ClassDecoder& Arm32DecoderState::decode_sat_add_sub(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00600000) == 0x00000000 /* op(22:21) == 00 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01000050 /* $pattern(31:0) == xxxx00010000xxxxxxxx00000101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01200050 /* $pattern(31:0) == xxxx00010010xxxxxxxx00000101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00400000 /* op(22:21) == 10 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01400050 /* $pattern(31:0) == xxxx00010100xxxxxxxx00000101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01600050 /* $pattern(31:0) == xxxx00010110xxxxxxxx00000101xxxx */) {
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: signed_mult.
// Specified by: See Section A5.4.4
const ClassDecoder& Arm32DecoderState::decode_signed_mult(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07000010 /* $pattern(31:0) == xxxx01110000xxxxxxxxxxxx00x1xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */ &&
      (insn.Bits() & 0x0FF0F0D0) == 0x0700F010 /* $pattern(31:0) == xxxx01110000xxxx1111xxxx00x1xxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07000050 /* $pattern(31:0) == xxxx01110000xxxxxxxxxxxx01x1xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */ &&
      (insn.Bits() & 0x0FF0F0D0) == 0x0700F050 /* $pattern(31:0) == xxxx01110000xxxx1111xxxx01x1xxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07400010 /* $pattern(31:0) == xxxx01110100xxxxxxxxxxxx00x1xxxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07400050 /* $pattern(31:0) == xxxx01110100xxxxxxxxxxxx01x1xxxx */) {
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */ &&
      (insn.Bits() & 0x0FF000D0) == 0x07500010 /* $pattern(31:0) == xxxx01110101xxxxxxxxxxxx00x1xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */ &&
      (insn.Bits() & 0x0FF0F0D0) == 0x0750F010 /* $pattern(31:0) == xxxx01110101xxxx1111xxxx00x1xxxx */) {
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;
  }

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* op2(7:5) == 11x */ &&
      (insn.Bits() & 0x0FF000D0) == 0x075000D0 /* $pattern(31:0) == xxxx01110101xxxxxxxxxxxx11x1xxxx */) {
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp.
// Specified by: See Section A7.4
const ClassDecoder& Arm32DecoderState::decode_simd_dp(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x00B80000) == 0x00800000 /* A(23:19) == 1x000 */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */) {
    return decode_simd_dp_1imm(insn);
  }

  if ((insn.Bits() & 0x00B80000) == 0x00880000 /* A(23:19) == 1x001 */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */) {
    return decode_simd_dp_2shift(insn);
  }

  if ((insn.Bits() & 0x00B00000) == 0x00900000 /* A(23:19) == 1x01x */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */) {
    return decode_simd_dp_2shift(insn);
  }

  if ((insn.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19) == 1x10x */ &&
      (insn.Bits() & 0x00000050) == 0x00000000 /* C(7:4) == x0x0 */) {
    return decode_simd_dp_3diff(insn);
  }

  if ((insn.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19) == 1x10x */ &&
      (insn.Bits() & 0x00000050) == 0x00000040 /* C(7:4) == x1x0 */) {
    return decode_simd_dp_2scalar(insn);
  }

  if ((insn.Bits() & 0x00A00000) == 0x00800000 /* A(23:19) == 1x0xx */ &&
      (insn.Bits() & 0x00000050) == 0x00000000 /* C(7:4) == x0x0 */) {
    return decode_simd_dp_3diff(insn);
  }

  if ((insn.Bits() & 0x00A00000) == 0x00800000 /* A(23:19) == 1x0xx */ &&
      (insn.Bits() & 0x00000050) == 0x00000040 /* C(7:4) == x1x0 */) {
    return decode_simd_dp_2scalar(insn);
  }

  if ((insn.Bits() & 0x00A00000) == 0x00A00000 /* A(23:19) == 1x1xx */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */) {
    return decode_simd_dp_2shift(insn);
  }

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:19) == 0xxxx */) {
    return decode_simd_dp_3same(insn);
  }

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:19) == 1xxxx */ &&
      (insn.Bits() & 0x00000090) == 0x00000090 /* C(7:4) == 1xx1 */) {
    return decode_simd_dp_2shift(insn);
  }

  if ((insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000F00) == 0x00000C00 /* B(11:8) == 1100 */ &&
      (insn.Bits() & 0x00000090) == 0x00000000 /* C(7:4) == 0xx0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* B(11:8) == 10xx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* B(11:8) == 0xxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */) {
    return decode_simd_dp_2misc(insn);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_1imm.
// Specified by: See Section A7.4.6
const ClassDecoder& Arm32DecoderState::decode_simd_dp_1imm(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000020) == 0x00000000 /* op(5:5) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000E00 /* cmode(11:8) == 1110 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000F00 /* cmode(11:8) == 1111 */) {
    return Undefined_instance_;
  }

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000C00 /* cmode(11:8) == 110x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* cmode(11:8) == 10xx */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* cmode(11:8) == 0xxx */) {
    return EffectiveNoOp_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_2misc.
// Specified by: See Section A7.4.5
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2misc(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000780) == 0x00000700 /* B(10:6) == 1110x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000380) == 0x00000100 /* B(10:6) == x010x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000580) == 0x00000580 /* B(10:6) == 1x11x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000100) == 0x00000000 /* B(10:6) == xx0xx */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000380) == 0x00000380 /* B(10:6) == x111x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000280) == 0x00000200 /* B(10:6) == x1x0x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000200) == 0x00000000 /* B(10:6) == x0xxx */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x000007C0) == 0x00000300 /* B(10:6) == 01100 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x000006C0) == 0x00000600 /* B(10:6) == 11x00 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(10:6) == 010xx */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(10:6) == 00xxx */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00030000) == 0x00030000 /* A(17:16) == 11 */ &&
      (insn.Bits() & 0x00000400) == 0x00000400 /* B(10:6) == 1xxxx */) {
    return EffectiveNoOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_2scalar.
// Specified by: See Section A7.4.3
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2scalar(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8) == 1010 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8) == 1011 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000B00) == 0x00000200 /* A(11:8) == 0x10 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000B00) == 0x00000300 /* A(11:8) == 0x11 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000200) == 0x00000000 /* A(11:8) == xx0x */) {
    return EffectiveNoOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_2shift.
// Specified by: See Section A7.4.4
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2shift(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000500 /* A(11:8) == 0101 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* B(6:6) == 1 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* L(7:7) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000900 /* A(11:8) == 1001 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8) == 1010 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000E00) == 0x00000400 /* A(11:8) == 010x */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000600) == 0x00000600 /* A(11:8) == x11x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000C00) == 0x00000000 /* A(11:8) == 00xx */) {
    return EffectiveNoOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_3diff.
// Specified by: See Section A7.4.2
const ClassDecoder& Arm32DecoderState::decode_simd_dp_3diff(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000D00) == 0x00000900 /* A(11:8) == 10x1 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000900) == 0x00000800 /* A(11:8) == 1xx0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000800) == 0x00000000 /* A(11:8) == 0xxx */) {
    return EffectiveNoOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_3same.
// Specified by: See Section A7.4.1
const ClassDecoder& Arm32DecoderState::decode_simd_dp_3same(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000100 /* A(11:8) == 0001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000500 /* A(11:8) == 0101 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000900 /* A(11:8) == 1001 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8) == 1011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* C(21:20) == 0x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* C(21:20) == 0x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00200000) == 0x00200000 /* C(21:20) == 1x */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000700) == 0x00000700 /* A(11:8) == x111 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000B00) == 0x00000300 /* A(11:8) == 0x11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000B00) == 0x00000B00 /* A(11:8) == 1x11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000D00) == 0x00000100 /* A(11:8) == 00x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000D00) == 0x00000800 /* A(11:8) == 10x0 */) {
    return EffectiveNoOp_instance_;
  }

  if ((insn.Bits() & 0x00000900) == 0x00000000 /* A(11:8) == 0xx0 */) {
    return EffectiveNoOp_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_load_store.
// Specified by: See Section A7.7
const ClassDecoder& Arm32DecoderState::decode_simd_load_store(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x00200000) == 0x00000000 /* L(21:21) == 0 */) {
    return decode_simd_load_store_l0(insn);
  }

  if ((insn.Bits() & 0x00200000) == 0x00200000 /* L(21:21) == 1 */) {
    return decode_simd_load_store_l1(insn);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_load_store_l0.
// Specified by: See Section A7.7, Table A7 - 20
const ClassDecoder& Arm32DecoderState::decode_simd_load_store_l0(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000300 /* B(11:8) == 0011 */) {
    return VectorStore_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(11:8) == x010 */) {
    return VectorStore_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(11:8) == x00x */) {
    return VectorStore_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000400 /* B(11:8) == 01xx */) {
    return VectorStore_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* B(11:8) == 10xx */) {
    return VectorStore_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* B(11:8) == 0xxx */) {
    return VectorStore_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_load_store_l1.
// Specified by: See Section A7.7, Table A7 - 21
const ClassDecoder& Arm32DecoderState::decode_simd_load_store_l1(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000300 /* B(11:8) == 0011 */) {
    return VectorLoad_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(11:8) == x010 */) {
    return VectorLoad_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(11:8) == x00x */) {
    return VectorLoad_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000400 /* B(11:8) == 01xx */) {
    return VectorLoad_instance_;
  }

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */) {
    return VectorLoad_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: super_cop.
// Specified by: See Section A5.6
const ClassDecoder& Arm32DecoderState::decode_super_cop(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x03E00000) == 0x00000000 /* op1(25:20) == 00000x */) {
    return Undefined_instance_;
  }

  if ((insn.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20) == 00010x */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20) == 00010x */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */) {
    return decode_ext_reg_transfers(insn);
  }

  if ((insn.Bits() & 0x03100000) == 0x02100000 /* op1(25:20) == 10xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02100000) == 0x00000000 /* op1(25:20) == 0xxxx0 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02100000) == 0x00100000 /* op1(25:20) == 0xxxx1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02100000) == 0x00100000 /* op1(25:20) == 0xxxx1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */) {
    return decode_vfp_data_proc(insn);
  }

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */) {
    return decode_ext_reg_move(insn);
  }

  if ((insn.Bits() & 0x03000000) == 0x03000000 /* op1(25:20) == 11xxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op1(25:20) == 0xxxxx */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */) {
    return decode_ext_reg_load_store(insn);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: sync.
// Specified by: See Section A5.2.10
const ClassDecoder& Arm32DecoderState::decode_sync(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00800000 /* op(23:20) == 1000 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01800F90 /* $pattern(31:0) == xxxx00011000xxxxxxxx11111001xxxx */) {
    return StoreBasedMemoryRtBits0To3_instance_;
  }

  if ((insn.Bits() & 0x00F00000) == 0x00900000 /* op(23:20) == 1001 */ &&
      (insn.Bits() & 0x0FF00FFF) == 0x01900F9F /* $pattern(31:0) == xxxx00011001xxxxxxxx111110011111 */) {
    return LoadBasedMemory_instance_;
  }

  if ((insn.Bits() & 0x00F00000) == 0x00A00000 /* op(23:20) == 1010 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01A00F90 /* $pattern(31:0) == xxxx00011010xxxxxxxx11111001xxxx */) {
    return StoreBasedMemoryDoubleRtBits0To3_instance_;
  }

  if ((insn.Bits() & 0x00F00000) == 0x00B00000 /* op(23:20) == 1011 */ &&
      (insn.Bits() & 0x0FF00FFF) == 0x01B00F9F /* $pattern(31:0) == xxxx00011011xxxxxxxx111110011111 */) {
    return LoadBasedMemoryDouble_instance_;
  }

  if ((insn.Bits() & 0x00F00000) == 0x00C00000 /* op(23:20) == 1100 */ &&
      (insn.Bits() & 0x0FF00FF0) == 0x01C00F90 /* $pattern(31:0) == xxxx00011100xxxxxxxx11111001xxxx */) {
    return StoreBasedMemoryRtBits0To3_instance_;
  }

  if ((insn.Bits() & 0x00F00000) == 0x00D00000 /* op(23:20) == 1101 */ &&
      (insn.Bits() & 0x0FF00FFF) == 0x01D00F9F /* $pattern(31:0) == xxxx00011101xxxxxxxx111110011111 */) {
    return LoadBasedMemory_instance_;
  }

  if ((insn.Bits() & 0x00F00000) == 0x00E00000 /* op(23:20) == 1110 */) {
    return StoreBasedMemoryRtBits0To3_instance_;
  }

  if ((insn.Bits() & 0x00F00000) == 0x00F00000 /* op(23:20) == 1111 */ &&
      (insn.Bits() & 0x0FF00FFF) == 0x01F00F9F /* $pattern(31:0) == xxxx00011111xxxxxxxx111110011111 */) {
    return LoadBasedMemory_instance_;
  }

  if ((insn.Bits() & 0x00B00000) == 0x00000000 /* op(23:20) == 0x00 */) {
    return Deprecated_instance_;
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: unconditional.
// Specified by: See Section A5.7
const ClassDecoder& Arm32DecoderState::decode_unconditional(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x0FF00000) == 0x0C400000 /* op1(27:20) == 11000100 */) {
    return CoprocessorOp_instance_;
  }

  if ((insn.Bits() & 0x0FF00000) == 0x0C500000 /* op1(27:20) == 11000101 */) {
    return MoveDoubleFromCoprocessor_instance_;
  }

  if ((insn.Bits() & 0x0FB00000) == 0x0C200000 /* op1(27:20) == 11000x10 */) {
    return StoreCoprocessor_instance_;
  }

  if ((insn.Bits() & 0x0FB00000) == 0x0C300000 /* op1(27:20) == 11000x11 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) {
    return LoadCoprocessor_instance_;
  }

  if ((insn.Bits() & 0x0F900000) == 0x0C800000 /* op1(27:20) == 11001xx0 */) {
    return StoreCoprocessor_instance_;
  }

  if ((insn.Bits() & 0x0F900000) == 0x0C900000 /* op1(27:20) == 11001xx1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) {
    return LoadCoprocessor_instance_;
  }

  if ((insn.Bits() & 0x0E500000) == 0x08100000 /* op1(27:20) == 100xx0x1 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x0E500000) == 0x08400000 /* op1(27:20) == 100xx1x0 */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x0F100000) == 0x0D000000 /* op1(27:20) == 1101xxx0 */) {
    return StoreCoprocessor_instance_;
  }

  if ((insn.Bits() & 0x0F100000) == 0x0D100000 /* op1(27:20) == 1101xxx1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) {
    return LoadCoprocessor_instance_;
  }

  if ((insn.Bits() & 0x0F100000) == 0x0E000000 /* op1(27:20) == 1110xxx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */) {
    return CoprocessorOp_instance_;
  }

  if ((insn.Bits() & 0x0F100000) == 0x0E100000 /* op1(27:20) == 1110xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */) {
    return MoveFromCoprocessor_instance_;
  }

  if ((insn.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20) == 1110xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */) {
    return CoprocessorOp_instance_;
  }

  if ((insn.Bits() & 0x0E000000) == 0x0A000000 /* op1(27:20) == 101xxxxx */) {
    return Forbidden_instance_;
  }

  if ((insn.Bits() & 0x08000000) == 0x00000000 /* op1(27:20) == 0xxxxxxx */) {
    return decode_misc_hints_simd(insn);
  }

  if (true) {
    return Undefined_instance_;
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: vfp_data_proc.
// Specified by: A7.5 Table A7 - 16, page A7 - 24
const ClassDecoder& Arm32DecoderState::decode_vfp_data_proc(
     const Instruction insn) const
{
  if ((insn.Bits() & 0x00B00000) == 0x00000000 /* opc1(23:20) == 0x00 */ &&
      (insn.Bits() & 0x0FB00E10) == 0x0E000A00 /* $pattern(31:0) == xxxx11100x00xxxxxxxx101xxxx0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x00B00000) == 0x00100000 /* opc1(23:20) == 0x01 */ &&
      (insn.Bits() & 0x0FB00E10) == 0x0E100A00 /* $pattern(31:0) == xxxx11100x01xxxxxxxx101xxxx0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x00B00000) == 0x00200000 /* opc1(23:20) == 0x10 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E200A00 /* $pattern(31:0) == xxxx11100x10xxxxxxxx101xx0x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x00B00000) == 0x00200000 /* opc1(23:20) == 0x10 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E200A40 /* $pattern(31:0) == xxxx11100x10xxxxxxxx101xx1x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20) == 0x11 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E300A00 /* $pattern(31:0) == xxxx11100x11xxxxxxxx101xx0x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x00B00000) == 0x00300000 /* opc1(23:20) == 0x11 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E300A40 /* $pattern(31:0) == xxxx11100x11xxxxxxxx101xx1x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x00B00000) == 0x00800000 /* opc1(23:20) == 1x00 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */ &&
      (insn.Bits() & 0x0FB00E50) == 0x0E800A00 /* $pattern(31:0) == xxxx11101x00xxxxxxxx101xx0x0xxxx */) {
    return VfpOp_instance_;
  }

  if ((insn.Bits() & 0x00B00000) == 0x00B00000 /* opc1(23:20) == 1x11 */) {
    return decode_other_vfp_data_proc(insn);
  }

  // Catch any attempt to fall though ...
  return not_implemented_;
}

const ClassDecoder& Arm32DecoderState::decode(const Instruction insn) const {
  return decode_ARMv7(insn);
}

}  // namespace nacl_arm_dec
