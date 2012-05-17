/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE


#include "native_client/src/trusted/validator_arm/gen/arm32_decode.h"

#include <stdio.h>

namespace nacl_arm_dec {


Arm32DecoderState::Arm32DecoderState() : DecoderState()
  , Binary2RegisterImmediateOp_instance_()
  , Binary3RegisterOp_instance_()
  , Binary3RegisterShiftedTest_instance_()
  , Binary4RegisterShiftedOp_instance_()
  , BinaryRegisterImmediateTest_instance_()
  , Branch_instance_()
  , Breakpoint_instance_()
  , BxBlx_instance_()
  , CoprocessorOp_instance_()
  , DataProc_instance_()
  , Defs12To15_instance_()
  , Defs12To15RdRnRsRmNotPc_instance_()
  , Deprecated_instance_()
  , EffectiveNoOp_instance_()
  , Forbidden_instance_()
  , LdrRegister_instance_()
  , LoadCoprocessor_instance_()
  , LoadDoubleExclusive_instance_()
  , LoadDoubleI_instance_()
  , LoadDoubleR_instance_()
  , LoadExclusive_instance_()
  , LoadImmediate_instance_()
  , LoadMultiple_instance_()
  , LoadRegister_instance_()
  , LongMultiply_instance_()
  , MaskAddress_instance_()
  , MaybeSetsConds_instance_()
  , MoveDoubleFromCoprocessor_instance_()
  , MoveFromCoprocessor_instance_()
  , MoveToStatusRegister_instance_()
  , Multiply_instance_()
  , PackSatRev_instance_()
  , Roadblock_instance_()
  , SatAddSub_instance_()
  , StoreCoprocessor_instance_()
  , StoreDoubleR_instance_()
  , StoreExclusive_instance_()
  , StoreImmediate_instance_()
  , StoreRegister_instance_()
  , StrRegister_instance_()
  , TestIfAddressMasked_instance_()
  , Unary1RegisterBitRange_instance_()
  , Unary1RegisterImmediateOp_instance_()
  , Unary2RegisterImmedShiftedOp_instance_()
  , Unary2RegisterOp_instance_()
  , Unary3RegisterShiftedOp_instance_()
  , Undefined_instance_()
  , Unpredictable_instance_()
  , VectorLoad_instance_()
  , VectorStore_instance_()
{}

Arm32DecoderState::~Arm32DecoderState() {}

// Implementation of table: ARMv7.
// Specified by: See Section A5.1
const ClassDecoder& Arm32DecoderState::decode_ARMv7(
     const Instruction insn) const
{

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0E000000) == 0x04000000 /* op1(27:25) == 010 */ &&
      true)
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
      (insn.Bits() & 0x0C000000) == 0x00000000 /* op1(27:25) == 00x */ &&
      true)
    return decode_dp_misc(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x08000000 /* op1(27:25) == 10x */ &&
      true)
    return decode_branch_block_xfer(insn);

  if ((insn.Bits() & 0xF0000000) != 0xF0000000 /* cond(31:28) == ~1111 */ &&
      (insn.Bits() & 0x0C000000) == 0x0C000000 /* op1(27:25) == 11x */ &&
      true)
    return decode_super_cop(insn);

  if ((insn.Bits() & 0xF0000000) == 0xF0000000 /* cond(31:28) == 1111 */ &&
      true &&
      true)
    return decode_unconditional(insn);

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: ARMv7 could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: branch_block_xfer.
// Specified by: See Section A5.5
const ClassDecoder& Arm32DecoderState::decode_branch_block_xfer(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x02500000) == 0x00000000 /* op(25:20) == 0xx0x0 */)
    return StoreImmediate_instance_;

  if ((insn.Bits() & 0x02500000) == 0x00100000 /* op(25:20) == 0xx0x1 */)
    return LoadMultiple_instance_;

  if ((insn.Bits() & 0x02400000) == 0x00400000 /* op(25:20) == 0xx1xx */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:20) == 1xxxxx */)
    return Branch_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: branch_block_xfer could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: dp_immed.
