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

// UnsafeClassDecoderTester
UnsafeCondNopTester::UnsafeCondNopTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder),
      expected_decoder_(nacl_arm_dec::UNKNOWN) {}

bool UnsafeCondNopTester::
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

// CondNopTester
CondNopTester::CondNopTester(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

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

// Unary1RegisterSetTester
Unary1RegisterSetTester::Unary1RegisterSetTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Unary1RegisterSetTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check that registers are correctly defined.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.read_spsr.IsDefined(inst), inst.Bit(22));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.read_spsr.IsDefined(inst))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary1RegisterUseTester
Unary1RegisterUseTester::Unary1RegisterUseTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Unary1RegisterUseTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check that registers are correctly defined.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.mask.value(inst), inst.Bits(19, 18));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  switch (expected_decoder_.mask.value(inst)) {
    case 0:
    case 1:
      EXPECT_FALSE(decoder.defs(inst).Contains(kConditions));
      break;
    case 2:
    case 3:
      EXPECT_TRUE(decoder.defs(inst).Contains(kConditions));
      break;

    default:
      // This should NOT happen.
      EXPECT_FALSE(true);
      break;
  }

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
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary1RegisterImmediateOpTester
Unary1RegisterImmediateOpTester::Unary1RegisterImmediateOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Unary1RegisterImmediateOpTester::
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm4.value(inst), inst.Bits(19, 16));
  EXPECT_EQ(expected_decoder_.imm12.value(inst), inst.Bits(11, 0));
  EXPECT_EQ(expected_decoder_.ImmediateValue(inst),
            (inst.Bits(19, 16) << 12) | inst.Bits(11, 0));
  EXPECT_LT(expected_decoder_.ImmediateValue(inst), (uint32_t) 0x10000);

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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
  NC_PRECOND(!(expected_decoder_.d.reg(inst).Equals(kRegisterPc) &&
               expected_decoder_.conditions.is_updated(inst)));
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder_.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder_.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder);
}

// Unary1RegisterBitRangeTester
Unary1RegisterBitRangeTester::Unary1RegisterBitRangeTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Unary1RegisterBitRangeTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check registers and flags used.
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.lsb.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.msb.value(inst), inst.Bits(20, 16));
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
        << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Binary2RegisterBitRangeTester
Binary2RegisterBitRangeTester::Binary2RegisterBitRangeTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary2RegisterBitRangeTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check registers and flags used.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder_.lsb.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.imm5.value(inst), inst.Bits(20, 16));
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
        << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Binary2RegisterBitRangeNotRnIsPcTester
Binary2RegisterBitRangeNotRnIsPcTester::Binary2RegisterBitRangeNotRnIsPcTester(
    const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeTester(decoder) {}

bool Binary2RegisterBitRangeNotRnIsPcTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterBitRangeTester::ApplySanityChecks(inst, decoder));

  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
        << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Binary2RegisterBitRangeTesterNotRnIsPc
Binary2RegisterBitRangeTesterNotRnIsPc::Binary2RegisterBitRangeTesterNotRnIsPc(
    const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcTester(decoder) {}

bool Binary2RegisterBitRangeTesterNotRnIsPc::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(kRegisterPc)));
  return Binary2RegisterBitRangeTester::PassesParsePreconditions(inst, decoder);
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 0));

  // Other NaCl constraints about this instruction.
  if (apply_rd_is_pc_check_) {
    EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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
  NC_PRECOND(!(expected_decoder_.d.reg(inst).Equals(kRegisterPc) &&
               expected_decoder_.conditions.is_updated(inst)));
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder_.d.reg(inst).Equals(kRegisterPc)) &&
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
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(kRegisterPc) &&
               !expected_decoder_.conditions.is_updated(inst)));
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15 and S=0.
  if ((expected_decoder_.n.reg(inst).Equals(kRegisterPc)) &&
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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

  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
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
  NC_PRECOND(!(expected_decoder_.d.reg(inst).Equals(kRegisterPc) &&
               expected_decoder_.conditions.is_updated(inst)));
  return Unary2RegisterOpTester::PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder_.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder_.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary2RegisterOpTester::ApplySanityChecks(inst, decoder);
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
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
                Equals(kConditions));
  } else {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterOpTesterAltB
Binary3RegisterOpAltBTester::Binary3RegisterOpAltBTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder), test_conditions_(true) {}

