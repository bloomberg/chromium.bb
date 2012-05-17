/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/actual_vs_baseline.h"

#include "gtest/gtest.h"

namespace nacl_arm_test {

ActualVsBaselineTester::ActualVsBaselineTester(
    const NamedClassDecoder& actual,
    DecoderTester& baseline_tester)
    : Arm32DecoderTester(baseline_tester.ExpectedDecoder()),
      actual_(actual),
      baseline_(baseline_tester.ExpectedDecoder()),
      baseline_tester_(baseline_tester),
      actual_decoder_(actual_.named_decoder()),
      baseline_decoder_(baseline_.named_decoder()) {}

void ActualVsBaselineTester::ProcessMatch() {
  // First make sure the baseline is happy.
  baseline_tester_.InjectInstruction(inst_);
  baseline_tester_.ProcessMatch();

  // Check virtuals.
  CheckSafety();
  CheckDefs();
  CheckImmediateAddressingDefs();
  CheckBaseAddressRegister();
  CheckOffsetIsImmediate();
  CheckBranchTargetRegister();
  CheckIsRelativeBranch();
  CheckBranchTargetOffset();
  CheckIsLiteralPoolHead();
  CheckClearsBits();
  CheckSetsZIfBitsClear();
}

bool ActualVsBaselineTester::ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder) {
  UNREFERENCED_PARAMETER(inst);
  UNREFERENCED_PARAMETER(decoder);
  EXPECT_TRUE(false) << "Sanity Checks shouldn't be applied!";
  return false;
}

const NamedClassDecoder& ActualVsBaselineTester::ExpectedDecoder() const {
  return baseline_;
}

void ActualVsBaselineTester::CheckSafety() {
  // Note: We don't actually check if safety is identical. All we worry
  // about is that the validator (in sel_ldr) accepts/rejects the same
  // way in terms of safety. We don't worry if the reasons for safety
  // failing is the same.
  if (actual_decoder_.safety(inst_) == nacl_arm_dec::MAY_BE_SAFE) {
    EXPECT_EQ(baseline_decoder_.safety(inst_), nacl_arm_dec::MAY_BE_SAFE);
  } else {
    EXPECT_NE(baseline_decoder_.safety(inst_), nacl_arm_dec::MAY_BE_SAFE);
  }
}

void ActualVsBaselineTester::CheckDefs() {
  EXPECT_TRUE(
      baseline_decoder_.defs(inst_).Equals(actual_decoder_.defs(inst_)));
}

void ActualVsBaselineTester::CheckImmediateAddressingDefs() {
  EXPECT_TRUE(
      baseline_decoder_.immediate_addressing_defs(inst_).Equals(
          actual_decoder_.immediate_addressing_defs(inst_)));
}

void ActualVsBaselineTester::CheckBaseAddressRegister() {
  EXPECT_TRUE(
      baseline_decoder_.base_address_register(inst_).Equals(
          actual_decoder_.base_address_register(inst_)));
}

void ActualVsBaselineTester::CheckOffsetIsImmediate() {
  EXPECT_EQ(baseline_decoder_.offset_is_immediate(inst_),
            actual_decoder_.offset_is_immediate(inst_));
}

void ActualVsBaselineTester::CheckBranchTargetRegister() {
  EXPECT_TRUE(
      baseline_decoder_.branch_target_register(inst_).Equals(
          actual_decoder_.branch_target_register(inst_)));
}

void ActualVsBaselineTester::CheckIsRelativeBranch() {
  EXPECT_EQ(baseline_decoder_.is_relative_branch(inst_),
            actual_decoder_.is_relative_branch(inst_));
}

void ActualVsBaselineTester::CheckBranchTargetOffset() {
  EXPECT_EQ(baseline_decoder_.branch_target_offset(inst_),
            actual_decoder_.branch_target_offset(inst_));
}

void ActualVsBaselineTester::CheckIsLiteralPoolHead() {
  EXPECT_EQ(baseline_decoder_.is_literal_pool_head(inst_),
            actual_decoder_.is_literal_pool_head(inst_));
}

// Mask to use if code bundle size is 16.
static const uint32_t code_mask = 15;

// Mask to use if data block is 16 bytes.
static const uint32_t data16_mask = 0xFFFF;

// Mask to use if data block is 24 bytes.
static const uint32_t data24_mask = 0xFFFFFF;

void ActualVsBaselineTester::CheckClearsBits() {
  // Note: We don't actually test all combinations. We just try a few.

  // Assuming code bundle size is 16, see if we are the same.
  EXPECT_EQ(baseline_decoder_.clears_bits(inst_, code_mask),
            actual_decoder_.clears_bits(inst_, code_mask));

  // Assume data division size is 16 bytes.
  EXPECT_EQ(baseline_decoder_.clears_bits(inst_, data16_mask),
            actual_decoder_.clears_bits(inst_, data16_mask));

  // Assume data division size is 24 bytes.
  EXPECT_EQ(baseline_decoder_.clears_bits(inst_, data24_mask),
            actual_decoder_.clears_bits(inst_, data24_mask));
}

void ActualVsBaselineTester::CheckSetsZIfBitsClear() {
  // Note: We don't actually test all combinations of masks. We just
  // try a few.
  for (uint32_t i = 0; i < 15; ++i) {
    nacl_arm_dec::Register r(i);
    EXPECT_EQ(baseline_decoder_.sets_Z_if_bits_clear(inst_, r, data24_mask),
              actual_decoder_.sets_Z_if_bits_clear(inst_, r, data24_mask));
  }
}

}  // namespace