// Specified by: See Section A5.2.3
const ClassDecoder& Arm32DecoderState::decode_dp_immed(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01F00000) == 0x00400000 /* op(24:20) == 00100 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Unary1RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00500000 /* op(24:20) == 00101 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Binary2RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00800000 /* op(24:20) == 01000 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Unary1RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x00900000 /* op(24:20) == 01001 */ &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Binary2RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01100000 /* op(24:20) == 10001 */ &&
      true)
    return TestIfAddressMasked_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01500000 /* op(24:20) == 10101 */ &&
      true)
    return BinaryRegisterImmediateTest_instance_;

  if ((insn.Bits() & 0x01B00000) == 0x01300000 /* op(24:20) == 10x11 */ &&
      true)
    return BinaryRegisterImmediateTest_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00400000 /* op(24:20) == 0010x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Binary2RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op(24:20) == 0100x */ &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return Binary2RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00A00000 /* op(24:20) == 0101x */ &&
      true)
    return Binary2RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00C00000 /* op(24:20) == 0110x */ &&
      true)
    return Binary2RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01800000 /* op(24:20) == 1100x */ &&
      true)
    return Binary2RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op(24:20) == 1110x */ &&
      true)
    return MaskAddress_instance_;

  if ((insn.Bits() & 0x01600000) == 0x00600000 /* op(24:20) == 0x11x */ &&
      true)
    return Binary2RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x01A00000 /* op(24:20) == 11x1x */ &&
      true)
    return Unary1RegisterImmediateOp_instance_;

  if ((insn.Bits() & 0x01C00000) == 0x00000000 /* op(24:20) == 000xx */ &&
      true)
    return Binary2RegisterImmediateOp_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_immed could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
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
      (insn.Bits() & 0x01F00000) == 0x01000000 /* op1(24:20) == 10000 */ &&
      true)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01F00000) == 0x01400000 /* op1(24:20) == 10100 */ &&
      true)
    return DataProc_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01B00000) == 0x01200000 /* op1(24:20) == 10x10 */ &&
      true)
    return decode_msr_and_hints(insn);

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* op(25:25) == 1 */ &&
      (insn.Bits() & 0x01900000) != 0x01000000 /* op1(24:20) == ~10xx0 */ &&
      true)
    return decode_dp_immed(insn);

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_misc could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: dp_reg.
// Specified by: See Section A5.2.1
const ClassDecoder& Arm32DecoderState::decode_dp_reg(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */)
    return Unary2RegisterImmedShiftedOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */)
    return Unary2RegisterImmedShiftedOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op3(6:5) == 00 */)
    return Unary2RegisterOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      (insn.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */ &&
      (insn.Bits() & 0x00000060) == 0x00000060 /* op3(6:5) == 11 */)
    return Unary2RegisterOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      true &&
      (insn.Bits() & 0x00000060) == 0x00000020 /* op3(6:5) == 01 */)
    return Unary2RegisterImmedShiftedOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */ &&
      true &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op3(6:5) == 10 */)
    return Unary2RegisterImmedShiftedOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */ &&
      true &&
      true)
    return Unary2RegisterImmedShiftedOp_instance_;

  if ((insn.Bits() & 0x01900000) == 0x01100000 /* op1(24:20) == 10xx1 */ &&
      true &&
      true)
    return MaybeSetsConds_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x01800000 /* op1(24:20) == 11x0x */ &&
      true &&
      true)
    return Defs12To15_instance_;

  if ((insn.Bits() & 0x01000000) == 0x00000000 /* op1(24:20) == 0xxxx */ &&
      true &&
      true)
    return Defs12To15_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_reg could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: dp_reg_shifted.
