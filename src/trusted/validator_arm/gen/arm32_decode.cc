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
  , MaskAddress_instance_()
  , MoveDoubleFromCoprocessor_instance_()
  , MoveFromCoprocessor_instance_()
  , PackSatRev_instance_()
  , Roadblock_instance_()
  , StoreBasedImmedMemory_instance_()
  , StoreBasedImmedMemoryDouble_instance_()
  , StoreBasedMemoryDoubleRtBits0To3_instance_()
  , StoreBasedMemoryRtBits0To3_instance_()
  , StoreBasedOffsetMemory_instance_()
  , StoreBasedOffsetMemoryDouble_instance_()
  , StoreCoprocessor_instance_()
  , StoreRegisterList_instance_()
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
      (insn.Bits() & 0x0E000000) == 0x04000000 /* op1(27:25) == 010 */)
    return decode_load_store_word_byte(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25) == 011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */)
    return decode_load_store_word_byte(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x06000000 /* op1(27:25) == 011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return decode_media(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x00000000 /* op1(27:25) == 00x */)
    return decode_dp_misc(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x08000000 /* op1(27:25) == 10x */)
    return decode_branch_block_xfer(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x0C000000 /* op1(27:25) == 11x */)
    return decode_super_cop(insn);

  if ((insn.Bits() & 0xF0000000) == 0xF0000000 /* cond(31:28) == 1111 */)
    return decode_unconditional(insn);

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: branch_block_xfer.
// Specified by: See Section A5.5
const ClassDecoder& Arm32DecoderState::decode_branch_block_xfer(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x02500000) == 0x00000000 /* op(25:20) == 0xx0x0 */)
    return StoreRegisterList_instance_;

  if ((insn.Bits() & 0x02500000) == 0x00100000 /* op(25:20) == 0xx0x1 */)
    return LoadMultiple_instance_;

  if ((insn.Bits() & 0x02400000) == 0x00400000 /* op(25:20) == 0xx1xx */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:20) == 1xxxxx */)
    return Branch_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: dp_immed.
// Specified by: See Section A5.2.3
const ClassDecoder& Arm32DecoderState::decode_dp_immed(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op(24:20) == 10001 */)
    return TestIfAddressMasked_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op(24:20) == 10101 */)
    return DontCareInst_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01300000 /* op(24:20) == 10x11 */)
    return DontCareInst_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op(24:20) == 0010x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op(24:20) == 0100x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op(24:20) == 0110x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op(24:20) == 1100x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op(24:20) == 1110x */)
    return MaskAddress_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op(24:20) == 1111x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00A00000 /* op(24:20) == x101x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01600000) == 0x00600000 /* op(24:20) == 0x11x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01C00000) == 0x00000000 /* op(24:20) == 000xx */)
    return Defs12To15_instance_;

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
      (insn.Bits() & 0x00000090) == 0x00000010 /* op2(7:4) == 0xx1 */)
    return decode_dp_reg_shifted(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return decode_dp_reg(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) == 0x01000000 /* op1(24:20) == 10xx0 */ &&
      (insn.Bits() & 0x00000090) == 0x00000080 /* op2(7:4) == 1xx0 */)
    return decode_half_mult(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01900000) == 0x01000000 /* op1(24:20) == 10xx0 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:4) == 0xxx */)
    return decode_misc(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) != 0x00200000 /* op1(24:20) == ~0xx1x */ &&
      (insn.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4) == 1011 */)
    return decode_extra_load_store(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) != 0x00200000 /* op1(24:20) == ~0xx1x */ &&
      (insn.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4) == 11x1 */)
    return decode_extra_load_store(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x000000F0) == 0x000000B0 /* op2(7:4) == 1011 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x000000D0) == 0x000000D0 /* op2(7:4) == 11x1 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* op1(24:20) == 0xxxx */ &&
      (insn.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4) == 1001 */)
    return decode_mult(insn);

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op(25:25) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* op1(24:20) == 1xxxx */ &&
      (insn.Bits() & 0x000000F0) == 0x00000090 /* op2(7:4) == 1001 */)
    return decode_sync(insn);

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01F00000) == 0x01000000 /* op1(24:20) == 10000 */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01F00000) == 0x01400000 /* op1(24:20) == 10100 */)
    return DataProc_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01B00000) == 0x01200000 /* op1(24:20) == 10x10 */)
    return decode_msr_and_hints(insn);

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */)
    return decode_dp_immed(insn);

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: dp_reg.
// Specified by: See Section A5.2.1
const ClassDecoder& Arm32DecoderState::decode_dp_reg(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20) == 0001x */)
    return Defs12To15CondsDontCare_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20) == 1100x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000020 /* op3(6:5) == 01 */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op3(6:5) == 10 */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00C00000 /* op1(24:20) == x110x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01600000) == 0x00600000 /* op1(24:20) == 0x11x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01900000) == 0x01100000 /* op1(24:20) == 10xx1 */)
    return DontCareInst_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x00000000 /* op1(24:20) == 00x0x */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01C00000) == 0x00800000 /* op1(24:20) == 010xx */)
    return Defs12To15_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: dp_reg_shifted.
