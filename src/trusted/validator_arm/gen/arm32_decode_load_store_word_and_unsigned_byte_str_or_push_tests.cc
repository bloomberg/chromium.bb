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

using nacl_arm_dec::Instruction;
using nacl_arm_dec::ClassDecoder;
using nacl_arm_dec::Register;
using nacl_arm_dec::RegisterList;

namespace nacl_arm_test {

// The following classes are derived class decoder testers that
// add row pattern constraints and decoder restrictions to each tester.
// This is done so that it can be used to make sure that the
// corresponding pattern is not tested for cases that would be excluded
//  due to row checks, or restrictions specified by the row restrictions.


// Neutral case:
// inst(24:21)=1001 & inst(19:16)=1101 & inst(11:0)=000000000100
//    = {baseline: 'Store2RegisterImm12OpRnNotRtOnWriteback',
//       constraints: }
//
// Representaive case:
// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = {baseline: Store2RegisterImm12OpRnNotRtOnWriteback,
//       constraints: }
class LoadStore2RegisterImm12OpTesterCase0
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase0(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01200000 /* Flags(24:21)=~1001 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) return false;
  if ((inst.Bits() & 0x00000FFF) != 0x00000004 /* Imm12(11:0)=~000000000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// 
//    = {baseline: 'Store2RegisterImm12Op',
//       constraints: & inst(31:0)=~xxxx010100101101xxxx000000000100 }
//
// Representaive case:
// 
//    = {baseline: Store2RegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxx010100101101xxxx000000000100 }
class LoadStore2RegisterImm12OpTesterCase1
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase1(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check pattern restrictions of row.
  if ((inst.Bits() & 0x0FFF0FFF) == 0x052D0004 /* constraint(31:0)=xxxx010100101101xxxx000000000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:21)=1001 & inst(19:16)=1101 & inst(11:0)=000000000100
//    = {baseline: 'Store2RegisterImm12OpRnNotRtOnWriteback',
//       constraints: ,
//       rule: 'Push_Rule_123_A2_P248'}
//
// Representative case:
// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = {baseline: Store2RegisterImm12OpRnNotRtOnWriteback,
//       constraints: ,
//       rule: Push_Rule_123_A2_P248}
class Store2RegisterImm12OpRnNotRtOnWritebackTester_Case0
    : public LoadStore2RegisterImm12OpTesterCase0 {
 public:
  Store2RegisterImm12OpRnNotRtOnWritebackTester_Case0()
    : LoadStore2RegisterImm12OpTesterCase0(
      state_.Store2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248_instance_)
  {}
};

// Neutral case:
// 
//    = {baseline: 'Store2RegisterImm12Op',
//       constraints: & inst(31:0)=~xxxx010100101101xxxx000000000100 ,
//       rule: 'Str_Rule_194_A1_P384'}
//
// Representative case:
// 
//    = {baseline: Store2RegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxx010100101101xxxx000000000100 ,
//       rule: Str_Rule_194_A1_P384}
class Store2RegisterImm12OpTester_Case1
    : public LoadStore2RegisterImm12OpTesterCase1 {
 public:
  Store2RegisterImm12OpTester_Case1()
    : LoadStore2RegisterImm12OpTesterCase1(
      state_.Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_)
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
// inst(24:21)=1001 & inst(19:16)=1101 & inst(11:0)=000000000100
//    = {actual: 'Store2RegisterImm12OpRnNotRtOnWriteback',
//       baseline: 'Store2RegisterImm12OpRnNotRtOnWriteback',
//       constraints: ,
//       pattern: 'cccc010100101101tttt000000000100',
//       rule: 'Push_Rule_123_A2_P248'}
//
// Representaive case:
// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = {actual: Store2RegisterImm12OpRnNotRtOnWriteback,
//       baseline: Store2RegisterImm12OpRnNotRtOnWriteback,
//       constraints: ,
//       pattern: cccc010100101101tttt000000000100,
//       rule: Push_Rule_123_A2_P248}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpRnNotRtOnWritebackTester_Case0_TestCase0) {
  Store2RegisterImm12OpRnNotRtOnWritebackTester_Case0 tester;
  tester.Test("cccc010100101101tttt000000000100");
}

// Neutral case:
// 
//    = {actual: 'StoreBasedImmedMemory',
//       baseline: 'Store2RegisterImm12Op',
//       constraints: & inst(31:0)=~xxxx010100101101xxxx000000000100 ,
//       pattern: 'cccc010pu0w0nnnnttttiiiiiiiiiiii',
//       rule: 'Str_Rule_194_A1_P384'}
//
// Representative case:
// 
//    = {actual: StoreBasedImmedMemory,
//       baseline: Store2RegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxx010100101101xxxx000000000100 ,
//       pattern: cccc010pu0w0nnnnttttiiiiiiiiiiii,
//       rule: Str_Rule_194_A1_P384}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Case1_TestCase1) {
  Store2RegisterImm12OpTester_Case1 baseline_tester;
  NamedStoreBasedImmedMemory_Str_Rule_194_A1_P384 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu0w0nnnnttttiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
