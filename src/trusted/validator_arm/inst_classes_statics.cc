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

// Unary3RegisterShiftedOp
const RegMBits0To3Interface Unary3RegisterShiftedOp::m_;
const RegSBits8To11Interface Unary3RegisterShiftedOp::s_;
const RegDBits12To15Interface Unary3RegisterShiftedOp::d_;
const UpdatesFlagsRegisterBit20Interface Unary3RegisterShiftedOp::flags_;
const ConditionBits28To31Interface Unary3RegisterShiftedOp::cond_;

// Binary4RegisterShiftedOp
const RegMBits0To3Interface Binary4RegisterShiftedOp::m_;
const RegSBits8To11Interface Binary4RegisterShiftedOp::s_;
const RegDBits12To15Interface Binary4RegisterShiftedOp::d_;
const RegNBits16To19Interface Binary4RegisterShiftedOp::n_;
const UpdatesFlagsRegisterBit20Interface Binary4RegisterShiftedOp::flags_;
const ConditionBits28To31Interface Binary4RegisterShiftedOp::cond_;

// Binary3RegisterShiftedTest
const RegMBits0To3Interface Binary3RegisterShiftedTest::m_;
const RegSBits8To11Interface Binary3RegisterShiftedTest::s_;
const RegNBits16To19Interface Binary3RegisterShiftedTest::n_;
const UpdatesFlagsRegisterBit20Interface Binary3RegisterShiftedTest::flags_;
const ConditionBits28To31Interface Binary3RegisterShiftedTest::cond_;

}  // namespace nacl_arm_dec