// Specified by: See Section A5.2.2
const ClassDecoder& Arm32DecoderState::decode_dp_reg_shifted(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01E00000) == 0x00200000 /* op1(24:20) == 0001x */)
    return Defs12To15CondsDontCareRdRnRsRmNotPc_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op1(24:20) == 1100x */)
    return Defs12To15RdRnRsRmNotPc_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00C00000 /* op1(24:20) == x110x */)
    return Defs12To15RdRnRsRmNotPc_instance_;

  if ((insn.Bits() & 0x01600000) == 0x00600000 /* op1(24:20) == 0x11x */)
    return Defs12To15RdRnRsRmNotPc_instance_;

  if ((insn.Bits() & 0x01900000) == 0x01100000 /* op1(24:20) == 10xx1 */)
    return DontCareInstRnRsRmNotPc_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x00000000 /* op1(24:20) == 00x0x */)
    return Defs12To15RdRnRsRmNotPc_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x01A00000 /* op1(24:20) == 11x1x */)
    return Defs12To15RdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x01C00000) == 0x00800000 /* op1(24:20) == 010xx */)
    return Defs12To15RdRnRsRmNotPc_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: ext_reg_load_store.
// Specified by: A7.6
const ClassDecoder& Arm32DecoderState::decode_ext_reg_load_store(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x01B00000) == 0x00900000 /* opcode(24:20) == 01x01 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x00B00000 /* opcode(24:20) == 01x11 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01200000 /* opcode(24:20) == 10x10 */)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01300000 /* opcode(24:20) == 10x11 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* opcode(24:20) == 0010x */)
    return decode_ext_reg_transfers(insn);

  if ((insn.Bits() & 0x01300000) == 0x01000000 /* opcode(24:20) == 1xx00 */)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x01300000) == 0x01100000 /* opcode(24:20) == 1xx01 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x01900000) == 0x00800000 /* opcode(24:20) == 01xx0 */)
    return StoreCoprocessor_instance_;

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
      (insn.Bits() & 0x00E00000) == 0x00000000 /* A(23:21) == 000 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x00000100) == 0x00000000 /* C(8:8) == 0 */ &&
      (insn.Bits() & 0x00E00000) == 0x00E00000 /* A(23:21) == 111 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x00800000) == 0x00000000 /* A(23:21) == 0xx */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* L(20:20) == 0 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */ &&
      (insn.Bits() & 0x00800000) == 0x00800000 /* A(23:21) == 1xx */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:5) == 0x */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* L(20:20) == 1 */ &&
      (insn.Bits() & 0x00000100) == 0x00000100 /* C(8:8) == 1 */)
    return CoprocessorOp_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: ext_reg_transfers.
