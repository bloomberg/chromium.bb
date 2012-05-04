/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/baseline_classes.h"

// Implementation of static interface accessors. This are put in a
// separate file, since they are only needed if one compiles in dbg
// mode rather than opt.

namespace nacl_arm_dec {

// Unary1RegisterImmediateOp
const Imm12Bits0To11Interface Unary1RegisterImmediateOp::imm12;
const RegDBits12To15Interface Unary1RegisterImmediateOp::d;
const Imm4Bits16To19Interface Unary1RegisterImmediateOp::imm4;
const UpdatesFlagsRegisterBit20Interface
Unary1RegisterImmediateOp::flags;
const ConditionBits28To31Interface Unary1RegisterImmediateOp::cond;

// Binary2RegisterImmediateOp
const Imm12Bits0To11Interface Binary2RegisterImmediateOp::imm;
const RegDBits12To15Interface Binary2RegisterImmediateOp::d;
const RegNBits16To19Interface Binary2RegisterImmediateOp::n;
const UpdatesFlagsRegisterBit20Interface Binary2RegisterImmediateOp::flags;
const ConditionBits28To31Interface Binary2RegisterImmediateOp::cond;

// BinaryRegisterImmediateTest::
const Imm12Bits0To11Interface BinaryRegisterImmediateTest::imm;
const RegNBits16To19Interface BinaryRegisterImmediateTest::n;
const UpdatesFlagsRegisterBit20Interface BinaryRegisterImmediateTest::flags;
const ConditionBits28To31Interface BinaryRegisterImmediateTest::cond;

// Unary2RegisterOp
const RegMBits0To3Interface Unary2RegisterOp::m;
const RegDBits12To15Interface Unary2RegisterOp::d;
const UpdatesFlagsRegisterBit20Interface Unary2RegisterOp::flags;
const ConditionBits28To31Interface Unary2RegisterOp::cond;

// Binary3RegisterOp
const RegNBits0To3Interface Binary3RegisterOp::n;
const RegMBits8To11Interface Binary3RegisterOp::m;
const RegDBits12To15Interface Binary3RegisterOp::d;
const UpdatesFlagsRegisterBit20Interface Binary3RegisterOp::flags;
const ConditionBits28To31Interface Binary3RegisterOp::cond;

// Unary2RegisterImmedShiftedOp
const RegMBits0To3Interface Unary2RegisterImmedShiftedOp::m;
const ShiftTypeBits5To6Interface
Unary2RegisterImmedShiftedOp::shift_type;
const Imm5Bits7To11Interface Unary2RegisterImmedShiftedOp::imm;
const RegDBits12To15Interface Unary2RegisterImmedShiftedOp::d;
const UpdatesFlagsRegisterBit20Interface
Unary2RegisterImmedShiftedOp::flags;
const ConditionBits28To31Interface
Unary2RegisterImmedShiftedOp::cond;

// Unary3RegisterShiftedOp
const RegMBits0To3Interface Unary3RegisterShiftedOp::m;
const ShiftTypeBits5To6Interface Unary3RegisterShiftedOp::shift_type;
const RegSBits8To11Interface Unary3RegisterShiftedOp::s;
const RegDBits12To15Interface Unary3RegisterShiftedOp::d;
const UpdatesFlagsRegisterBit20Interface
Unary3RegisterShiftedOp::flags;
const ConditionBits28To31Interface Unary3RegisterShiftedOp::cond;

// Binary3RegisterImmedShiftedOp
const RegMBits0To3Interface Binary3RegisterImmedShiftedOp::m;
const ShiftTypeBits5To6Interface
Binary3RegisterImmedShiftedOp::shift_type;
const Imm5Bits7To11Interface Binary3RegisterImmedShiftedOp::imm;
const RegDBits12To15Interface Binary3RegisterImmedShiftedOp::d;
const RegNBits16To19Interface Binary3RegisterImmedShiftedOp::n;
const UpdatesFlagsRegisterBit20Interface
Binary3RegisterImmedShiftedOp::flags;
const ConditionBits28To31Interface Binary3RegisterImmedShiftedOp::cond;

// Binary4RegisterShiftedOp
const RegMBits0To3Interface Binary4RegisterShiftedOp::m;
const RegSBits8To11Interface Binary4RegisterShiftedOp::s;
const RegDBits12To15Interface Binary4RegisterShiftedOp::d;
const RegNBits16To19Interface Binary4RegisterShiftedOp::n;
const UpdatesFlagsRegisterBit20Interface
Binary4RegisterShiftedOp::flags;
const ConditionBits28To31Interface Binary4RegisterShiftedOp::cond;

// Binary2RegisterImmedShiftedTest::
const RegMBits0To3Interface Binary2RegisterImmedShiftedTest::m;
const ShiftTypeBits5To6Interface
Binary2RegisterImmedShiftedTest::shift_type;
const Imm5Bits7To11Interface Binary2RegisterImmedShiftedTest::imm;
const RegNBits16To19Interface Binary2RegisterImmedShiftedTest::n;
const UpdatesFlagsRegisterBit20Interface
Binary2RegisterImmedShiftedTest::flags;
const ConditionBits28To31Interface
Binary2RegisterImmedShiftedTest::cond;

// Binary3RegisterShiftedTest
const RegMBits0To3Interface Binary3RegisterShiftedTest::m;
const ShiftTypeBits5To6Interface Binary3RegisterShiftedTest::shift_type;
const RegSBits8To11Interface Binary3RegisterShiftedTest::s;
const RegNBits16To19Interface Binary3RegisterShiftedTest::n;
const UpdatesFlagsRegisterBit20Interface
Binary3RegisterShiftedTest::flags;
const ConditionBits28To31Interface Binary3RegisterShiftedTest::cond;

}  // namespace nacl_arm_dec
