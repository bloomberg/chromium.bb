/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"

#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/decoder_tester.h"

using nacl_arm_dec::kRegisterFlags;
using nacl_arm_dec::kRegisterNone;
using nacl_arm_dec::kRegisterPc;
using nacl_arm_dec::Instruction;

namespace nacl_arm_test {

Binary4RegisterShiftedOpTester::Binary4RegisterShiftedOpTester()
    : Arm32DecoderTester(state_.Binary4RegisterShiftedOp_instance_)
{}

Binary4RegisterShiftedOpTester::Binary4RegisterShiftedOpTester(
    const NamedBinary4RegisterShiftedOp& decoder)
      : Arm32DecoderTester(decoder) {}

void Binary4RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond_.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond_.undefined(inst) &&
      (&state_.Binary4RegisterShiftedOp_instance_ != &decoder)) return;

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
}

Binary4RegisterShiftedOpTesterRegsNotPc::
Binary4RegisterShiftedOpTesterRegsNotPc()
    : Binary4RegisterShiftedOpTester() {}

Binary4RegisterShiftedOpTesterRegsNotPc::
Binary4RegisterShiftedOpTesterRegsNotPc(
    const NamedBinary4RegisterShiftedOp& decoder)
      : Binary4RegisterShiftedOpTester(decoder) {}

void Binary4RegisterShiftedOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterShiftedOp expected_decoder;

  Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder);

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
