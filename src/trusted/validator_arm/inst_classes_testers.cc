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

using nacl_arm_dec::Instruction;
using nacl_arm_dec::Register;

namespace nacl_arm_test {

// UncondDecoderTester
bool UncondDecoderTester::PassesParsePreconditions(
    Instruction inst,
    const NamedClassDecoder& decoder) {
  // Didn't parse undefined conditional.
  NC_PRECOND(cond_decoder_.cond.undefined(inst));
  return Arm32DecoderTester::PassesParsePreconditions(inst, decoder);
}

bool UncondDecoderTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that condition is defined correctly.
  EXPECT_EQ(cond_decoder_.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse defined conditional.
  if (cond_decoder_.cond.defined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  return Arm32DecoderTester::ApplySanityChecks(inst, decoder);
}

// UnsafeUncondDecoderTester
bool UnsafeUncondDecoderTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Apply ARM restriction -- I.e. we shouldn't be here. This is an
  // UNSAFE instruction.
  NC_EXPECT_FALSE_PRECOND(true);

  // Don't continue, we've already reported the root problem!
  return false;
}

// CondDecoderTester
CondDecoderTester::CondDecoderTester(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool CondDecoderTester::PassesParsePreconditions(
    Instruction inst,
    const NamedClassDecoder& decoder) {
  // Didn't parse undefined conditional.
  NC_PRECOND(cond_decoder_.cond.defined(inst));
  return Arm32DecoderTester::PassesParsePreconditions(inst, decoder);
}

bool CondDecoderTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that condition is defined correctly.
  EXPECT_EQ(cond_decoder_.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (cond_decoder_.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  return Arm32DecoderTester::ApplySanityChecks(inst, decoder);
}

// UnsafeCondDecoderTester
bool UnsafeCondDecoderTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Apply ARM restriction -- I.e. we shouldn't be here. This is an
  // UNSAFE instruction.
  NC_EXPECT_FALSE_PRECOND(true);

  // Don't continue, we've already reported the root problem!
  return false;
}

// CondVfpOpTester
CondVfpOpTester::CondVfpOpTester(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool CondVfpOpTester::ApplySanityChecks(Instruction inst,
                                        const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Apply ARM restrictions.
  EXPECT_EQ(expected_decoder_.coproc.value(inst), inst.Bits(11, 8));

  // Apply NaCl restrictions, i.e. that this is a VFP operation.
  EXPECT_TRUE(
      (expected_decoder_.coproc.value(inst) == static_cast<uint32_t>(10)) ||
      (expected_decoder_.coproc.value(inst) == static_cast<uint32_t>(11)));

  return true;
}

// CondAdvSIMDOpTester
CondAdvSIMDOpTester::CondAdvSIMDOpTester(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool CondAdvSIMDOpTester::ApplySanityChecks(Instruction inst,
                                        const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Apply ARM restrictions.
  EXPECT_EQ(expected_decoder_.coproc.value(inst), inst.Bits(11, 8));

  // Apply NaCl restrictions, i.e. that this is an advanced SIMD operation.
  EXPECT_TRUE(
      (expected_decoder_.coproc.value(inst) == static_cast<uint32_t>(10)) ||
      (expected_decoder_.coproc.value(inst) == static_cast<uint32_t>(11)));

  EXPECT_EQ(static_cast<uint32_t>(expected_decoder_.cond.value(inst)),
            static_cast<uint32_t>(Instruction::AL))
      << "Expected DEPRECATED for " << InstContents();

  return true;
}

// MoveImmediate12ToApsrTester
MoveImmediate12ToApsrTester::MoveImmediate12ToApsrTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool MoveImmediate12ToApsrTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check if mask and immediate values are correctly defined.
  EXPECT_EQ(expected_decoder_.mask.value(inst), inst.Bits(19, 18));
  EXPECT_EQ(expected_decoder_.imm12.value(inst), inst.Bits(11, 0));

  // Check that mask not zero.
  EXPECT_NE(static_cast<uint32_t>(0), expected_decoder_.mask.value(inst));

  // Check that mask tests match mask.
  EXPECT_EQ(expected_decoder_.mask.value(inst),
            ((static_cast<uint32_t>(
                expected_decoder_.UpdatesConditions(inst)) << 1) |
             static_cast<uint32_t>(expected_decoder_.UpdatesApsrGe(inst))));

  return true;
}

// Immediate16UseTester
Immediate16UseTester::Immediate16UseTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Immediate16UseTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check if immediate values are defined correctly.
  EXPECT_EQ(expected_decoder_.imm4.value(inst), inst.Bits(3, 0));
  EXPECT_EQ(expected_decoder_.imm12.value(inst), inst.Bits(19, 8));
  EXPECT_EQ(expected_decoder_.value(inst),
            ((inst.Bits(19, 8) << 4) | inst.Bits(3, 0)));

  return true;
}

// BranchImmediate24Tester
BranchImmediate24Tester::BranchImmediate24Tester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool BranchImmediate24Tester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check if immediate values are defined correctly.
  EXPECT_EQ(expected_decoder_.imm24.value(inst), inst.Bits(23, 0));

  return true;
}

// BranchToRegisterTester
BranchToRegisterTester::BranchToRegisterTester(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool BranchToRegisterTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check if mask and immediate values are correctly defined.
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.link_register.IsUpdated(inst), inst.Bit(5));

