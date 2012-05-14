/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/actual_classes.h"

// Implementation of static interface accessors. This are put in a
// separate file, since they are only needed if one compiles in dbg
// mode rather than opt.

namespace nacl_arm_dec {

// Defs12To15
const RegDBits12To15Interface Defs12To15::d;
const UpdatesConditionsBit20Interface Defs12To15::conditions;

// Defs12To15RdRnRsRmNotPc
const RegMBits0To3Interface Defs12To15RdRnRsRmNotPc::m;
const RegSBits8To11Interface Defs12To15RdRnRsRmNotPc::s;
const RegNBits16To19Interface Defs12To15RdRnRsRmNotPc::n;

// ImmediateBic
const Imm12Bits0To11Interface ImmediateBic::imm12;

// TestImmediate
const Imm12Bits0To11Interface TestImmediate::imm12;

}  // namespace nacl_arm_dec