bool Binary3RegisterOpAltBTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (test_conditions_) {
    if (expected_decoder_.conditions.is_updated(inst)) {
      EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                  Equals(kConditions));
    } else {
      EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                  Equals(kRegisterNone));
    }
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterOpAltBTesterRegsNotPc
Binary3RegisterOpAltBTesterRegsNotPc::Binary3RegisterOpAltBTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTester(decoder) {}

bool Binary3RegisterOpAltBTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterOpAltBNoCondUpdatesTester
Binary3RegisterOpAltBNoCondUpdatesTester::
Binary3RegisterOpAltBNoCondUpdatesTester(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTester(decoder) {
  test_conditions_ = false;
}

// Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc
Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
             ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
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
                Equals(kConditions));
  } else {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.a.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
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
  NC_PRECOND(!(expected_decoder_.a.reg(inst).Equals(kRegisterPc)));
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when bits(15:12)=15.
  if (expected_decoder_.a.reg(inst).Equals(kRegisterPc)) {
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
                Equals(kConditions));
  } else {
    EXPECT_TRUE(expected_decoder_.conditions.conds_if_updated(inst).
                Equals(kRegisterNone));
  }

  // Arm constraint between RdHi and RdLo.
  EXPECT_FALSE(expected_decoder_.d_hi.reg(inst).
               Equals(expected_decoder_.d_lo.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d_lo.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d_hi.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.d_hi.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d_lo.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.t2.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// LoadStore2RegisterImm8OpTester
LoadStore2RegisterImm8OpTester::LoadStore2RegisterImm8OpTester(
    const NamedClassDecoder& decoder) : CondDecoderTester(decoder) {}

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
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for loads and store.
  // TODO(karl): Should we not allow this?
  // EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
  //              (expected_decoder_.n.reg(inst).Equals(kRegisterPc) ||
  //               expected_decoder_.n.reg(inst).Equals(
  //                   expected_decoder_.t.reg(inst))))
  //     << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(kRegisterPc))
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
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(kRegisterPc)));
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8OpTesterNotRnIsPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  if (expected_decoder_.n.reg(inst).Equals(kRegisterPc)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }
  return LoadStore2RegisterImm8OpTester::ApplySanityChecks(inst, decoder);
}

// LoadStore2RegisterImm8DoubleOpTester
LoadStore2RegisterImm8DoubleOpTester::
LoadStore2RegisterImm8DoubleOpTester(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}

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

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for loads and store.
  // TODO(karl): Should we not allow this?
  // EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
  //              expected_decoder_.n.reg(inst).Equals(
  //                  expected_decoder_.t2.reg(inst)))
  //     << "Expected UNPREDICTABLE for " << InstContents();

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
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(kRegisterPc)));
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  if (expected_decoder_.n.reg(inst).Equals(kRegisterPc)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return LoadStore2RegisterImm8DoubleOpTester::
      ApplySanityChecks(inst, decoder);
}

// LoadStore2RegisterImm12OpTester
LoadStore2RegisterImm12OpTester::LoadStore2RegisterImm12OpTester(
    const NamedClassDecoder& decoder) : CondDecoderTester(decoder) {}

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
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder_.indexing.IsPreIndexing(inst), inst.Bit(24));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for loads and store.
  // TODO(karl): Should we not allow this?
  // EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
  //              (expected_decoder_.n.reg(inst).Equals(kRegisterPc) ||
  //               expected_decoder_.n.reg(inst).Equals(
  //                   expected_decoder_.t.reg(inst))))
  //     << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(kRegisterPc))
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
  NC_PRECOND(!(expected_decoder_.n.reg(inst).Equals(kRegisterPc)));
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTesterNotRnIsPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rn=15.
  if (expected_decoder_.n.reg(inst).Equals(kRegisterPc)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }
  return LoadStore2RegisterImm12OpTester::ApplySanityChecks(inst, decoder);
}

