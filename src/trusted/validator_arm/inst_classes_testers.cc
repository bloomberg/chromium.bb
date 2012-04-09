/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"

#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named.h"

using nacl_arm_dec::kRegisterFlags;
using nacl_arm_dec::kRegisterNone;
using nacl_arm_dec::kRegisterPc;
using nacl_arm_dec::ClassDecoder;
using nacl_arm_dec::Instruction;
using nacl_arm_dec::NamedBinary4RegisterShiftedOp;

namespace nacl_arm_test {

Binary4RegisterShiftedOpTester::Binary4RegisterShiftedOpTester()
      : Arm32DecoderTester(state_.Binary4RegisterShiftedOp_instance_) {}

void Binary4RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst, const ClassDecoder& decoder) {
  NamedBinary4RegisterShiftedOp &expected_decoder =
      state_.Binary4RegisterShiftedOp_instance_;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond_.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond_.undefined(inst) &&
      (&expected_decoder != &decoder)) return;

  // Check if expected class name found.
  Arm32DecoderTester::ApplySanityChecks(inst, decoder);

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.n_.number(inst), inst.bits(19, 16));
  EXPECT_EQ(expected_decoder.d_.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.s_.number(inst), inst.bits(11, 8));
  EXPECT_EQ(expected_decoder.m_.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.flags_.is_updated(inst), inst.bit(20));
  if (expected_decoder.flags_.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.flags_.reg_if_updated(inst), kRegisterFlags);
  } else {
    EXPECT_EQ(expected_decoder.flags_.reg_if_updated(inst), kRegisterNone);
  }

  // Other ARM constraints about this instruction.
  EXPECT_NE(expected_decoder.n_.reg(inst), kRegisterPc)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.d_.reg(inst), kRegisterPc)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.s_.reg(inst), kRegisterPc)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.m_.reg(inst), kRegisterPc)
      << "Expected Unpredictable for " << InstContents();
}

}  // namespace
