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

using nacl_arm_dec::kConditions;
using nacl_arm_dec::kRegisterNone;
using nacl_arm_dec::kRegisterPc;
using nacl_arm_dec::kRegisterStack;
using nacl_arm_dec::Instruction;

namespace nacl_arm_test {

// Unary1RegisterImmediateOpTester
Unary1RegisterImmediateOpTester::Unary1RegisterImmediateOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary1RegisterImmediateOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary1RegisterImmediateOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.d.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm4.value(inst), inst.bits(19, 16));
  EXPECT_EQ(expected_decoder.imm12.value(inst), inst.bits(11, 0));
  EXPECT_EQ(expected_decoder.ImmediateValue(inst),
            (inst.bits(19, 16) << 12) | inst.bits(11, 0));
  EXPECT_LT(expected_decoder.ImmediateValue(inst), (uint32_t) 0x10000);

  // Other NaCl constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary1RegisterImmediateOpTesterRegsNotPc
Unary1RegisterImmediateOpTesterRegsNotPc::
Unary1RegisterImmediateOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}

bool Unary1RegisterImmediateOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary1RegisterImmediateOp expected_decoder;

  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary1RegisterImmediateOpTesterNotRdIsPcAndS
Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
Unary1RegisterImmediateOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}

bool Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary1RegisterImmediateOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst) == kRegisterPc) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder);
}

// Binary2RegisterImmediateOpTester
Binary2RegisterImmediateOpTester::Binary2RegisterImmediateOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder), apply_rd_is_pc_check_(true) {}

bool Binary2RegisterImmediateOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary2RegisterImmediateOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.n.number(inst), inst.bits(19, 16));
  EXPECT_EQ(expected_decoder.d.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.bits(11, 0));

  // Other NaCl constraints about this instruction.
  if (apply_rd_is_pc_check_) {
    EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
        << "Expected FORBIDDEN_OPERANDS for " << InstContents();
  }

  return true;
}

// Binary2RegisterImmediateOpTesterNotRdIsPcAndS
Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}

bool Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary2RegisterImmediateOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst) == kRegisterPc) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder);
}

// Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS
Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(
    const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}

bool Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary2RegisterImmediateOp expected_decoder;

  // Check that we don't parse when Rn=15 and S=0.
  if ((expected_decoder.n.reg(inst) == kRegisterPc) &&
      !expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      ApplySanityChecks(inst, decoder);
}

// Binary2RegisterImmediateOpTesterRdCanBePcAndNotRdIsPcAndS
Binary2RegisterImmediateOpTesterRdCanBePcAndNotRdIsPcAndS::
Binary2RegisterImmediateOpTesterRdCanBePcAndNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {
      apply_rd_is_pc_check_ = false;
}

// BinaryRegisterImmediateTestTester
BinaryRegisterImmediateTestTester::BinaryRegisterImmediateTestTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool BinaryRegisterImmediateTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::BinaryRegisterImmediateTest expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.n.number(inst), inst.bits(19, 16));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.bits(11, 0));

  return true;
}

// Unary2RegisterOpTester
Unary2RegisterOpTester::Unary2RegisterOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary2RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(!Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.d.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.m.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Other NaCl constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary2RegisterOpTesterNotRdIsPcAndS
Unary2RegisterOpTesterNotRdIsPcAndS::Unary2RegisterOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}


bool Unary2RegisterOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst) == kRegisterPc) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary2RegisterOpTester::ApplySanityChecks(inst, decoder);
}

// Binary3RegisterOpTester
Binary3RegisterOpTester::Binary3RegisterOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary3RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.d.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.m.number(inst), inst.bits(11, 8));
  EXPECT_EQ(expected_decoder.n.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Other NaCl constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterOpTesterRegsNotPc
Binary3RegisterOpTesterRegsNotPc::Binary3RegisterOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}

bool Binary3RegisterOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOp expected_decoder;

  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.m.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.n.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary2RegisterImmedShiftedOpTester
Unary2RegisterImmedShiftedOpTester::Unary2RegisterImmedShiftedOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary2RegisterImmedShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterImmedShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.d.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.m.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.bits(11, 7));
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary2RegisterImmedShiftedOpTesterImm5NotZero
Unary2RegisterImmedShiftedOpTesterImm5NotZero::
Unary2RegisterImmedShiftedOpTesterImm5NotZero(
    const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}

