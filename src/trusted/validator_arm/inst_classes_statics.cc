/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/inst_classes.h"

// Implementation of static interface accessors. This are put in a
// separate file, since they are only needed if one compiles in dbg
// mode rather than opt.

namespace nacl_arm_dec {

// Unary1RegisterImmediateOp
const Imm12Bits0To11Interface Unary1RegisterImmediateOp::imm12_;
const RegDBits12To15Interface Unary1RegisterImmediateOp::d_;
const Imm4Bits16To19Interface Unary1RegisterImmediateOp::imm4_;
const UpdatesFlagsRegisterBit20Interface
Unary1RegisterImmediateOp::flags_;
const ConditionBits28To31Interface Unary1RegisterImmediateOp::cond_;

// Unary2RegisterOp
const RegMBits0To3Interface Unary2RegisterOp::m_;
const RegDBits12To15Interface Unary2RegisterOp::d_;
const UpdatesFlagsRegisterBit20Interface Unary2RegisterOp::flags_;
const ConditionBits28To31Interface Unary2RegisterOp::cond_;

// Binary3RegisterOp
const RegNBits0To3Interface Binary3RegisterOp::n_;
const RegMBits8To11Interface Binary3RegisterOp::m_;
const RegDBits12To15Interface Binary3RegisterOp::d_;
const UpdatesFlagsRegisterBit20Interface Binary3RegisterOp::flags_;
const ConditionBits28To31Interface Binary3RegisterOp::cond_;

// Unary2RegisterImmedShiftedOp
const RegMBits0To3Interface Unary2RegisterImmedShiftedOp::m_;
const ShiftTypeBits5To6Interface
Unary2RegisterImmedShiftedOp::shift_type_;
const Imm5Bits7To11Interface Unary2RegisterImmedShiftedOp::imm_;
const RegDBits12To15Interface Unary2RegisterImmedShiftedOp::d_;
const UpdatesFlagsRegisterBit20Interface
Unary2RegisterImmedShiftedOp::flags_;
const ConditionBits28To31Interface
Unary2RegisterImmedShiftedOp::cond_;

// Unary3RegisterShiftedOp
const RegMBits0To3Interface Unary3RegisterShiftedOp::m_;
const ShiftTypeBits5To6Interface Unary3RegisterShiftedOp::shift_type_;
const RegSBits8To11Interface Unary3RegisterShiftedOp::s_;
const RegDBits12To15Interface Unary3RegisterShiftedOp::d_;
const UpdatesFlagsRegisterBit20Interface
Unary3RegisterShiftedOp::flags_;
const ConditionBits28To31Interface Unary3RegisterShiftedOp::cond_;

// Binary3RegisterImmedShiftedOp
const RegMBits0To3Interface Binary3RegisterImmedShiftedOp::m_;
const ShiftTypeBits5To6Interface
Binary3RegisterImmedShiftedOp::shift_type_;
const Imm5Bits7To11Interface Binary3RegisterImmedShiftedOp::imm_;
const RegDBits12To15Interface Binary3RegisterImmedShiftedOp::d_;
const RegNBits16To19Interface Binary3RegisterImmedShiftedOp::n_;
const UpdatesFlagsRegisterBit20Interface
Binary3RegisterImmedShiftedOp::flags_;
const ConditionBits28To31Interface Binary3RegisterImmedShiftedOp::cond_;

// Binary4RegisterShiftedOp
const RegMBits0To3Interface Binary4RegisterShiftedOp::m_;
const RegSBits8To11Interface Binary4RegisterShiftedOp::s_;
const RegDBits12To15Interface Binary4RegisterShiftedOp::d_;
const RegNBits16To19Interface Binary4RegisterShiftedOp::n_;
const UpdatesFlagsRegisterBit20Interface
Binary4RegisterShiftedOp::flags_;
const ConditionBits28To31Interface Binary4RegisterShiftedOp::cond_;

// Binary2RegisterImmedShiftedTest::
const RegMBits0To3Interface Binary2RegisterImmedShiftedTest::m_;
const ShiftTypeBits5To6Interface
Binary2RegisterImmedShiftedTest::shift_type_;
const Imm5Bits7To11Interface Binary2RegisterImmedShiftedTest::imm_;
const RegNBits16To19Interface Binary2RegisterImmedShiftedTest::n_;
const UpdatesFlagsRegisterBit20Interface
Binary2RegisterImmedShiftedTest::flags_;
const ConditionBits28To31Interface
Binary2RegisterImmedShiftedTest::cond_;

// Binary3RegisterShiftedTest
const RegMBits0To3Interface Binary3RegisterShiftedTest::m_;
const ShiftTypeBits5To6Interface Binary3RegisterShiftedTest::shift_type_;
const RegSBits8To11Interface Binary3RegisterShiftedTest::s_;
const RegNBits16To19Interface Binary3RegisterShiftedTest::n_;
const UpdatesFlagsRegisterBit20Interface
Binary3RegisterShiftedTest::flags_;
const ConditionBits28To31Interface Binary3RegisterShiftedTest::cond_;

}  // namespace nacl_arm_dec