  return true;
}

// BranchToRegisterTesterRegsNotPc
BranchToRegisterTesterRegsNotPc::BranchToRegisterTesterRegsNotPc(
const NamedClassDecoder& decoder)
    : BranchToRegisterTester(decoder) {}

bool BranchToRegisterTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(BranchToRegisterTester::ApplySanityChecks(inst, decoder));

  // Check if register Rm is Pc.
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary1RegisterImmediateOp12Tester
bool Unary1RegisterImmediateOp12Tester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm12.value(inst), inst.Bits(11, 0));

  return true;
}

// Unary1RegisterImmediateOpTester
Unary1RegisterImmediateOpTester::Unary1RegisterImmediateOpTester(
    const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOp12Tester(decoder) {}

bool Unary1RegisterImmediateOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(Unary1RegisterImmediateOp12Tester::
             ApplySanityChecks(inst, decoder));

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm4.value(inst), inst.Bits(19, 16));
  EXPECT_EQ(expected_decoder_.ImmediateValue(inst),
            (inst.Bits(19, 16) << 12) | inst.Bits(11, 0));
  EXPECT_LT(expected_decoder_.ImmediateValue(inst), (uint32_t) 0x10000);

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary1RegisterImmediateOpPcTester
bool Unary1RegisterImmediateOpPcTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check that expected fields are properly defined.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.imm12.value(inst), inst.Bits(11, 0));

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
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary1RegisterImmediateOpTesterNotRdIsPcAndS
Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
Unary1RegisterImmediateOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}

bool Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  NC_PRECOND(!(expected_decoder_.d.reg(inst).Equals(Register::Pc()) &&
               expected_decoder_.conditions.is_updated(inst)));
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder_.d.reg(inst).Equals(Register::Pc())) &&
      expected_decoder_.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder);
}

// Unary1RegisterBitRangeMsbGeLsbTester
Unary1RegisterBitRangeMsbGeLsbTester::
Unary1RegisterBitRangeMsbGeLsbTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Unary1RegisterBitRangeMsbGeLsbTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check registers and flags used.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.lsb.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.msb.value(inst), inst.Bits(20, 16));
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
        << "Expected UNPREDICTABLE for " << InstContents();

  EXPECT_FALSE(expected_decoder_.msb.value(inst) <
               expected_decoder_.lsb.value(inst))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Binary2RegisterBitRangeMsbGeLsbTester
Binary2RegisterBitRangeMsbGeLsbTester::
Binary2RegisterBitRangeMsbGeLsbTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary2RegisterBitRangeMsbGeLsbTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check registers and flags used.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.lsb.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.msb.value(inst), inst.Bits(20, 16));
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
        << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
        << "Expected UNPREDICTABLE for " << InstContents();

  EXPECT_FALSE(expected_decoder_.msb.value(inst) <
               expected_decoder_.lsb.value(inst))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester
Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester::
Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.lsb.value(inst) +
               expected_decoder_.widthm1.value(inst) > 31)
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Binary2RegisterImmediateOpTester
Binary2RegisterImmediateOpTester::Binary2RegisterImmediateOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder), apply_rd_is_pc_check_(true) {}

bool Binary2RegisterImmediateOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 0));

  // Other NaCl constraints about this instruction.
  if (apply_rd_is_pc_check_) {
    EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
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
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  NC_PRECOND(!(expected_decoder_.d.reg(inst).Equals(Register::Pc()) &&
               expected_decoder_.conditions.is_updated(inst)));
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder_.d.reg(inst).Equals(Register::Pc())) &&
      expected_decoder_.conditions.is_updated(inst)) {
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
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15 and S=0.
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(Register::Pc()) &&
               !expected_decoder_.conditions.is_updated(inst)));
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15 and S=0.
  if ((expected_decoder_.n.reg(inst).Equals(Register::Pc())) &&
      !expected_decoder_.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      ApplySanityChecks(inst, decoder);
}

// BinaryRegisterImmediateTestTester
BinaryRegisterImmediateTestTester::BinaryRegisterImmediateTestTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool BinaryRegisterImmediateTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 0));

  return true;
}

// Unary2RegisterOpTester
Unary2RegisterOpTester::Unary2RegisterOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Unary2RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary2RegisterOpNotRmIsPcTester
Unary2RegisterOpNotRmIsPcTester::Unary2RegisterOpNotRmIsPcTester(
    const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}

bool Unary2RegisterOpNotRmIsPcTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary2RegisterOpTesterNotRdIsPcAndS
Unary2RegisterOpTesterNotRdIsPcAndS::Unary2RegisterOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}

bool Unary2RegisterOpTesterNotRdIsPcAndS::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  NC_PRECOND(!(expected_decoder_.d.reg(inst).Equals(Register::Pc()) &&
               expected_decoder_.conditions.is_updated(inst)));
  return Unary2RegisterOpTester::PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder_.d.reg(inst).Equals(Register::Pc())) &&
      expected_decoder_.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary2RegisterOpTester::ApplySanityChecks(inst, decoder);
}

// Unary2RegisterOpNotRmIsPcTesterRegsNotPc
bool Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpNotRmIsPcTester::ApplySanityChecks(inst, decoder));

  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary2RegisterShiftedOpTester