// Specified by: A7.8 page A7 - 32
const ClassDecoder& Arm32DecoderState::decode_ext_reg_transfers(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x000000D0) == 0x00000010 /* op(7:4) == 00x1 */)
    return MoveDoubleFromCoprocessor_instance_;

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
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return StoreBasedOffsetMemory_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */)
    return StoreBasedImmedMemory_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return LoadBasedOffsetMemoryDouble_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */)
    return LoadBasedOffsetMemory_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */)
    return LoadBasedImmedMemoryDouble_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */)
    return LoadBasedImmedMemory_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return StoreBasedOffsetMemoryDouble_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */)
    return StoreBasedImmedMemoryDouble_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op2(6:5) == x1 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */)
    return LoadBasedOffsetMemory_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op2(6:5) == x1 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */)
    return LoadBasedImmedMemory_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: half_mult.
// Specified by: See Section A5.2.7
const ClassDecoder& Arm32DecoderState::decode_half_mult(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00600000) == 0x00000000 /* op1(22:21) == 00 */)
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:21) == 01 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op(5:5) == 0 */)
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:21) == 01 */ &&
      (insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */)
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00400000 /* op1(22:21) == 10 */)
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op1(22:21) == 11 */)
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;

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
      (insn.Bits() & 0x01300000) == 0x00000000 /* op1(24:20) == 0xx00 */)
    return StoreBasedImmedMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01300000) == 0x00100000 /* op1(24:20) == 0xx01 */)
    return LoadBasedImmedMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01100000) == 0x01000000 /* op1(24:20) == 1xxx0 */)
    return StoreBasedImmedMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01100000) == 0x01100000 /* op1(24:20) == 1xxx1 */)
    return LoadBasedImmedMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return LoadBasedOffsetMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x00700000) == 0x00100000 /* op1(24:20) == xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return LoadBasedOffsetMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01300000) == 0x00000000 /* op1(24:20) == 0xx00 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return StoreBasedOffsetMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01100000) == 0x01000000 /* op1(24:20) == 1xxx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return StoreBasedOffsetMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01100000) == 0x01100000 /* op1(24:20) == 1xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return LoadBasedOffsetMemory_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Forbidden_instance_;

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
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12) == ~1111 */)
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20) == 11000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12) == 1111 */)
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01F00000 /* op1(24:20) == 11111 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Roadblock_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0) == ~1111 */)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0) == 1111 */)
    return Unary1RegisterBitRange_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x01A00000 /* op1(24:20) == 11x1x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(7:5) == x10 */)
    return Defs12To15RdRnNotPc_instance_;

  if ((insn.Bits() & 0x01C00000) == 0x00000000 /* op1(24:20) == 000xx */)
    return decode_parallel_add_sub_signed(insn);

  if ((insn.Bits() & 0x01C00000) == 0x00400000 /* op1(24:20) == 001xx */)
    return decode_parallel_add_sub_unsigned(insn);

  if ((insn.Bits() & 0x01800000) == 0x00800000 /* op1(24:20) == 01xxx */)
    return decode_pack_sat_rev(insn);

  if ((insn.Bits() & 0x01800000) == 0x01000000 /* op1(24:20) == 10xxx */)
    return decode_signed_mult(insn);

  if (true)
    return Undefined_instance_;

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
      (insn.Bits() & 0x00030000) == 0x00000000 /* op1(19:16) == xx00 */)
    return Unary1RegisterUse_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* op(22:21) == x0 */)
    return Unary1RegisterSet_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */)
    return BxBlx_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */)
    return Defs12To15RdRnNotPc_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000020 /* op2(6:4) == 010 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000030 /* op2(6:4) == 011 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */)
    return BxBlx_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000050 /* op2(6:4) == 101 */)
    return decode_sat_add_sub(insn);

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */)
    return Breakpoint_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */)
    return Forbidden_instance_;

  if (true)
    return Undefined_instance_;

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
      (insn.Bits() & 0x00010000) == 0x00010000 /* Rn(19:16) == xxx1 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x01000000 /* op1(26:20) == 0010000 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:4) == xx0x */ &&
      (insn.Bits() & 0x00010000) == 0x00000000 /* Rn(19:16) == xxx0 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000010 /* op2(7:4) == 0001 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000050 /* op2(7:4) == 0101 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000D0) == 0x00000040 /* op2(7:4) == 01x0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x04100000 /* op1(26:20) == 100x001 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x04500000 /* op1(26:20) == 100x101 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05100000 /* op1(26:20) == 101x001 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05100000 /* op1(26:20) == 101x001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Unpredictable_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05500000 /* op1(26:20) == 101x101 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x06500000 /* op1(26:20) == 110x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x07500000 /* op1(26:20) == 111x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x06700000) == 0x06100000 /* op1(26:20) == 11xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x06300000) == 0x04300000 /* op1(26:20) == 10xxx11 */)
    return Unpredictable_instance_;

  if ((insn.Bits() & 0x06300000) == 0x06300000 /* op1(26:20) == 11xxx11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */)
    return Unpredictable_instance_;

  if ((insn.Bits() & 0x07100000) == 0x04000000 /* op1(26:20) == 100xxx0 */)
    return decode_simd_load_store(insn);

  if ((insn.Bits() & 0x06000000) == 0x02000000 /* op1(26:20) == 01xxxxx */)
    return decode_simd_dp(insn);

  if (true)
    return Undefined_instance_;

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
      (insn.Bits() & 0x000000FF) == 0x00000000 /* op2(7:0) == 00000000 */)
    return DontCareInst_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000001 /* op2(7:0) == 00000001 */)
    return DontCareInst_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000004 /* op2(7:0) == 00000100 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FE) == 0x00000002 /* op2(7:0) == 0000001x */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000F0) == 0x000000F0 /* op2(7:0) == 1111xxxx */)
    return DontCareInst_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00040000 /* op1(19:16) == 0100 */)
    return DontCareInst_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000B0000) == 0x00080000 /* op1(19:16) == 1x00 */)
    return DontCareInst_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00400000 /* op(22:22) == 1 */)
    return Forbidden_instance_;

  if (true)
    return Forbidden_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: mult.