// Specified by: See Section A5.2.2
const ClassDecoder& Arm32DecoderState::decode_dp_reg_shifted(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x01E00000) == 0x00600000 /* op1(24:20) == 0011x */)
    return Defs12To15RdRnRsRmNotPc_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x00800000 /* op1(24:20) == 0100x */)
    return Defs12To15RdRnRsRmNotPc_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01A00000 /* op1(24:20) == 1101x */)
    return Binary3RegisterOp_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01E00000 /* op1(24:20) == 1111x */)
    return Unary3RegisterShiftedOp_instance_;

  if ((insn.Bits() & 0x01600000) == 0x00400000 /* op1(24:20) == 0x10x */)
    return Binary4RegisterShiftedOp_instance_;

  if ((insn.Bits() & 0x01900000) == 0x01100000 /* op1(24:20) == 10xx1 */)
    return Binary3RegisterShiftedTest_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x00A00000 /* op1(24:20) == 01x1x */)
    return Binary4RegisterShiftedOp_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x01800000 /* op1(24:20) == 11x0x */)
    return Binary4RegisterShiftedOp_instance_;

  if ((insn.Bits() & 0x01C00000) == 0x00000000 /* op1(24:20) == 000xx */)
    return Binary4RegisterShiftedOp_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: dp_reg_shifted could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: extra_load_store.
// Specified by: See Section A5.2.8
const ClassDecoder& Arm32DecoderState::decode_extra_load_store(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return StrRegister_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000020 /* op2(6:5) == 01 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */)
    return LdrRegister_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return LoadDoubleR_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */)
    return LdrRegister_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */)
    return LoadDoubleI_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000040 /* op2(6:5) == 10 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */)
    return LoadImmediate_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00000000 /* op1(24:20) == xx0x0 */)
    return StoreDoubleR_instance_;

  if ((insn.Bits() & 0x00000060) == 0x00000060 /* op2(6:5) == 11 */ &&
      (insn.Bits() & 0x00500000) == 0x00100000 /* op1(24:20) == xx0x1 */)
    return LoadRegister_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op2(6:5) == x1 */ &&
      (insn.Bits() & 0x00500000) == 0x00400000 /* op1(24:20) == xx1x0 */)
    return StoreImmediate_instance_;

  if ((insn.Bits() & 0x00000020) == 0x00000020 /* op2(6:5) == x1 */ &&
      (insn.Bits() & 0x00500000) == 0x00500000 /* op1(24:20) == xx1x1 */)
    return LoadImmediate_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: extra_load_store could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: half_mult.
// Specified by: See Section A5.2.7
const ClassDecoder& Arm32DecoderState::decode_half_mult(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00600000) == 0x00400000 /* op1(22:21) == 10 */)
    return LongMultiply_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00600000 /* op1(22:21) == 11 */)
    return Multiply_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op1(22:21) == 0x */)
    return Multiply_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: half_mult could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: load_store_word_byte.
// Specified by: See Section A5.3
const ClassDecoder& Arm32DecoderState::decode_load_store_word_byte(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01300000) == 0x00000000 /* op1(24:20) == 0xx00 */ &&
      true)
    return StoreImmediate_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01300000) == 0x00100000 /* op1(24:20) == 0xx01 */ &&
      true)
    return LoadImmediate_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01100000) == 0x01000000 /* op1(24:20) == 1xxx0 */ &&
      true)
    return StoreImmediate_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01100000) == 0x01100000 /* op1(24:20) == 1xxx1 */ &&
      true)
    return LoadImmediate_instance_;

  if ((insn.Bits() & 0x02000000) == 0x00000000 /* A(25:25) == 0 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      true)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01700000) == 0x00500000 /* op1(24:20) == 0x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return LoadRegister_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x00700000) == 0x00100000 /* op1(24:20) == xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return LoadRegister_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01300000) == 0x00000000 /* op1(24:20) == 0xx00 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return StoreRegister_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01100000) == 0x01000000 /* op1(24:20) == 1xxx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return StoreRegister_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01100000) == 0x01100000 /* op1(24:20) == 1xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return LoadRegister_instance_;

  if ((insn.Bits() & 0x02000000) == 0x02000000 /* A(25:25) == 1 */ &&
      (insn.Bits() & 0x01200000) == 0x00200000 /* op1(24:20) == 0xx1x */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */)
    return Forbidden_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: load_store_word_byte could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: media.