bool Unary2RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));
  EXPECT_EQ(expected_decoder_.imm5.value(inst), inst.Bits(11, 7));

  return true;
}

// Binary3RegisterShiftedOpTester
bool Binary3RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));

  return true;
}

// Binary3RegisterOpTester
Binary3RegisterOpTester::Binary3RegisterOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary3RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
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
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterOpTesterAltA
Binary3RegisterOpAltATester::Binary3RegisterOpAltATester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary3RegisterOpAltATester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(Register::None()));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterOpAltATesterRegsNotPc
Binary3RegisterOpAltATesterRegsNotPc::Binary3RegisterOpAltATesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}

bool Binary3RegisterOpAltATesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltATester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterOpTesterAltB
Binary3RegisterOpAltBNoCondUpdatesTester::
Binary3RegisterOpAltBNoCondUpdatesTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder), test_conditions_(false) {}

bool Binary3RegisterOpAltBNoCondUpdatesTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc
Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}

bool Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
             ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPcAndRegsNotPc
Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPcAndRegsNotPc::
Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPcAndRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}

bool Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPcAndRegsNotPc::
PassesParsePreconditions(
    Instruction inst,
    const NamedClassDecoder& decoder) {
  NC_PRECOND(!expected_decoder_.n.reg(inst).Equals(Register::Pc()));
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc
Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc::
Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}

bool Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc::
PassesParsePreconditions(
    Instruction inst,
    const NamedClassDecoder& decoder) {
  NC_PRECOND(!expected_decoder_.n.reg(inst).Equals(Register::Pc()));
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

// Binary4RegisterDualOpTester
Binary4RegisterDualOpTester::Binary4RegisterDualOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary4RegisterDualOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder_.a.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(Register::None()));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary4RegisterDualOpTesterRegsNotPc
Binary4RegisterDualOpTesterRegsNotPc::Binary4RegisterDualOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTester(decoder) {}

bool Binary4RegisterDualOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.a.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc
Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc::
Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}

bool Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when bits(15:12)=15.
  NC_PRECOND(!(expected_decoder_.a.reg(inst).Equals(Register::Pc())));
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when bits(15:12)=15.
  if (expected_decoder_.a.reg(inst).Equals(Register::Pc())) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }
  return Binary4RegisterDualOpTesterRegsNotPc::ApplySanityChecks(inst, decoder);
}

// Binary4RegisterDualResultTester
Binary4RegisterDualResultTester::Binary4RegisterDualResultTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary4RegisterDualResultTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.d_hi.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder_.d_lo.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(Register::None()));
  }

  // Arm constraint between RdHi and RdLo.
  EXPECT_FALSE(expected_decoder_.d_hi.reg(inst).
               Equals(expected_decoder_.d_lo.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d_lo.reg(inst).Equals(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d_hi.reg(inst).Equals(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary4RegisterDualResultTesterRegsNotPc
Binary4RegisterDualResultTesterRegsNotPc::
Binary4RegisterDualResultTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTester(decoder) {}

bool Binary4RegisterDualResultTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualResultTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d_hi.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d_lo.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// LoadExclusive2RegisterOpTester
LoadExclusive2RegisterOpTester::LoadExclusive2RegisterOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool LoadExclusive2RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// LoadExclusive2RegisterDoubleOpTester
LoadExclusive2RegisterDoubleOpTester::LoadExclusive2RegisterDoubleOpTester(
    const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}

bool LoadExclusive2RegisterDoubleOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadExclusive2RegisterOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.t.number(inst) + 1,
            expected_decoder_.t2.number(inst));

  // Other ARM constraints about this instruction.
  EXPECT_TRUE(expected_decoder_.t.IsEven(inst));
  EXPECT_FALSE(expected_decoder_.t2.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// LoadRegisterImm8OpTester
bool LoadRegisterImm8OpTester::ApplySanityChecks(
    Instruction inst, const NamedClassDecoder& decoder) {
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Test fields
  EXPECT_EQ(expected_decoder_.imm4L.value(inst), inst.Bits(3, 0));
  EXPECT_EQ(expected_decoder_.imm4H.value(inst), inst.Bits(11, 8));
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.writes.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder_.indexing.IsDefined(inst), inst.Bit(24));

  return true;
}

// LoadRegisterImm8DoubleOpTester
bool LoadRegisterImm8DoubleOpTester::ApplySanityChecks(
    Instruction inst, const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadRegisterImm8OpTester::ApplySanityChecks(inst, decoder));

  // Test fields.
  EXPECT_EQ(expected_decoder_.t.number(inst) + 1,
            expected_decoder_.t2.number(inst));

  // Other ARM constraints about this instruction.
  EXPECT_TRUE(expected_decoder_.t.IsEven(inst));
  EXPECT_FALSE(expected_decoder_.t2.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// LoadStore2RegisterImm8OpTester
LoadStore2RegisterImm8OpTester::LoadStore2RegisterImm8OpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder),
      // TODO(jfb) Assuming this is a load seems wrong.
      expected_decoder_(true) {}

bool LoadStore2RegisterImm8OpTester::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Should not parse if P=0 && W=1.
  NC_PRECOND(!(expected_decoder_.indexing.IsPostIndexing(inst) &&
               expected_decoder_.writes.IsDefined(inst)));
  return CondDecoderTester::PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8OpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Should not parse if P=0 && W=1.
  if (expected_decoder_.indexing.IsPostIndexing(inst) &&
      expected_decoder_.writes.IsDefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.writes.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder_.indexing.IsPreIndexing(inst), inst.Bit(24));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();

  EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
               (expected_decoder_.n.reg(inst).Equals(Register::Pc()) ||
                expected_decoder_.n.reg(inst).Equals(
                    expected_decoder_.t.reg(inst))))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// LoadStore2RegisterImm8OpTesterNotRnIsPc
LoadStore2RegisterImm8OpTesterNotRnIsPc::
LoadStore2RegisterImm8OpTesterNotRnIsPc(
    const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}

bool LoadStore2RegisterImm8OpTesterNotRnIsPc::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(Register::Pc())));
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8OpTesterNotRnIsPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  if (expected_decoder_.n.reg(inst).Equals(Register::Pc())) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }
  return LoadStore2RegisterImm8OpTester::ApplySanityChecks(inst, decoder);
}