// Specified by: See Section A5.2.5
const ClassDecoder& Arm32DecoderState::decode_mult(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00400000 /* op(23:20) == 0100 */)
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00600000 /* op(23:20) == 0110 */)
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00D00000) == 0x00500000 /* op(23:20) == 01x1 */)
    return Undefined_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00000000 /* op(23:20) == 000x */)
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00E00000) == 0x00200000 /* op(23:20) == 001x */)
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* op(23:20) == 1xxx */)
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: other_vfp_data_proc.
// Specified by: A7.5 Table A7 - 17, page 17 - 25
const ClassDecoder& Arm32DecoderState::decode_other_vfp_data_proc(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x000F0000) == 0x00010000 /* opc2(19:16) == 0001 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x000F0000) == 0x00070000 /* opc2(19:16) == 0111 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* opc3(7:6) == 11 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x00070000) == 0x00000000 /* opc2(19:16) == x000 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x000E0000) == 0x000E0000 /* opc2(19:16) == 111x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x00060000) == 0x00020000 /* opc2(19:16) == x01x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x00060000) == 0x00040000 /* opc2(19:16) == x10x */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* opc3(7:6) == x1 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */)
    return CoprocessorOp_instance_;

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
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00700000 /* op1(22:20) == 111 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(22:20) == x11 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00300000 /* op1(22:20) == x11 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000A0 /* op2(7:5) == 101 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:20) == 01x */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00000000 /* op1(22:20) == xx0 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00200000) == 0x00200000 /* op1(22:20) == x1x */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */)
    return PackSatRev_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: parallel_add_sub_signed.