// Specified by: See Section A5.4
const ClassDecoder& Arm32DecoderState::decode_media(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x01F00000) == 0x01800000 /* op1(24:20) == 11000 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000000 /* op2(7:5) == 000 */ &&
      true)
    return Multiply_instance_;

  if ((insn.Bits() & 0x01F00000) == 0x01F00000 /* op1(24:20) == 11111 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */ &&
      true)
    return Roadblock_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) != 0x0000000F)
    return DataProc_instance_;

  if ((insn.Bits() & 0x01E00000) == 0x01C00000 /* op1(24:20) == 1110x */ &&
      (insn.Bits() & 0x00000060) == 0x00000000 /* op2(7:5) == x00 */ &&
      (insn.Bits() & 0x0000000F) == 0x0000000F)
    return Unary1RegisterBitRange_instance_;

  if ((insn.Bits() & 0x01A00000) == 0x01A00000 /* op1(24:20) == 11x1x */ &&
      (insn.Bits() & 0x00000060) == 0x00000040 /* op2(7:5) == x10 */ &&
      true)
    return DataProc_instance_;

  if ((insn.Bits() & 0x01800000) == 0x00000000 /* op1(24:20) == 00xxx */ &&
      true &&
      true)
    return decode_parallel_add_sub(insn);

  if ((insn.Bits() & 0x01800000) == 0x00800000 /* op1(24:20) == 01xxx */ &&
      true &&
      true)
    return decode_pack_sat_rev(insn);

  if ((insn.Bits() & 0x01800000) == 0x01000000 /* op1(24:20) == 10xxx */ &&
      true &&
      true)
    return decode_signed_mult(insn);

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: media could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: misc.
// Specified by: See Section A5.2.12
const ClassDecoder& Arm32DecoderState::decode_misc(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00000000 /* op1(19:16) == xx00 */)
    return MoveToStatusRegister_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      true)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000000 /* op2(6:4) == 000 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* op(22:21) == x0 */ &&
      true)
    return DataProc_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      true)
    return BxBlx_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000010 /* op2(6:4) == 001 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      true)
    return DataProc_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000020 /* op2(6:4) == 010 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      true)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000030 /* op2(6:4) == 011 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      true)
    return BxBlx_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000050 /* op2(6:4) == 101 */ &&
      true &&
      true)
    return decode_sat_add_sub(insn);

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00200000 /* op(22:21) == 01 */ &&
      true)
    return Breakpoint_instance_;

  if ((insn.Bits() & 0x00000070) == 0x00000070 /* op2(6:4) == 111 */ &&
      (insn.Bits() & 0x00600000) == 0x00600000 /* op(22:21) == 11 */ &&
      true)
    return Forbidden_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: misc could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
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
      (insn.Bits() & 0x000000F0) == 0x00000010 /* op2(7:4) == 0001 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000F0) == 0x00000050 /* op2(7:4) == 0101 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07F00000) == 0x05700000 /* op1(26:20) == 1010111 */ &&
      (insn.Bits() & 0x000000D0) == 0x00000040 /* op2(7:4) == 01x0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x04100000 /* op1(26:20) == 100x001 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x04500000 /* op1(26:20) == 100x101 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05100000 /* op1(26:20) == 101x001 */ &&
      true &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05100000 /* op1(26:20) == 101x001 */ &&
      true &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return Unpredictable_instance_;

  if ((insn.Bits() & 0x07700000) == 0x05500000 /* op1(26:20) == 101x101 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x06500000 /* op1(26:20) == 110x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x07700000) == 0x07500000 /* op1(26:20) == 111x101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x06700000) == 0x06100000 /* op1(26:20) == 11xx001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x06300000) == 0x04300000 /* op1(26:20) == 10xxx11 */ &&
      true &&
      true)
    return Unpredictable_instance_;

  if ((insn.Bits() & 0x06300000) == 0x06300000 /* op1(26:20) == 11xxx11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op2(7:4) == xxx0 */ &&
      true)
    return Unpredictable_instance_;

  if ((insn.Bits() & 0x07100000) == 0x04000000 /* op1(26:20) == 100xxx0 */ &&
      true &&
      true)
    return decode_simd_load_store(insn);

  if ((insn.Bits() & 0x06000000) == 0x02000000 /* op1(26:20) == 01xxxxx */ &&
      true &&
      true)
    return decode_simd_dp(insn);

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: misc_hints_simd could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: msr_and_hints.
// Specified by: See Section A5.2.11
const ClassDecoder& Arm32DecoderState::decode_msr_and_hints(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000000)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000002)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FF) == 0x00000004)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000FD) == 0x00000001)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00000000 /* op1(19:16) == 0000 */ &&
      (insn.Bits() & 0x000000F0) == 0x000000F0 /* op2(7:0) == 1111xxxx */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000F0000) == 0x00040000 /* op1(19:16) == 0100 */ &&
      true)
    return MoveToStatusRegister_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x000B0000) == 0x00080000 /* op1(19:16) == 1x00 */ &&
      true)
    return MoveToStatusRegister_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00030000) == 0x00010000 /* op1(19:16) == xx01 */ &&
      true)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00000000 /* op(22:22) == 0 */ &&
      (insn.Bits() & 0x00020000) == 0x00020000 /* op1(19:16) == xx1x */ &&
      true)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x00400000) == 0x00400000 /* op(22:22) == 1 */ &&
      true &&
      true)
    return Forbidden_instance_;

  if (true)
    return Forbidden_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: msr_and_hints could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: mult.