// LoadStore2RegisterImm8DoubleOpTester
LoadStore2RegisterImm8DoubleOpTester::
LoadStore2RegisterImm8DoubleOpTester(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder),
      // TODO(jfb) Assuming this is a load seems wrong.
      expected_decoder_(true) {}

bool LoadStore2RegisterImm8DoubleOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm8OpTester::
             ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.t.number(inst) + 1,
            expected_decoder_.t2.number(inst));

  // Other ARM constraints about this instruction.
  EXPECT_TRUE(expected_decoder_.t.IsEven(inst));
  EXPECT_NE(expected_decoder_.t2.number(inst), static_cast<uint32_t>(15))
      << "Expected UNPREDICTABLE for " << InstContents();

  EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
               expected_decoder_.n.reg(inst).Equals(
                   expected_decoder_.t2.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc
LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc::
LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc(
    const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}

bool LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(Register::Pc())));
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()));

  return LoadStore2RegisterImm8DoubleOpTester::
      ApplySanityChecks(inst, decoder);
}

// PreloadRegisterImm12OpTester
bool PreloadRegisterImm12OpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.imm12.value(inst), inst.Bits(11, 0));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.read.IsDefined(inst), inst.Bit(22));
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));

  return true;
}

// PreloadRegisterPairOpTester
bool PreloadRegisterPairOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));
  EXPECT_EQ(expected_decoder_.imm5.value(inst), inst.Bits(11, 7));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.read.IsDefined(inst), inst.Bit(22));
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));

  // Check that we don't parse when Rm=15.
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()));

  return true;
}

// PreloadRegisterPairOpWAndRnNotPcTester
bool PreloadRegisterPairOpWAndRnNotPcTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that it passes base tests.
  NC_PRECOND(PreloadRegisterPairOpTester::
             ApplySanityChecks(inst, decoder));

  // Check that we don't parse when Rn=15 and R=0
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc())
               && !expected_decoder_.read.IsDefined(inst));

  return true;
}

// LoadStore2RegisterImm12OpTester
LoadStore2RegisterImm12OpTester::LoadStore2RegisterImm12OpTester(
    const NamedClassDecoder& decoder)
  : CondDecoderTester(decoder),
    // TODO(jfb) Assuming this is a load seems wrong.
    expected_decoder_(true) {}

bool LoadStore2RegisterImm12OpTester::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Should not parse if P=0 && W=1.
  NC_PRECOND(!(expected_decoder_.indexing.IsPostIndexing(inst) &&
               expected_decoder_.writes.IsDefined(inst)));
  return CondDecoderTester::PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Should not parse if P=0 && W=1.
  if (expected_decoder_.indexing.IsPostIndexing(inst) &&
      expected_decoder_.writes.IsDefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.imm12.value(inst), inst.Bits(11, 0));
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.writes.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder_.byte.IsDefined(inst), inst.Bit(22));
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder_.indexing.IsPreIndexing(inst), inst.Bit(24));

  // Other ARM constraints about this instruction.
  if (expected_decoder_.byte.IsDefined(inst)) {
    EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
        << "Expected UNPREDICTABLE for " << InstContents();
  }

  EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
               (expected_decoder_.n.reg(inst).Equals(Register::Pc()) ||
                expected_decoder_.n.reg(inst).Equals(
                    expected_decoder_.t.reg(inst))))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// LoadStore2RegisterImm12OpTesterNotRnIsPc
LoadStore2RegisterImm12OpTesterNotRnIsPc::
LoadStore2RegisterImm12OpTesterNotRnIsPc(
    const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}

bool LoadStore2RegisterImm12OpTesterNotRnIsPc::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(Register::Pc())));
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTesterNotRnIsPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  if (expected_decoder_.n.reg(inst).Equals(Register::Pc())) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }
  return LoadStore2RegisterImm12OpTester::ApplySanityChecks(inst, decoder);
}