// Specified by: See Section A5.4.1
const ClassDecoder& Arm32DecoderState::decode_parallel_add_sub_signed(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: parallel_add_sub_unsigned.
// Specified by: See Section A5.4.2
const ClassDecoder& Arm32DecoderState::decode_parallel_add_sub_unsigned(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: sat_add_sub.
// Specified by: See Section A5.2.6
const ClassDecoder& Arm32DecoderState::decode_sat_add_sub(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if (true)
    return Defs12To15CondsDontCareRnRdRmNotPc_instance_;

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
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */)
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */)
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return Defs12To19CondsDontCareRdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */)
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      (insn.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */)
    return Defs16To19CondsDontCareRdRmRnNotPc_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* op2(7:5) == 11x */)
    return Defs16To19CondsDontCareRdRaRmRnNotPc_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp.
// Specified by: See Section A7.4
const ClassDecoder& Arm32DecoderState::decode_simd_dp(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x00B80000) == 0x00800000 /* A(23:19) == 1x000 */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_1imm(insn);

  if ((insn.Bits() & 0x00B80000) == 0x00880000 /* A(23:19) == 1x001 */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if ((insn.Bits() & 0x00B00000) == 0x00900000 /* A(23:19) == 1x01x */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if ((insn.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19) == 1x10x */ &&
      (insn.Bits() & 0x00000050) == 0x00000000 /* C(7:4) == x0x0 */)
    return decode_simd_dp_3diff(insn);

  if ((insn.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19) == 1x10x */ &&
      (insn.Bits() & 0x00000050) == 0x00000040 /* C(7:4) == x1x0 */)
    return decode_simd_dp_2scalar(insn);

  if ((insn.Bits() & 0x00A00000) == 0x00800000 /* A(23:19) == 1x0xx */ &&
      (insn.Bits() & 0x00000050) == 0x00000000 /* C(7:4) == x0x0 */)
    return decode_simd_dp_3diff(insn);

  if ((insn.Bits() & 0x00A00000) == 0x00800000 /* A(23:19) == 1x0xx */ &&
      (insn.Bits() & 0x00000050) == 0x00000040 /* C(7:4) == x1x0 */)
    return decode_simd_dp_2scalar(insn);

  if ((insn.Bits() & 0x00A00000) == 0x00A00000 /* A(23:19) == 1x1xx */ &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:19) == 0xxxx */)
    return decode_simd_dp_3same(insn);

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:19) == 1xxxx */ &&
      (insn.Bits() & 0x00000090) == 0x00000090 /* C(7:4) == 1xx1 */)
    return decode_simd_dp_2shift(insn);

  if ((insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000F00) == 0x00000C00 /* B(11:8) == 1100 */ &&
      (insn.Bits() & 0x00000090) == 0x00000000 /* C(7:4) == 0xx0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* B(11:8) == 10xx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* B(11:8) == 0xxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* C(7:4) == xxx0 */)
    return decode_simd_dp_2misc(insn);

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_1imm.
// Specified by: See Section A7.4.6
const ClassDecoder& Arm32DecoderState::decode_simd_dp_1imm(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000020) == 0x00000000 /* op(5:5) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000E00 /* cmode(11:8) == 1110 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000F00) == 0x00000F00 /* cmode(11:8) == 1111 */)
    return Undefined_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000C00 /* cmode(11:8) == 110x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* cmode(11:8) == 10xx */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op(5:5) == 1 */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* cmode(11:8) == 0xxx */)
    return EffectiveNoOp_instance_;

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
      (insn.Bits() & 0x00000780) == 0x00000700 /* B(10:6) == 1110x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000380) == 0x00000100 /* B(10:6) == x010x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000580) == 0x00000580 /* B(10:6) == 1x11x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00000000 /* A(17:16) == 00 */ &&
      (insn.Bits() & 0x00000100) == 0x00000000 /* B(10:6) == xx0xx */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000380) == 0x00000380 /* B(10:6) == x111x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000280) == 0x00000200 /* B(10:6) == x1x0x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00010000 /* A(17:16) == 01 */ &&
      (insn.Bits() & 0x00000200) == 0x00000000 /* B(10:6) == x0xxx */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x000007C0) == 0x00000300 /* B(10:6) == 01100 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x000006C0) == 0x00000600 /* B(10:6) == 11x00 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(10:6) == 010xx */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00020000 /* A(17:16) == 10 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(10:6) == 00xxx */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00030000) == 0x00030000 /* A(17:16) == 11 */ &&
      (insn.Bits() & 0x00000400) == 0x00000400 /* B(10:6) == 1xxxx */)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_dp_2scalar.
