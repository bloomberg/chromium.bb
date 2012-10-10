/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif


#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/actual_vs_baseline.h"
#include "native_client/src/trusted/validator_arm/actual_classes.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"
#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"

namespace nacl_arm_test {

// The following classes are derived class decoder testers that
// add row pattern constraints and decoder restrictions to each tester.
// This is done so that it can be used to make sure that the
// corresponding pattern is not tested for cases that would be excluded
//  due to row checks, or restrictions specified by the row restrictions.


// Neutral case:
// inst(8)=0 & inst(7:4)=00x1
//    = {baseline: 'MoveDoubleVfpRegisterOp',
//       constraints: }
//
// Representaive case:
// C(8)=0 & op(7:4)=00x1
//    = {baseline: MoveDoubleVfpRegisterOp,
//       constraints: }
class MoveDoubleVfpRegisterOpTesterCase0
    : public MoveDoubleVfpRegisterOpTester {
 public:
  MoveDoubleVfpRegisterOpTesterCase0(const NamedClassDecoder& decoder)
    : MoveDoubleVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveDoubleVfpRegisterOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x000000D0) != 0x00000010 /* op(7:4)=~00x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveDoubleVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(8)=1 & inst(7:4)=00x1
//    = {baseline: 'MoveDoubleVfpRegisterOp',
//       constraints: }
//
// Representaive case:
// C(8)=1 & op(7:4)=00x1
//    = {baseline: MoveDoubleVfpRegisterOp,
//       constraints: }
class MoveDoubleVfpRegisterOpTesterCase1
    : public MoveDoubleVfpRegisterOpTester {
 public:
  MoveDoubleVfpRegisterOpTesterCase1(const NamedClassDecoder& decoder)
    : MoveDoubleVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveDoubleVfpRegisterOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8)=~1 */) return false;
  if ((inst.Bits() & 0x000000D0) != 0x00000010 /* op(7:4)=~00x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveDoubleVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(8)=0 & inst(7:4)=00x1
//    = {baseline: 'MoveDoubleVfpRegisterOp',
//       constraints: ,
//       rule: 'Vmov_two_S_Rule_A1'}
//
// Representative case:
// C(8)=0 & op(7:4)=00x1
//    = {baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       rule: Vmov_two_S_Rule_A1}
class MoveDoubleVfpRegisterOpTester_Case0
    : public MoveDoubleVfpRegisterOpTesterCase0 {
 public:
  MoveDoubleVfpRegisterOpTester_Case0()
    : MoveDoubleVfpRegisterOpTesterCase0(
      state_.MoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(8)=1 & inst(7:4)=00x1
//    = {baseline: 'MoveDoubleVfpRegisterOp',
//       constraints: ,
//       rule: 'Vmov_one_D_Rule_A1'}
//
// Representative case:
// C(8)=1 & op(7:4)=00x1
//    = {baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       rule: Vmov_one_D_Rule_A1}
class MoveDoubleVfpRegisterOpTester_Case1
    : public MoveDoubleVfpRegisterOpTesterCase1 {
 public:
  MoveDoubleVfpRegisterOpTester_Case1()
    : MoveDoubleVfpRegisterOpTesterCase1(
      state_.MoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// Neutral case:
// inst(8)=0 & inst(7:4)=00x1
//    = {actual: 'MoveDoubleVfpRegisterOp',
//       baseline: 'MoveDoubleVfpRegisterOp',
//       constraints: ,
//       pattern: 'cccc1100010otttttttt101000m1mmmm',
//       rule: 'Vmov_two_S_Rule_A1'}
//
// Representaive case:
// C(8)=0 & op(7:4)=00x1
//    = {actual: MoveDoubleVfpRegisterOp,
//       baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       pattern: cccc1100010otttttttt101000m1mmmm,
//       rule: Vmov_two_S_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       MoveDoubleVfpRegisterOpTester_Case0_TestCase0) {
  MoveDoubleVfpRegisterOpTester_Case0 tester;
  tester.Test("cccc1100010otttttttt101000m1mmmm");
}

// Neutral case:
// inst(8)=1 & inst(7:4)=00x1
//    = {actual: 'MoveDoubleVfpRegisterOp',
//       baseline: 'MoveDoubleVfpRegisterOp',
//       constraints: ,
//       pattern: 'cccc1100010otttttttt101100m1mmmm',
//       rule: 'Vmov_one_D_Rule_A1'}
//
// Representaive case:
// C(8)=1 & op(7:4)=00x1
//    = {actual: MoveDoubleVfpRegisterOp,
//       baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       pattern: cccc1100010otttttttt101100m1mmmm,
//       rule: Vmov_one_D_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       MoveDoubleVfpRegisterOpTester_Case1_TestCase1) {
  MoveDoubleVfpRegisterOpTester_Case1 tester;
  tester.Test("cccc1100010otttttttt101100m1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