// LoadStoreRegisterListTester
LoadStoreRegisterListTester::LoadStoreRegisterListTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool LoadStoreRegisterListTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.wback.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder_.register_list.value(inst), inst.Bits(15, 0));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_NE(static_cast<uint32_t>(0),
            expected_decoder_.register_list.value(inst))
      << "Expected UNPREDICTABLE for " << InstContents();
  if ((inst.Bit(20) == 0 /* store */) &&
      expected_decoder_.wback.IsDefined(inst) &&
      expected_decoder_.register_list.registers(inst).Contains(
          expected_decoder_.n.reg(inst))) {
    // Store multiple stores an unknown value if there's writeback and the
    // base register isn't at least the first register that is stored.
    for (nacl_arm_dec::Register::Number r = 0,
             n = expected_decoder_.n.reg(inst).number();
         r < n; ++r) {
      EXPECT_FALSE(expected_decoder_.register_list.value(inst) & (1u << r))
          << "Expected UNPREDICTABLE for " << InstContents();
    }
  }

  return true;
}

// LoadStoreVectorOpTester
LoadStoreVectorOpTester::LoadStoreVectorOpTester(
    const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}

bool LoadStoreVectorOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name (etc) is found.
  NC_PRECOND(CondVfpOpTester::ApplySanityChecks(inst, decoder));

  // Check registers and flags used.
  EXPECT_EQ(expected_decoder_.imm8.value(inst), inst.Bits(7, 0));
  EXPECT_TRUE(expected_decoder_.vd.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.wback.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder_.d_bit.value(inst), inst.Bits(22, 22));
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder_.indexing.IsDefined(inst), inst.Bit(24));

  return true;
}

// LoadStoreVectorRegisterListTester
LoadStoreVectorRegisterListTester::LoadStoreVectorRegisterListTester(
    const NamedClassDecoder& decoder)
    : LoadStoreVectorOpTester(decoder) {}

bool LoadStoreVectorRegisterListTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name (etc) is found.
  NC_PRECOND(LoadStoreVectorOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.wback.IsDefined(inst) &&
               (expected_decoder_.indexing.IsDefined(inst) ==
                expected_decoder_.direction.IsAdd(inst)));
  EXPECT_FALSE(expected_decoder_.wback.IsDefined(inst) &&
               expected_decoder_.n.reg(inst).Equals(Register::Pc()));
  EXPECT_NE(expected_decoder_.NumRegisters(inst), static_cast<uint32_t>(0))
      << "Expected UNPREDICTABLE for " << InstContents();
  if (expected_decoder_.coproc.value(inst) == 11 /* A1: 64-bit registers */) {
    EXPECT_TRUE(expected_decoder_.imm8.IsEven(inst))
        << "Expected UNPREDICTABLE for " << InstContents();
    EXPECT_LE(expected_decoder_.imm8.value(inst) / 2,
              static_cast<uint32_t>(16))
        << "Expected UNPREDICTABLE for " << InstContents();
    EXPECT_LE(expected_decoder_.FirstReg(inst).number()
              + expected_decoder_.imm8.value(inst) / 2,
              static_cast<uint32_t>(32))
        << "Expected UNPREDICTABLE for " << InstContents();
  } else {
    EXPECT_LE(expected_decoder_.FirstReg(inst).number()
              + expected_decoder_.imm8.value(inst),
              static_cast<uint32_t>(32))
        << "Expected UNPREDICTABLE for " << InstContents();
  }

  uint32_t puw =
      (static_cast<uint32_t>(expected_decoder_.indexing.IsDefined(inst)) << 2) +
      (static_cast<uint32_t>(expected_decoder_.direction.IsAdd(inst)) << 1) +
      static_cast<uint32_t>(expected_decoder_.wback.IsDefined(inst));
  bool valid_puw = (puw == 2) || (puw == 3) || (puw == 5);
  EXPECT_TRUE(valid_puw) << InstContents();

  return true;
}

// LoadStoreVectorRegisterListTesterNotRnIsSp
LoadStoreVectorRegisterListTesterNotRnIsSp::
LoadStoreVectorRegisterListTesterNotRnIsSp(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}

bool LoadStoreVectorRegisterListTesterNotRnIsSp::PassesParsePreconditions(
    Instruction inst,
    const NamedClassDecoder& decoder) {
  NC_PRECOND(!expected_decoder_.n.reg(inst).Equals(Register::Sp()));
  return LoadStoreVectorRegisterListTester::PassesParsePreconditions(
      inst, decoder);
}

// StoreVectorRegisterListTester
StoreVectorRegisterListTester::StoreVectorRegisterListTester(
    const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}

bool StoreVectorRegisterListTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name (etc) is found.
  NC_PRECOND(
      LoadStoreVectorRegisterListTester::ApplySanityChecks(inst, decoder));

  // Other constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()));

  return true;
}

// StoreVectorRegisterListTesterNotRnIsSp
StoreVectorRegisterListTesterNotRnIsSp::
StoreVectorRegisterListTesterNotRnIsSp(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}

bool StoreVectorRegisterListTesterNotRnIsSp::PassesParsePreconditions(
    Instruction inst,
    const NamedClassDecoder& decoder) {
  NC_PRECOND(!expected_decoder_.n.reg(inst).Equals(Register::Sp()));
  return StoreVectorRegisterListTester::PassesParsePreconditions(
      inst, decoder);
}

// StoreVectorRegisterTester
StoreVectorRegisterTester::StoreVectorRegisterTester(
    const NamedClassDecoder& decoder)
    : LoadStoreVectorOpTester(decoder) {}

bool StoreVectorRegisterTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name (etc) is found.
  NC_PRECOND(
      LoadStoreVectorOpTester::ApplySanityChecks(inst, decoder));

  // Other constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()));

  return true;
}