// Specified by: See Section A7.4.3
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2scalar(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8) == 1010 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8) == 1011 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000200 /* A(11:8) == 0x10 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000300 /* A(11:8) == 0x11 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000200) == 0x00000000 /* A(11:8) == xx0x */)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

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
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* B(6:6) == 1 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* L(7:7) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000900 /* A(11:8) == 1001 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8) == 1010 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000E00) == 0x00000400 /* A(11:8) == 010x */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000600) == 0x00000600 /* A(11:8) == x11x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000C00) == 0x00000000 /* A(11:8) == 00xx */)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

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
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000D00) == 0x00000900 /* A(11:8) == 10x1 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000900) == 0x00000800 /* A(11:8) == 1xx0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000800) == 0x00000000 /* A(11:8) == 0xxx */)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

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
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000500 /* A(11:8) == 0101 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000900 /* A(11:8) == 1001 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8) == 1011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* C(21:20) == 0x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* C(21:20) == 0x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00200000) == 0x00200000 /* C(21:20) == 1x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000700) == 0x00000700 /* A(11:8) == x111 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000300 /* A(11:8) == 0x11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000B00 /* A(11:8) == 1x11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000D00) == 0x00000100 /* A(11:8) == 00x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000D00) == 0x00000800 /* A(11:8) == 10x0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000900) == 0x00000000 /* A(11:8) == 0xx0 */)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: simd_load_store.
// Specified by: See Section A7.7
const ClassDecoder& Arm32DecoderState::decode_simd_load_store(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x00200000) == 0x00000000 /* L(21:21) == 0 */)
    return decode_simd_load_store_l0(insn);

  if ((insn.Bits() & 0x00200000) == 0x00200000 /* L(21:21) == 1 */)
    return decode_simd_load_store_l1(insn);

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
      (insn.Bits() & 0x00000F00) == 0x00000300 /* B(11:8) == 0011 */)
    return VectorStore_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(11:8) == x010 */)
    return VectorStore_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(11:8) == x00x */)
    return VectorStore_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000400 /* B(11:8) == 01xx */)
    return VectorStore_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000800 /* B(11:8) == 10xx */)
    return VectorStore_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */ &&
      (insn.Bits() & 0x00000800) == 0x00000000 /* B(11:8) == 0xxx */)
    return VectorStore_instance_;

  if (true)
    return Undefined_instance_;

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
      (insn.Bits() & 0x00000F00) == 0x00000300 /* B(11:8) == 0011 */)
    return VectorLoad_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000700) == 0x00000200 /* B(11:8) == x010 */)
    return VectorLoad_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000600) == 0x00000000 /* B(11:8) == x00x */)
    return VectorLoad_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* A(23:23) == 0 */ &&
      (insn.Bits() & 0x00000C00) == 0x00000400 /* B(11:8) == 01xx */)
    return VectorLoad_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */)
    return VectorLoad_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: super_cop.