// Specified by: See Section A5.2.5
const ClassDecoder& Arm32DecoderState::decode_mult(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00400000 /* op(23:20) == 0100 */)
    return LongMultiply_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00600000 /* op(23:20) == 0110 */)
    return Multiply_instance_;

  if ((insn.Bits() & 0x00D00000) == 0x00500000 /* op(23:20) == 01x1 */)
    return Undefined_instance_;

  if ((insn.Bits() & 0x00C00000) == 0x00000000 /* op(23:20) == 00xx */)
    return Multiply_instance_;

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* op(23:20) == 1xxx */)
    return LongMultiply_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: mult could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
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

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00600000 /* op1(22:20) == 110 */ &&
      (insn.Bits() & 0x000000A0) == 0x00000020 /* op2(7:5) == 0x1 */)
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

  if ((insn.Bits() & 0x00500000) == 0x00000000 /* op1(22:20) == 0x0 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000060 /* op2(7:5) == 011 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00600000) == 0x00200000 /* op1(22:20) == 01x */ &&
      (insn.Bits() & 0x000000E0) == 0x00000020 /* op2(7:5) == 001 */)
    return PackSatRev_instance_;

  if ((insn.Bits() & 0x00200000) == 0x00200000 /* op1(22:20) == x1x */ &&
      (insn.Bits() & 0x00000020) == 0x00000000 /* op2(7:5) == xx0 */)
    return PackSatRev_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: pack_sat_rev could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: parallel_add_sub.
// Specified by: See Sections A5.4.1, A5.4.2
const ClassDecoder& Arm32DecoderState::decode_parallel_add_sub(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return DataProc_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return DataProc_instance_;

  if ((insn.Bits() & 0x00300000) == 0x00200000 /* op1(21:20) == 10 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return DataProc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x000000E0) == 0x00000080 /* op2(7:5) == 100 */)
    return DataProc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x000000E0) == 0x000000E0 /* op2(7:5) == 111 */)
    return DataProc_instance_;

  if ((insn.Bits() & 0x00100000) == 0x00100000 /* op1(21:20) == x1 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */)
    return DataProc_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: parallel_add_sub could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: sat_add_sub.