// LoadStore3RegisterOpTester
LoadStore3RegisterOpTester::LoadStore3RegisterOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder),
      // TODO(jfb) Assuming this is a load seems wrong.
      expected_decoder_(true) {}

bool LoadStore3RegisterOpTester::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Should not parse if P=0 && W=1.
  NC_PRECOND(!(expected_decoder_.indexing.IsPostIndexing(inst) &&
               expected_decoder_.writes.IsDefined(inst)));
  return CondDecoderTester::PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Should not parse if P=0 && W=1.
  if (expected_decoder_.indexing.IsPostIndexing(inst) &&
      expected_decoder_.writes.IsDefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.writes.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder_.indexing.IsPreIndexing(inst), inst.Bit(24));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();

  EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
               (expected_decoder_.n.reg(inst).Equals(Register::Pc()) ||
                expected_decoder_.n.reg(inst).Equals(
                    expected_decoder_.t.reg(inst))))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.indexing.IsPreIndexing(inst))
      << "Expected FORBIDDEN for " << InstContents();

  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// LoadStore3RegisterDoubleOpTester
LoadStore3RegisterDoubleOpTester::
LoadStore3RegisterDoubleOpTester(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder),
      // TODO(jfb) Assuming this is a load seems wrong.
      expected_decoder_(true) {}

bool LoadStore3RegisterDoubleOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.t.number(inst) + 1,
            expected_decoder_.t2.number(inst));

  // Other ARM constraints about this instruction.
  EXPECT_TRUE(expected_decoder_.t.IsEven(inst));
  EXPECT_NE(expected_decoder_.t2.number(inst), static_cast<uint32_t>(15))
      << "Expected UNPREDICTABLE for " << InstContents();

  EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
               expected_decoder_.n.reg(inst).Equals(
                   expected_decoder_.t2.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// StoreExclusive3RegisterOpTester
StoreExclusive3RegisterOpTester::StoreExclusive3RegisterOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool StoreExclusive3RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).
               Equals(expected_decoder_.n.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).
               Equals(expected_decoder_.t.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// StoreExclusive3RegisterDoubleOpTester
StoreExclusive3RegisterDoubleOpTester::StoreExclusive3RegisterDoubleOpTester(
    const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}

bool StoreExclusive3RegisterDoubleOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreExclusive3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.t.number(inst) + 1,
            expected_decoder_.t2.number(inst));

  // Other ARM constraints about this instruction.
  EXPECT_TRUE(expected_decoder_.t.IsEven(inst));
  EXPECT_FALSE(expected_decoder_.t2.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).
               Equals(expected_decoder_.t2.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// LoadStore3RegisterImm5OpTester
LoadStore3RegisterImm5OpTester::LoadStore3RegisterImm5OpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder),
      // TODO(jfb) Assuming this is a load seems wrong.
      expected_decoder_(true) {}

bool LoadStore3RegisterImm5OpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Should not parse if P=0 && W=1.
  if (expected_decoder_.indexing.IsPostIndexing(inst) &&
      expected_decoder_.writes.IsDefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.writes.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder_.byte.IsDefined(inst), inst.Bit(22));
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder_.indexing.IsPreIndexing(inst), inst.Bit(24));

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

  // Other ARM constraints about this instruction.
  if (expected_decoder_.byte.IsDefined(inst)) {
    EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
        << "Expected UNPREDICTABLE for " << InstContents();
  }

  EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
               (expected_decoder_.n.reg(inst).Equals(Register::Pc()) ||
                expected_decoder_.n.reg(inst).Equals(
                    expected_decoder_.t.reg(inst))))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary2RegisterImmedShiftedOpTester
Unary2RegisterImmedShiftedOpTester::Unary2RegisterImmedShiftedOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Unary2RegisterImmedShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Unary2RegisterSatImmedShiftedOpTester
bool Unary2RegisterSatImmedShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));

  // Check that immediate values are computed correctly.
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));
  EXPECT_EQ(expected_decoder_.imm5.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.sat_immed.value(inst), inst.Bits(20, 16));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Unary3RegisterShiftedOpTester
Unary3RegisterShiftedOpTester::Unary3RegisterShiftedOpTester(
    const NamedClassDecoder& decoder) : CondDecoderTester(decoder) {}

bool Unary3RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.s.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Check the shift type.
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
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
  NC_PRECOND(Unary3RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.s.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary4RegisterShiftedOpTester
Binary4RegisterShiftedOpTester::Binary4RegisterShiftedOpTester(
    const NamedClassDecoder& decoder)
      : CondDecoderTester(decoder) {}

bool Binary4RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.s.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
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
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.s.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary2RegisterImmedShiftedTestTester
Binary2RegisterImmedShiftedTestTester::Binary2RegisterImmedShiftedTestTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary2RegisterImmedShiftedTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

  return true;
}

// Binary3RegisterShiftedTestTester
Binary3RegisterShiftedTestTester::Binary3RegisterShiftedTestTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary3RegisterShiftedTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder_.s.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::Conditions()));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(Register::None()));
  }
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

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
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(
      inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.s.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// VectorUnary2RegisterOpBaseTester
bool VectorUnary2RegisterOpBaseTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.vm.number(inst), inst.Bits(3, 0));
  EXPECT_EQ(expected_decoder_.m.value(inst), inst.Bits(5, 5));
  EXPECT_EQ(expected_decoder_.vd.number(inst), inst.Bits(15, 12));
  EXPECT_EQ(expected_decoder_.d.value(inst), inst.Bits(22, 22));

  return true;
}