// Specified by: See Section A5.6
const ClassDecoder& Arm32DecoderState::decode_super_cop(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x03F00000) == 0x00400000 /* op1(25:20) == 000100 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x03F00000) == 0x00500000 /* op1(25:20) == 000101 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return MoveDoubleFromCoprocessor_instance_;

  if ((insn.Bits() & 0x03E00000) == 0x00000000 /* op1(25:20) == 00000x */)
    return Undefined_instance_;

  if ((insn.Bits() & 0x03E00000) == 0x00400000 /* op1(25:20) == 00010x */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */)
    return decode_ext_reg_transfers(insn);

  if ((insn.Bits() & 0x03100000) == 0x02100000 /* op1(25:20) == 10xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return MoveFromCoprocessor_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00000000 /* op1(25:20) == 0xxxx0 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00100000 /* op1(25:20) == 0xxxx1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00100000 /* op1(25:20) == 0xxxx1 */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000E00) != 0x00000A00 /* coproc(11:8) == ~101x */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */)
    return decode_vfp_data_proc(insn);

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */)
    return decode_ext_reg_move(insn);

  if ((insn.Bits() & 0x03000000) == 0x03000000 /* op1(25:20) == 11xxxx */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* op1(25:20) == 0xxxxx */ &&
      (insn.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8) == 101x */ &&
      (insn.Bits() & 0x03A00000) != 0x00000000 /* op1_repeated(25:20) == ~000x0x */)
    return decode_ext_reg_load_store(insn);

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: sync.
// Specified by: See Section A5.2.10
const ClassDecoder& Arm32DecoderState::decode_sync(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00800000 /* op(23:20) == 1000 */)
    return StoreBasedMemoryRtBits0To3_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00900000 /* op(23:20) == 1001 */)
    return LoadBasedMemory_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00A00000 /* op(23:20) == 1010 */)
    return StoreBasedMemoryDoubleRtBits0To3_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00B00000 /* op(23:20) == 1011 */)
    return LoadBasedMemoryDouble_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00000000 /* op(23:20) == 0x00 */)
    return Deprecated_instance_;

  if ((insn.Bits() & 0x00D00000) == 0x00C00000 /* op(23:20) == 11x0 */)
    return StoreBasedMemoryRtBits0To3_instance_;

  if ((insn.Bits() & 0x00D00000) == 0x00D00000 /* op(23:20) == 11x1 */)
    return LoadBasedMemory_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: unconditional.
// Specified by: See Section A5.7
const ClassDecoder& Arm32DecoderState::decode_unconditional(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x0FF00000) == 0x0C400000 /* op1(27:20) == 11000100 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x0FF00000) == 0x0C500000 /* op1(27:20) == 11000101 */)
    return MoveDoubleFromCoprocessor_instance_;

  if ((insn.Bits() & 0x0FB00000) == 0x0C200000 /* op1(27:20) == 11000x10 */)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x0FB00000) == 0x0C300000 /* op1(27:20) == 11000x11 */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x0F900000) == 0x0C800000 /* op1(27:20) == 11001xx0 */)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x0F900000) == 0x0C900000 /* op1(27:20) == 11001xx1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x0E500000) == 0x08100000 /* op1(27:20) == 100xx0x1 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x0E500000) == 0x08400000 /* op1(27:20) == 100xx1x0 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0D000000 /* op1(27:20) == 1101xxx0 */)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0D100000 /* op1(27:20) == 1101xxx1 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0E000000 /* op1(27:20) == 1110xxx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0E100000 /* op1(27:20) == 1110xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return MoveFromCoprocessor_instance_;

  if ((insn.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20) == 1110xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x0E000000) == 0x0A000000 /* op1(27:20) == 101xxxxx */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x08000000) == 0x00000000 /* op1(27:20) == 0xxxxxxx */)
    return decode_misc_hints_simd(insn);

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

// Implementation of table: vfp_data_proc.
// Specified by: A7.5 Table A7 - 16, page A7 - 24
const ClassDecoder& Arm32DecoderState::decode_vfp_data_proc(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x00B00000) == 0x00800000 /* opc1(23:20) == 1x00 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* opc3(7:6) == x0 */)
    return VfpOp_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00B00000 /* opc1(23:20) == 1x11 */)
    return decode_other_vfp_data_proc(insn);

  if ((insn.Bits() & 0x00800000) == 0x00000000 /* opc1(23:20) == 0xxx */)
    return VfpOp_instance_;

  // Catch any attempt to fall though ...
  return not_implemented_;
}

const ClassDecoder& Arm32DecoderState::decode(const Instruction insn) const {
  return decode_ARMv7(insn);
}

}  // namespace nacl_arm_dec
