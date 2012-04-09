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
#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"

namespace {

#ifdef __BIG_ENDIAN__
  #error This test will only succeed on a little-endian machine.  Sorry.
#endif

// Defines a gtest testing harness for testing Arm32 instructions.
class Arm32InstructionTests : public ::testing::Test {
 protected:
  Arm32InstructionTests() {}
};

// ***************************************************************************
// Add (register-shifted register):
//
// ADD(S)<c> <Rd>, <Rn>, <Rm>,  <type> <Rs>
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |  cond  | 0 0 0 0 1 0 0| S|   Rn   |   Rd   |   Rs   | 0|type| 1|   Rm   |
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// ***************************************************************************
TEST_F(Arm32InstructionTests, Add_A8_6_7_A1_A8_26) {
  nacl_arm_test::Binary4RegisterShiftedOpTester tester;
  tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

}  // namespace

// Test driver function.
int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
