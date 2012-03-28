/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Instruction decoding testing for each defined ARM (32-bit) instruction.
 *
 * All instructions are based on "ARM Architecture Reference Manual
 * ARM*v7-A and ARM*v7-R edition, Errata markup".
 *
 * Instructions are identifier using an ID of the form:
 *
 *     OP_SECTION_FORM_PAGE
 *
 * where:
 *    OP = The name of the operator.
 *    SECTION is the section that defines the instruction.
 *    FORM is which form on that page (i.e. A1, A2, ...),
 *    PAGE is the manual page the instruction is defined on.
 *
 * Classes that implement sanity tests for an instruction is named using the
 * instruction ID (see above) followed by the suffix _Tester. The corresponding
 * test function is named using the instruction ID (unless there are problems,
 * in which case the ID is prefixed with DISABLED_).
 */

#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/decoder_tester.h"
#include "native_client/src/trusted/validator_arm/inst_classes.h"

using nacl_arm_dec::Arm32DecoderTester;
using nacl_arm_dec::ClassDecoder;
using nacl_arm_dec::Binary4RegisterShiftedOp;
using nacl_arm_dec::Instruction;

namespace {

#ifdef __BIG_ENDIAN__
  #error This test will only succeed on a little-endian machine.  Sorry.
#endif

// Defines a gtest testing harness for testing Arm32 instructions.
class Arm32InstructionTests : public ::testing::Test {
 protected:
  Arm32InstructionTests() {}
};

/****************************************************************************
 * Add (register-shifted register):
 *
 * ADD(S)<c> <Rd>, <Rn>, <Rm>,  <type> <Rs>
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * |  cond  | 0 0 0 0 1 0 0| S|   Rn   |   Rd   |   Rs   | 0|type| 1|   Rm   |
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 *****************************************************************************
 */

class Add_A8_6_7_A1_A8_26_Tester : public Arm32DecoderTester {
 public:
  virtual void ApplySanityChecks(Instruction inst,
                                 const ClassDecoder& decoder) {
    UNREFERENCED_PARAMETER(decoder);
    // Don't test unconditionals.
    if (inst.bits(31, 28) == 0xF) return;
    // Check if expected class name found.
    Arm32DecoderTester::ApplySanityChecks(inst, decoder);
    // Check Registers and flags used in DataProc.
    EXPECT_EQ(expected_decoder_.Rn(inst).number(), inst.bits(19, 16));
    EXPECT_EQ(expected_decoder_.Rd(inst).number(), inst.bits(15, 12));
    EXPECT_EQ(expected_decoder_.Rs(inst).number(), inst.bits(11, 8));
    EXPECT_EQ(expected_decoder_.Rm(inst).number(), inst.bits(3, 0));
    EXPECT_EQ(expected_decoder_.UpdatesFlagsRegister(inst), inst.bit(20));
    // Other ARM constraints about this instruction.
    EXPECT_NE(expected_decoder_.Rn(inst).number(), (uint32_t) 15)
        << "Expected Unpredictable for " << InstContents();
    EXPECT_NE(expected_decoder_.Rd(inst).number(), (uint32_t) 15)
        << "Expected Unpredictable for " << InstContents();
    EXPECT_NE(expected_decoder_.Rs(inst).number(), (uint32_t) 15)
        << "Expected Unpredictable for " << InstContents();
    EXPECT_NE(expected_decoder_.Rm(inst).number(), (uint32_t) 15)
        << "Expected Unpredictable for " << InstContents();
  }

 private:
  Binary4RegisterShiftedOp expected_decoder_;
};

TEST_F(Arm32InstructionTests, DISABLE_Add_A8_6_7_A1_A8_26) {
  Add_A8_6_7_A1_A8_26_Tester tester;
  tester.Test("cccc0000100snnnnddddssss0tt1mmmm", "Binary4RegisterShiftedOp");
}

}  // namespace

// Test driver function.
int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