bool Unary2RegisterImmedShiftedOpTesterImm5NotZero::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterImmedShiftedOp expected_decoder;

  // Check that we don't parse when imm5=0.
  if (0 == expected_decoder.imm.value(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder);
}

// Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS
Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}

bool Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterImmedShiftedOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst) == kRegisterPc) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder);
}

// Unary3RegisterShiftedOpTester
Unary3RegisterShiftedOpTester::Unary3RegisterShiftedOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary3RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary3RegisterShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.d.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.s.number(inst), inst.bits(11, 8));
  EXPECT_EQ(expected_decoder.m.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Check the shift type.
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary3RegisterShiftedOpTesterRegsNotPc
Unary3RegisterShiftedOpTesterRegsNotPc::Unary3RegisterShiftedOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Unary3RegisterShiftedOpTester(decoder) {}

bool Unary3RegisterShiftedOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary3RegisterShiftedOp expected_decoder;

  NC_PRECOND(Unary3RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.s.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.m.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterImmedShiftedOpTester
Binary3RegisterImmedShiftedOpTester::Binary3RegisterImmedShiftedOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary3RegisterImmedShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterImmedShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.n.number(inst), inst.bits(19, 16));
  EXPECT_EQ(expected_decoder.d.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.m.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.bits(11, 7));
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS
Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}

bool Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterImmedShiftedOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst) == kRegisterPc) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder);
}

// Binary4RegisterShiftedOpTester
Binary4RegisterShiftedOpTester::Binary4RegisterShiftedOpTester(
    const NamedClassDecoder& decoder)
      : Arm32DecoderTester(decoder) {}

bool Binary4RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.n.number(inst), inst.bits(19, 16));
  EXPECT_EQ(expected_decoder.d.number(inst), inst.bits(15, 12));
  EXPECT_EQ(expected_decoder.s.number(inst), inst.bits(11, 8));
  EXPECT_EQ(expected_decoder.m.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Other NaCl constraints about this instruction.
  EXPECT_NE(expected_decoder.d.number(inst), (uint32_t) 15)
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary4RegisterShiftedOpTesterRegsNotPc
Binary4RegisterShiftedOpTesterRegsNotPc::
Binary4RegisterShiftedOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
      : Binary4RegisterShiftedOpTester(decoder) {}

bool Binary4RegisterShiftedOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterShiftedOp expected_decoder;

  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_NE(expected_decoder.n.reg(inst), kRegisterPc)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.d.reg(inst), kRegisterPc)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.s.reg(inst), kRegisterPc)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.m.reg(inst), kRegisterPc)
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary2RegisterImmedShiftedTestTester
Binary2RegisterImmedShiftedTestTester::Binary2RegisterImmedShiftedTestTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary2RegisterImmedShiftedTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary2RegisterImmedShiftedTest expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.n.number(inst), inst.bits(19, 16));
  EXPECT_EQ(expected_decoder.m.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.bits(11, 7));
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.bits(6, 5));

  return true;
}

// Binary3RegisterShiftedTestTester
Binary3RegisterShiftedTestTester::Binary3RegisterShiftedTestTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary3RegisterShiftedTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterShiftedTest expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_EQ(expected_decoder.n.number(inst), inst.bits(19, 16));
  EXPECT_EQ(expected_decoder.s.number(inst), inst.bits(11, 8));
  EXPECT_EQ(expected_decoder.m.number(inst), inst.bits(3, 0));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst), kConditions);
  } else {
    EXPECT_EQ(expected_decoder.conditions.conds_if_updated(inst),
              kRegisterNone);
  }
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.bits(6, 5));

  return true;
}

// Binary3RegisterShiftedTestTesterRegsNotPc
Binary3RegisterShiftedTestTesterRegsNotPc::
Binary3RegisterShiftedTestTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}

bool Binary3RegisterShiftedTestTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterShiftedTest expected_decoder;

  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(
      inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_NE(expected_decoder.n.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.s.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();
  EXPECT_NE(expected_decoder.m.number(inst), (uint32_t) 15)
      << "Expected Unpredictable for " << InstContents();

  return true;
}

}  // namespace