// Specified by: See Section A5.2.6
const ClassDecoder& Arm32DecoderState::decode_sat_add_sub(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if (true)
    return SatAddSub_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: sat_add_sub could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: signed_mult.
// Specified by: See Section A5.4.4
const ClassDecoder& Arm32DecoderState::decode_signed_mult(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000040 /* op2(7:5) == 01x */ &&
      (insn.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */)
    return Multiply_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00000000 /* op1(22:20) == 000 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */ &&
      true)
    return Multiply_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00400000 /* op1(22:20) == 100 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* op2(7:5) == 0xx */ &&
      true)
    return LongMultiply_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x00000000 /* op2(7:5) == 00x */ &&
      true)
    return Multiply_instance_;

  if ((insn.Bits() & 0x00700000) == 0x00500000 /* op1(22:20) == 101 */ &&
      (insn.Bits() & 0x000000C0) == 0x000000C0 /* op2(7:5) == 11x */ &&
      true)
    return Multiply_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: signed_mult could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: simd_dp.
// Specified by: See Section A7.4
const ClassDecoder& Arm32DecoderState::decode_simd_dp(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      (insn.Bits() & 0x00B00000) == 0x00B00000 /* A(23:19) == 1x11x */ &&
      true &&
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

  if (true &&
      (insn.Bits() & 0x00B80000) == 0x00800000 /* A(23:19) == 1x000 */ &&
      true &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_1imm(insn);

  if (true &&
      (insn.Bits() & 0x00B80000) == 0x00880000 /* A(23:19) == 1x001 */ &&
      true &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if (true &&
      (insn.Bits() & 0x00B00000) == 0x00900000 /* A(23:19) == 1x01x */ &&
      true &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if (true &&
      (insn.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19) == 1x10x */ &&
      true &&
      (insn.Bits() & 0x00000050) == 0x00000000 /* C(7:4) == x0x0 */)
    return decode_simd_dp_3diff(insn);

  if (true &&
      (insn.Bits() & 0x00B00000) == 0x00A00000 /* A(23:19) == 1x10x */ &&
      true &&
      (insn.Bits() & 0x00000050) == 0x00000040 /* C(7:4) == x1x0 */)
    return decode_simd_dp_2scalar(insn);

  if (true &&
      (insn.Bits() & 0x00A00000) == 0x00800000 /* A(23:19) == 1x0xx */ &&
      true &&
      (insn.Bits() & 0x00000050) == 0x00000000 /* C(7:4) == x0x0 */)
    return decode_simd_dp_3diff(insn);

  if (true &&
      (insn.Bits() & 0x00A00000) == 0x00800000 /* A(23:19) == 1x0xx */ &&
      true &&
      (insn.Bits() & 0x00000050) == 0x00000040 /* C(7:4) == x1x0 */)
    return decode_simd_dp_2scalar(insn);

  if (true &&
      (insn.Bits() & 0x00A00000) == 0x00A00000 /* A(23:19) == 1x1xx */ &&
      true &&
      (insn.Bits() & 0x00000090) == 0x00000010 /* C(7:4) == 0xx1 */)
    return decode_simd_dp_2shift(insn);

  if (true &&
      (insn.Bits() & 0x00800000) == 0x00000000 /* A(23:19) == 0xxxx */ &&
      true &&
      true)
    return decode_simd_dp_3same(insn);

  if (true &&
      (insn.Bits() & 0x00800000) == 0x00800000 /* A(23:19) == 1xxxx */ &&
      true &&
      (insn.Bits() & 0x00000090) == 0x00000090 /* C(7:4) == 1xx1 */)
    return decode_simd_dp_2shift(insn);

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: simd_dp_1imm.
// Specified by: See Section A7.4.6
const ClassDecoder& Arm32DecoderState::decode_simd_dp_1imm(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000020) == 0x00000000 /* op(5:5) == 0 */ &&
      true)
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
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_1imm could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
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
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2misc could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: simd_dp_2scalar.
// Specified by: See Section A7.4.3
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2scalar(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8) == 1010 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8) == 1011 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000200 /* A(11:8) == 0x10 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000300 /* A(11:8) == 0x11 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000200) == 0x00000000 /* A(11:8) == xx0x */ &&
      true)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2scalar could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: simd_dp_2shift.
// Specified by: See Section A7.4.4
const ClassDecoder& Arm32DecoderState::decode_simd_dp_2shift(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000500 /* A(11:8) == 0101 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */ &&
      (insn.Bits() & 0x00000080) == 0x00000000 /* L(7:7) == 0 */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000800 /* A(11:8) == 1000 */ &&
      true &&
      (insn.Bits() & 0x00000040) == 0x00000040 /* B(6:6) == 1 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000900 /* A(11:8) == 1001 */ &&
      true &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000A00 /* A(11:8) == 1010 */ &&
      true &&
      (insn.Bits() & 0x00000040) == 0x00000000 /* B(6:6) == 0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000E00) == 0x00000400 /* A(11:8) == 010x */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000600) == 0x00000600 /* A(11:8) == x11x */ &&
      true &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000C00) == 0x00000000 /* A(11:8) == 00xx */ &&
      true &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_2shift could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
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

  if ((insn.Bits() & 0x00000900) == 0x00000800 /* A(11:8) == 1xx0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000800) == 0x00000000 /* A(11:8) == 0xxx */ &&
      true)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_3diff could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: simd_dp_3same.