// LoadStore3RegisterOpTester
LoadStore3RegisterOpTester::LoadStore3RegisterOpTester(
    const NamedClassDecoder& decoder) : CondDecoderTester(decoder) {}

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
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for loads and stores.
  // TODO(karl): Should we not allow this?
  // EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
  //              (expected_decoder_.n.reg(inst).Equals(kRegisterPc) ||
  //               expected_decoder_.n.reg(inst).Equals(
  //                   expected_decoder_.t.reg(inst))))
  //     << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.indexing.IsPreIndexing(inst))
      << "Expected FORBIDDEN for " << InstContents();

  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// LoadStore3RegisterDoubleOpTester
LoadStore3RegisterDoubleOpTester::
LoadStore3RegisterDoubleOpTester(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {
}

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

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for loads and stores.
  // TODO(karl): Should we not allow this?
  // EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
  //              expected_decoder_.n.reg(inst).Equals(
  //                  expected_decoder_.t2.reg(inst)))
  //     << "Expected UNPREDICTABLE for " << InstContents();

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
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.t2.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).
               Equals(expected_decoder_.t2.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// LoadStore3RegisterImm5OpTester
LoadStore3RegisterImm5OpTester::LoadStore3RegisterImm5OpTester(
    const NamedClassDecoder& decoder) : CondDecoderTester(decoder) {}

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
  EXPECT_EQ(expected_decoder_.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder_.indexing.IsPreIndexing(inst), inst.Bit(24));

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.t.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for loads and store.
  // TODO(karl): Should we not allow this?
  // EXPECT_FALSE(expected_decoder_.HasWriteBack(inst) &&
  //              (expected_decoder_.n.reg(inst).Equals(kRegisterPc) ||
  //               expected_decoder_.n.reg(inst).Equals(
  //                   expected_decoder_.t.reg(inst))))
  //     << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(kRegisterPc))
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary2RegisterImmedShiftedOpTesterImm5NotZero
Unary2RegisterImmedShiftedOpTesterImm5NotZero::
Unary2RegisterImmedShiftedOpTesterImm5NotZero(
    const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}

bool Unary2RegisterImmedShiftedOpTesterImm5NotZero::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when imm5=0.
  NC_PRECOND(0 != expected_decoder_.imm.value(inst));
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterImm5NotZero::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when imm5=0.
  if (0 == expected_decoder_.imm.value(inst)) {
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
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  NC_PRECOND(!(expected_decoder_.d.reg(inst).Equals(kRegisterPc) &&
               expected_decoder_.conditions.is_updated(inst)));
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder_.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder_.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }
  return Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder);
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check the shift type.
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.s.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterImmedShiftedOpTester
Binary3RegisterImmedShiftedOpTester::Binary3RegisterImmedShiftedOpTester(
    const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}

bool Binary3RegisterImmedShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check if expected class name found.
  NC_PRECOND(CondDecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder_.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder_.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder_.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder_.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder_.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder_.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder_.shift_type.value(inst), inst.Bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS
Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}

bool Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
PassesParsePreconditions(Instruction inst,
                         const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  NC_PRECOND(!(expected_decoder_.d.reg(inst).Equals(kRegisterPc) &&
               expected_decoder_.conditions.is_updated(inst)));
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder_.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder_.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }
  return Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder);
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
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
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.s.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
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
        Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder_.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
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
  EXPECT_FALSE(expected_decoder_.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.s.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder_.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

}  // namespace