// Vector2RegisterMiscellaneousTester
bool Vector2RegisterMiscellaneousTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorUnary2RegisterOpBaseTester
             ::ApplySanityChecks(inst, decoder));

  // Check fields.
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.op.value(inst), inst.Bits(8, 7));
  EXPECT_EQ(expected_decoder_.f.IsDefined(inst), inst.Bit(10));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(19, 18));
  return true;
}

// Vector2RegisterMiscellaneous_I16_32_64NTester
bool Vector2RegisterMiscellaneous_I16_32_64NTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorUnary2RegisterOpBaseTester
             ::ApplySanityChecks(inst, decoder));

  // Check fields.
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.op.value(inst), inst.Bits(7, 6));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(19, 18));

  return true;
}

// Vector2RegisterMiscellaneous_CVT_H2STester
bool Vector2RegisterMiscellaneous_CVT_H2STester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorUnary2RegisterOpBaseTester
             ::ApplySanityChecks(inst, decoder));

  // Check fields.
  EXPECT_EQ(expected_decoder_.op.IsDefined(inst), inst.Bit(8));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(19, 18));

  return true;
}

// VectorUnary2RegisterDupTester
bool VectorUnary2RegisterDupTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorUnary2RegisterOpBaseTester
             ::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.imm4.value(inst), inst.Bits(19, 16));

  return true;
}

// VectorBinary3RegisterOpBaseTester
bool VectorBinary3RegisterOpBaseTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.vm.number(inst), inst.Bits(3, 0));
  EXPECT_EQ(expected_decoder_.m.value(inst), inst.Bits(5, 5));
  EXPECT_EQ(expected_decoder_.n.value(inst), inst.Bits(7, 7));
  EXPECT_EQ(expected_decoder_.vd.number(inst), inst.Bits(15, 12));
  EXPECT_EQ(expected_decoder_.vn.number(inst), inst.Bits(19, 16));
  EXPECT_EQ(expected_decoder_.d.value(inst), inst.Bits(22, 22));

  return true;
}

// VectorBinary2RegisterShiftAmountTester
bool VectorBinary2RegisterShiftAmountTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(VectorBinary3RegisterOpBaseTester::
             ApplySanityChecks(inst, decoder));

  // Check additional fields not checked in base class.
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.l.value(inst), inst.Bits(7, 7));
  EXPECT_EQ(expected_decoder_.op.IsDefined(inst), inst.Bit(8));
  EXPECT_EQ(expected_decoder_.imm6.value(inst), inst.Bits(21, 16));
  EXPECT_EQ(expected_decoder_.u.IsDefined(inst), inst.Bit(24));

  return true;
}

// VectorBinary3RegisterSameLengthTester
bool VectorBinary3RegisterSameLengthTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(VectorBinary3RegisterOpBaseTester::
             ApplySanityChecks(inst, decoder));

  // Check additional fields not checked in base class.
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.op.IsDefined(inst), inst.Bit(9));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(21, 20));
  EXPECT_EQ(expected_decoder_.u.IsDefined(inst), inst.Bit(24));

  return true;
}

bool VectorBinary3RegisterDifferentLengthTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(VectorBinary3RegisterOpBaseTester::
             ApplySanityChecks(inst, decoder));

  // Check additional fields.
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.op.IsDefined(inst), inst.Bit(8));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(21, 20));
  EXPECT_EQ(expected_decoder_.u.IsDefined(inst), inst.Bit(24));

  return true;
}

// VectorBinary2RegisterScalarTester
bool VectorBinary2RegisterScalarTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(VectorBinary3RegisterOpBaseTester::
             ApplySanityChecks(inst, decoder));

  // Check additional fields not checked in base class.
  EXPECT_EQ(expected_decoder_.f.IsDefined(inst), inst.Bit(8));
  EXPECT_EQ(expected_decoder_.op.IsDefined(inst), inst.Bit(10));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(21, 20));
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(24));

  return true;
}

// VectorBinary3RegisterImmOpTester
bool VectorBinary3RegisterImmOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterOpBaseTester
             ::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 8));

  return true;
}

// Vector1RegisterImmediateTester
bool Vector1RegisterImmediateTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.imm4.value(inst), inst.Bits(3, 0));
  EXPECT_EQ(expected_decoder_.op.IsDefined(inst), inst.Bit(5));
  EXPECT_EQ(expected_decoder_.q.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.cmode.value(inst), inst.Bits(11, 8));
  EXPECT_EQ(expected_decoder_.vd.number(inst), inst.Bits(15, 12));
  EXPECT_EQ(expected_decoder_.imm3.value(inst), inst.Bits(18, 16));
  EXPECT_EQ(expected_decoder_.d.value(inst), inst.Bits(22, 22));
  EXPECT_EQ(expected_decoder_.i.value(inst), inst.Bits(24, 24));

  return true;
}

// VectorBinary3RegisterLookupOpTester
bool VectorBinary3RegisterLookupOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterOpBaseTester
             ::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.op_flag.IsDefined(inst), inst.Bit(6));
  EXPECT_EQ(expected_decoder_.len.value(inst), inst.Bits(9, 8));

  return true;
}