// Specified by: See Section A7.4.1
const ClassDecoder& Arm32DecoderState::decode_simd_dp_3same(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00000F00) == 0x00000100 /* A(11:8) == 0001 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000500 /* A(11:8) == 0101 */ &&
      true &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000900 /* A(11:8) == 1001 */ &&
      true &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000B00 /* A(11:8) == 1011 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* C(21:20) == 0x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000D00 /* A(11:8) == 1101 */ &&
      true &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      (insn.Bits() & 0x00200000) == 0x00200000 /* C(21:20) == 1x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      true &&
      (insn.Bits() & 0x00200000) == 0x00000000 /* C(21:20) == 0x */)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000F00) == 0x00000E00 /* A(11:8) == 1110 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x01000000 /* U(24:24) == 1 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000700) == 0x00000700 /* A(11:8) == x111 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000300 /* A(11:8) == 0x11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000B00) == 0x00000B00 /* A(11:8) == 1x11 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* B(4:4) == 1 */ &&
      (insn.Bits() & 0x01000000) == 0x00000000 /* U(24:24) == 0 */ &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000D00) == 0x00000100 /* A(11:8) == 00x1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* B(4:4) == 0 */ &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000D00) == 0x00000800 /* A(11:8) == 10x0 */ &&
      true &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if ((insn.Bits() & 0x00000900) == 0x00000000 /* A(11:8) == 0xx0 */ &&
      true &&
      true &&
      true)
    return EffectiveNoOp_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_dp_3same could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
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
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
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
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store_l0 could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
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

  if ((insn.Bits() & 0x00800000) == 0x00800000 /* A(23:23) == 1 */ &&
      true)
    return VectorLoad_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: simd_load_store_l1 could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: super_cop.
// Specified by: See Section A5.6
const ClassDecoder& Arm32DecoderState::decode_super_cop(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x03F00000) == 0x00400000 /* op1(25:20) == 000100 */ &&
      true)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x03F00000) == 0x00500000 /* op1(25:20) == 000101 */ &&
      true)
    return MoveDoubleFromCoprocessor_instance_;

  if ((insn.Bits() & 0x03E00000) == 0x00000000 /* op1(25:20) == 00000x */ &&
      true)
    return Undefined_instance_;

  if ((insn.Bits() & 0x03100000) == 0x02000000 /* op1(25:20) == 10xxx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x03100000) == 0x02100000 /* op1(25:20) == 10xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */)
    return MoveFromCoprocessor_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00000000 /* op1(25:20) == 0xxxx0 */ &&
      true)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x02100000) == 0x00100000 /* op1(25:20) == 0xxxx1 */ &&
      true)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x03000000) == 0x02000000 /* op1(25:20) == 10xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x03000000) == 0x03000000 /* op1(25:20) == 11xxxx */ &&
      true)
    return Forbidden_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: super_cop could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: sync.
