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
// inst(22:21)=00
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=00
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase0
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase0(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00000000 /* op1(22:21)=~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=01 & inst(5)=0
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=01 & op(5)=0
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase1
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase1(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=01 & inst(5)=1 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase2
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase2(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=10
//    = {baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=10
//    = {baseline: Binary4RegisterDualResult,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase3
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase3(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op1(22:21)=~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase4
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op1(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(22:21)=00
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       rule: 'Smlaxx_Rule_166_A1_P330',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=00
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       rule: Smlaxx_Rule_166_A1_P330,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case0
    : public Binary4RegisterDualOpTesterCase0 {
 public:
  Binary4RegisterDualOpTester_Case0()
    : Binary4RegisterDualOpTesterCase0(
      state_.Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_)
  {}
};

// Neutral case:
// inst(22:21)=01 & inst(5)=0
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       rule: 'Smlawx_Rule_171_A1_340',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=01 & op(5)=0
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       rule: Smlawx_Rule_171_A1_340,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case1
    : public Binary4RegisterDualOpTesterCase1 {
 public:
  Binary4RegisterDualOpTester_Case1()
    : Binary4RegisterDualOpTesterCase1(
      state_.Binary4RegisterDualOp_Smlawx_Rule_171_A1_340_instance_)
  {}
};

// Neutral case:
// inst(22:21)=01 & inst(5)=1 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       rule: 'Smulwx_Rule_180_A1_P358',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       rule: Smulwx_Rule_180_A1_P358,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case2
    : public Binary3RegisterOpAltATesterCase2 {
 public:
  Binary3RegisterOpAltATester_Case2()
    : Binary3RegisterOpAltATesterCase2(
      state_.Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_)
  {}
};

// Neutral case:
// inst(22:21)=10
//    = {baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       rule: 'Smlalxx_Rule_169_A1_P336',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=10
//    = {baseline: Binary4RegisterDualResult,
//       constraints: ,
//       rule: Smlalxx_Rule_169_A1_P336,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case3
    : public Binary4RegisterDualResultTesterCase3 {
 public:
  Binary4RegisterDualResultTester_Case3()
    : Binary4RegisterDualResultTesterCase3(
      state_.Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_)
  {}
};

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       rule: 'Smulxx_Rule_178_P354',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       rule: Smulxx_Rule_178_P354,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case4
    : public Binary3RegisterOpAltATesterCase4 {
 public:
  Binary3RegisterOpAltATester_Case4()
    : Binary3RegisterOpAltATesterCase4(
      state_.Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_)
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
// inst(22:21)=00
//    = {actual: 'Defs16To19CondsDontCareRdRaRmRnNotPc',
//       baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       pattern: 'cccc00010000ddddaaaammmm1xx0nnnn',
//       rule: 'Smlaxx_Rule_166_A1_P330',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=00
//    = {actual: Defs16To19CondsDontCareRdRaRmRnNotPc,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       pattern: cccc00010000ddddaaaammmm1xx0nnnn,
//       rule: Smlaxx_Rule_166_A1_P330,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case0_TestCase0) {
  Binary4RegisterDualOpTester_Case0 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000ddddaaaammmm1xx0nnnn");
}

// Neutral case:
// inst(22:21)=01 & inst(5)=0
//    = {actual: 'Defs16To19CondsDontCareRdRaRmRnNotPc',
//       baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       pattern: 'cccc00010010ddddaaaammmm1x00nnnn',
//       rule: 'Smlawx_Rule_171_A1_340',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=01 & op(5)=0
//    = {actual: Defs16To19CondsDontCareRdRaRmRnNotPc,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       pattern: cccc00010010ddddaaaammmm1x00nnnn,
//       rule: Smlawx_Rule_171_A1_340,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case1_TestCase1) {
  Binary4RegisterDualOpTester_Case1 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010ddddaaaammmm1x00nnnn");
}

// Neutral case:
// inst(22:21)=01 & inst(5)=1 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'Defs16To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       pattern: 'cccc00010010dddd0000mmmm1x10nnnn',
//       rule: 'Smulwx_Rule_180_A1_P358',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: Defs16To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       pattern: cccc00010010dddd0000mmmm1x10nnnn,
//       rule: Smulwx_Rule_180_A1_P358,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case2_TestCase2) {
  Binary3RegisterOpAltATester_Case2 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010dddd0000mmmm1x10nnnn");
}

// Neutral case:
// inst(22:21)=10
//    = {actual: 'Defs12To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       pattern: 'cccc00010100hhhhllllmmmm1xx0nnnn',
//       rule: 'Smlalxx_Rule_169_A1_P336',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=10
//    = {actual: Defs12To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary4RegisterDualResult,
//       constraints: ,
//       pattern: cccc00010100hhhhllllmmmm1xx0nnnn,
//       rule: Smlalxx_Rule_169_A1_P336,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case3_TestCase3) {
  Binary4RegisterDualResultTester_Case3 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100hhhhllllmmmm1xx0nnnn");
}

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'Defs16To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       pattern: 'cccc00010110dddd0000mmmm1xx0nnnn',
//       rule: 'Smulxx_Rule_178_P354',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: Defs16To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       pattern: cccc00010110dddd0000mmmm1xx0nnnn,
//       rule: Smulxx_Rule_178_P354,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case4_TestCase4) {
  Binary3RegisterOpAltATester_Case4 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110dddd0000mmmm1xx0nnnn");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