// VfpUsesRegOpTester
bool VfpUsesRegOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// VfpMrsOpTester
bool VfpMrsOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));

  return true;
}

// MoveVfpRegisterOpTester
bool MoveVfpRegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.to_arm_reg.IsDefined(inst), inst.Bit(20));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// MoveVfpRegisterOpWithTypeSelTester
bool MoveVfpRegisterOpWithTypeSelTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveVfpRegisterOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.opc1.value(inst), inst.Bits(23, 21));
  EXPECT_EQ(expected_decoder_.opc2.value(inst), inst.Bits(6, 5));

  return true;
}

// MoveDoubleVfpRegisterOpTester
bool MoveDoubleVfpRegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveVfpRegisterOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder_.t2.reg(inst).Equals(inst.Reg(19, 16)));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.t2.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.IsSinglePrecision(inst)
               && (((expected_decoder_.vm.reg(inst).number() << 1) |
                    expected_decoder_.m.value(inst)) == 31))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.to_arm_reg.IsDefined(inst) &&
               expected_decoder_.t.reg(inst).Equals(
                   expected_decoder_.t2.reg(inst)))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// DuplicateToAdvSIMDRegistersTester
bool DuplicateToAdvSIMDRegistersTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(CondAdvSIMDOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_EQ(expected_decoder_.e.value(inst), inst.Bits(5, 5));
  EXPECT_TRUE(expected_decoder_.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.vd.number(inst), inst.Bits(19, 16));
  EXPECT_EQ(expected_decoder_.is_two_regs.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder_.b.value(inst), inst.Bits(22, 22));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(Register::Pc()))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.is_two_regs.IsDefined(inst)
               && !expected_decoder_.vd.IsEven(inst));
  EXPECT_NE(expected_decoder_.be_value(inst),
            static_cast<uint32_t>(0x3));

  return true;
}

// VectorLoadStoreMultipleTester
bool VectorLoadStoreMultipleTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check fields and registers.
  EXPECT_TRUE(expected_decoder_.rm.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.align.value(inst), inst.Bits(5, 4));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(7, 6));
  EXPECT_EQ(expected_decoder_.type.value(inst), inst.Bits(11, 8));
  EXPECT_EQ(expected_decoder_.vd.number(inst), inst.Bits(15, 12));
  EXPECT_TRUE(expected_decoder_.rn.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.d.value(inst), inst.Bits(22, 22));

  return true;
}

// VectorLoadStoreSingleTester
bool VectorLoadStoreSingleTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check fields and registers.
  EXPECT_TRUE(expected_decoder_.rm.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.index_align.value(inst), inst.Bits(7, 4));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(11, 10));
  EXPECT_EQ(expected_decoder_.vd.number(inst), inst.Bits(15, 12));
  EXPECT_TRUE(expected_decoder_.rn.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.d.value(inst), inst.Bits(22, 22));

  return true;
}

// VectorLoadSingleAllLanesTester
bool VectorLoadSingleAllLanesTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check fields and registers.
  EXPECT_TRUE(expected_decoder_.rm.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.a.IsDefined(inst), inst.Bit(4));
  EXPECT_EQ(expected_decoder_.t.IsDefined(inst), inst.Bit(5));
  EXPECT_EQ(expected_decoder_.size.value(inst), inst.Bits(7, 6));
  EXPECT_EQ(expected_decoder_.vd.number(inst), inst.Bits(15, 12));
  EXPECT_TRUE(expected_decoder_.rn.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.d.value(inst), inst.Bits(22, 22));

  return true;
}

// BarrierInstTester
bool BarrierInstTester::ApplySanityChecks(Instruction inst,
                                          const NamedClassDecoder& decoder) {
  NC_PRECOND(UncondDecoderTester::ApplySanityChecks(inst, decoder));
  EXPECT_EQ(expected_decoder_.option.value(inst), inst.Bits(3, 0));

  return true;
}

// DataBarrierTester
bool DataBarrierTester::ApplySanityChecks(Instruction inst,
                                          const NamedClassDecoder& decoder) {
  NC_PRECOND(BarrierInstTester::ApplySanityChecks(inst, decoder));
  EXPECT_EQ(expected_decoder_.option.value(inst), inst.Bits(3, 0));

  switch (expected_decoder_.option.value(inst)) {
    case 0xF:  // 1111
    case 0xE:  // 1110
    case 0xB:  // 1011
    case 0xA:  // 1010
    case 0x7:  // 0111
    case 0x6:  // 0110
    case 0x3:  // 0011
    case 0x2:  // 0010
      break;
    default:
      EXPECT_FALSE(true)
          << "Expected forbidden operands for option" << InstContents();
  }
  return true;
}

// InstructionBarrierTester
bool InstructionBarrierTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(BarrierInstTester::ApplySanityChecks(inst, decoder));

  EXPECT_EQ(static_cast<uint32_t>(0xF), expected_decoder_.option.value(inst))
      << "Expected forbidden operands for option" << InstContents();

  return true;
}

// PermanentlyUndefinedTester
bool PermanentlyUndefinedTester::ApplySanityChecks(Instruction inst,
                                        const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  return true;
}

}  // namespace nacl_arm_test