// Specified by: See Section A5.2.10
const ClassDecoder& Arm32DecoderState::decode_sync(
     const Instruction insn) const
{
  UNREFERENCED_PARAMETER(insn);
  if ((insn.Bits() & 0x00F00000) == 0x00800000 /* op(23:20) == 1000 */)
    return StoreExclusive_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00900000 /* op(23:20) == 1001 */)
    return LoadExclusive_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00B00000 /* op(23:20) == 1011 */)
    return LoadDoubleExclusive_instance_;

  if ((insn.Bits() & 0x00F00000) == 0x00C00000 /* op(23:20) == 1100 */)
    return StoreExclusive_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00000000 /* op(23:20) == 0x00 */)
    return Deprecated_instance_;

  if ((insn.Bits() & 0x00B00000) == 0x00A00000 /* op(23:20) == 1x10 */)
    return StoreExclusive_instance_;

  if ((insn.Bits() & 0x00D00000) == 0x00D00000 /* op(23:20) == 11x1 */)
    return LoadExclusive_instance_;

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: sync could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

// Implementation of table: unconditional.
// Specified by: See Section A5.7
const ClassDecoder& Arm32DecoderState::decode_unconditional(
     const Instruction insn) const
{

  if ((insn.Bits() & 0x0FF00000) == 0x0C400000 /* op1(27:20) == 11000100 */ &&
      true &&
      true)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x0FF00000) == 0x0C500000 /* op1(27:20) == 11000101 */ &&
      true &&
      true)
    return MoveDoubleFromCoprocessor_instance_;

  if ((insn.Bits() & 0x0FB00000) == 0x0C200000 /* op1(27:20) == 11000x10 */ &&
      true &&
      true)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x0FB00000) == 0x0C300000 /* op1(27:20) == 11000x11 */ &&
      true &&
      (insn.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x0F900000) == 0x0C800000 /* op1(27:20) == 11001xx0 */ &&
      true &&
      true)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x0F900000) == 0x0C900000 /* op1(27:20) == 11001xx1 */ &&
      true &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x0E500000) == 0x08100000 /* op1(27:20) == 100xx0x1 */ &&
      true &&
      true)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x0E500000) == 0x08400000 /* op1(27:20) == 100xx1x0 */ &&
      true &&
      true)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0D000000 /* op1(27:20) == 1101xxx0 */ &&
      true &&
      true)
    return StoreCoprocessor_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0D100000 /* op1(27:20) == 1101xxx1 */ &&
      true &&
      (insn.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */)
    return LoadCoprocessor_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0E000000 /* op1(27:20) == 1110xxx0 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      true)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x0F100000) == 0x0E100000 /* op1(27:20) == 1110xxx1 */ &&
      (insn.Bits() & 0x00000010) == 0x00000010 /* op(4:4) == 1 */ &&
      true)
    return MoveFromCoprocessor_instance_;

  if ((insn.Bits() & 0x0F000000) == 0x0E000000 /* op1(27:20) == 1110xxxx */ &&
      (insn.Bits() & 0x00000010) == 0x00000000 /* op(4:4) == 0 */ &&
      true)
    return CoprocessorOp_instance_;

  if ((insn.Bits() & 0x0E000000) == 0x0A000000 /* op1(27:20) == 101xxxxx */ &&
      true &&
      true)
    return Forbidden_instance_;

  if ((insn.Bits() & 0x08000000) == 0x00000000 /* op1(27:20) == 0xxxxxxx */ &&
      true &&
      true)
    return decode_misc_hints_simd(insn);

  if (true)
    return Undefined_instance_;

  // Catch any attempt to fall though ...
  fprintf(stderr, "TABLE IS INCOMPLETE: unconditional could not parse %08X",
          insn.Bits());
  return Forbidden_instance_;
}

const ClassDecoder& Arm32DecoderState::decode(const Instruction insn) const {
  return decode_ARMv7(insn);
}

}  // namespace nacl_arm_dec
