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
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=~1111
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
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
  if ((inst.Bits() & 0x01F00000) != 0x01800000 /* op1(24:20)=~11000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=1111
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase1
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01800000 /* op1(24:20)=~11000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=11111 & inst(7:5)=111
//    = {baseline: 'Roadblock',
//       constraints: }
//
// Representaive case:
// op1(24:20)=11111 & op2(7:5)=111
//    = {baseline: Roadblock,
//       constraints: }
class RoadblockTesterCase2
    : public RoadblockTester {
 public:
  RoadblockTesterCase2(const NamedClassDecoder& decoder)
    : RoadblockTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool RoadblockTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01F00000 /* op1(24:20)=~11111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return RoadblockTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(7:5)=x10
//    = {baseline: 'Binary2RegisterBitRangeNotRnIsPcBitfieldExtract',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(7:5)=x10
//    = {baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase3
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase3(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(7:5)=~x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=~1111
//    = {baseline: 'Binary2RegisterBitRangeMsbGeLsb',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = {baseline: Binary2RegisterBitRangeMsbGeLsb,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary2RegisterBitRangeMsbGeLsbTesterCase4
    : public Binary2RegisterBitRangeMsbGeLsbTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeMsbGeLsbTesterCase4(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeMsbGeLsbTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeMsbGeLsbTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(7:5)=~x00 */) return false;
  if ((inst.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeMsbGeLsbTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=1111
//    = {baseline: 'Unary1RegisterBitRangeMsbGeLsb',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = {baseline: Unary1RegisterBitRangeMsbGeLsb,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary1RegisterBitRangeMsbGeLsbTesterCase5
    : public Unary1RegisterBitRangeMsbGeLsbTesterRegsNotPc {
 public:
  Unary1RegisterBitRangeMsbGeLsbTesterCase5(const NamedClassDecoder& decoder)
    : Unary1RegisterBitRangeMsbGeLsbTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterBitRangeMsbGeLsbTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(7:5)=~x00 */) return false;
  if ((inst.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterBitRangeMsbGeLsbTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1111x & inst(7:5)=x10
//    = {baseline: 'Binary2RegisterBitRangeNotRnIsPcBitfieldExtract',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1111x & op2(7:5)=x10
//    = {baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase6
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase6(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(7:5)=~x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=~1111
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       rule: 'Usada8_Rule_254_A1_P502',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       rule: Usada8_Rule_254_A1_P502,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case0
    : public Binary4RegisterDualOpTesterCase0 {
 public:
  Binary4RegisterDualOpTester_Case0()
    : Binary4RegisterDualOpTesterCase0(
      state_.Binary4RegisterDualOp_Usada8_Rule_254_A1_P502_instance_)
  {}
};

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=1111
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       rule: 'Usad8_Rule_253_A1_P500',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       rule: Usad8_Rule_253_A1_P500,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case1
    : public Binary3RegisterOpAltATesterCase1 {
 public:
  Binary3RegisterOpAltATester_Case1()
    : Binary3RegisterOpAltATesterCase1(
      state_.Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500_instance_)
  {}
};

// Neutral case:
// inst(24:20)=11111 & inst(7:5)=111
//    = {baseline: 'Roadblock',
//       constraints: ,
//       rule: 'Udf_Rule_A1'}
//
// Representative case:
// op1(24:20)=11111 & op2(7:5)=111
//    = {baseline: Roadblock,
//       constraints: ,
//       rule: Udf_Rule_A1}
class RoadblockTester_Case2
    : public RoadblockTesterCase2 {
 public:
  RoadblockTester_Case2()
    : RoadblockTesterCase2(
      state_.Roadblock_Udf_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(7:5)=x10
//    = {baseline: 'Binary2RegisterBitRangeNotRnIsPcBitfieldExtract',
//       constraints: ,
//       rule: 'Sbfx_Rule_154_A1_P308',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(7:5)=x10
//    = {baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//       constraints: ,
//       rule: Sbfx_Rule_154_A1_P308,
//       safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case3
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase3 {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case3()
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase3(
      state_.Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=~1111
//    = {baseline: 'Binary2RegisterBitRangeMsbGeLsb',
//       constraints: ,
//       rule: 'Bfi_Rule_18_A1_P48',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = {baseline: Binary2RegisterBitRangeMsbGeLsb,
//       constraints: ,
//       rule: Bfi_Rule_18_A1_P48,
//       safety: ['RegsNotPc']}
class Binary2RegisterBitRangeMsbGeLsbTester_Case4
    : public Binary2RegisterBitRangeMsbGeLsbTesterCase4 {
 public:
  Binary2RegisterBitRangeMsbGeLsbTester_Case4()
    : Binary2RegisterBitRangeMsbGeLsbTesterCase4(
      state_.Binary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=1111
//    = {baseline: 'Unary1RegisterBitRangeMsbGeLsb',
//       constraints: ,
//       rule: 'Bfc_17_A1_P46',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = {baseline: Unary1RegisterBitRangeMsbGeLsb,
//       constraints: ,
//       rule: Bfc_17_A1_P46,
//       safety: ['RegsNotPc']}
class Unary1RegisterBitRangeMsbGeLsbTester_Case5
    : public Unary1RegisterBitRangeMsbGeLsbTesterCase5 {
 public:
  Unary1RegisterBitRangeMsbGeLsbTester_Case5()
    : Unary1RegisterBitRangeMsbGeLsbTesterCase5(
      state_.Unary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(7:5)=x10
//    = {baseline: 'Binary2RegisterBitRangeNotRnIsPcBitfieldExtract',
//       constraints: ,
//       rule: 'Ubfx_Rule_236_A1_P466',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1111x & op2(7:5)=x10
//    = {baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//       constraints: ,
//       rule: Ubfx_Rule_236_A1_P466,
//       safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case6
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase6 {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case6()
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase6(
      state_.Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466_instance_)
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
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=~1111
//    = {actual: 'Defs16To19CondsDontCareRdRaRmRnNotPc',
//       baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       pattern: 'cccc01111000ddddaaaammmm0001nnnn',
//       rule: 'Usada8_Rule_254_A1_P502',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
//    = {actual: Defs16To19CondsDontCareRdRaRmRnNotPc,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       pattern: cccc01111000ddddaaaammmm0001nnnn,
//       rule: Usada8_Rule_254_A1_P502,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case0_TestCase0) {
  Binary4RegisterDualOpTester_Case0 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01111000ddddaaaammmm0001nnnn");
}

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=1111
//    = {actual: 'Defs16To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       pattern: 'cccc01111000dddd1111mmmm0001nnnn',
//       rule: 'Usad8_Rule_253_A1_P500',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = {actual: Defs16To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       pattern: cccc01111000dddd1111mmmm0001nnnn,
//       rule: Usad8_Rule_253_A1_P500,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case1_TestCase1) {
  Binary3RegisterOpAltATester_Case1 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01111000dddd1111mmmm0001nnnn");
}

// Neutral case:
// inst(24:20)=11111 & inst(7:5)=111
//    = {actual: 'Roadblock',
//       baseline: 'Roadblock',
//       constraints: ,
//       pattern: 'cccc01111111iiiiiiiiiiii1111iiii',
//       rule: 'Udf_Rule_A1'}
//
// Representaive case:
// op1(24:20)=11111 & op2(7:5)=111
//    = {actual: Roadblock,
//       baseline: Roadblock,
//       constraints: ,
//       pattern: cccc01111111iiiiiiiiiiii1111iiii,
//       rule: Udf_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       RoadblockTester_Case2_TestCase2) {
  RoadblockTester_Case2 tester;
  tester.Test("cccc01111111iiiiiiiiiiii1111iiii");
}

// Neutral case:
// inst(24:20)=1101x & inst(7:5)=x10
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPcBitfieldExtract',
//       baseline: 'Binary2RegisterBitRangeNotRnIsPcBitfieldExtract',
//       constraints: ,
//       pattern: 'cccc0111101wwwwwddddlllll101nnnn',
//       rule: 'Sbfx_Rule_154_A1_P308',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(7:5)=x10
//    = {actual: Defs12To15CondsDontCareRdRnNotPcBitfieldExtract,
//       baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//       constraints: ,
//       pattern: cccc0111101wwwwwddddlllll101nnnn,
//       rule: Sbfx_Rule_154_A1_P308,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case3_TestCase3) {
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case3 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Sbfx_Rule_154_A1_P308 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111101wwwwwddddlllll101nnnn");
}

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=~1111
//    = {actual: 'Defs12To15CondsDontCareMsbGeLsb',
//       baseline: 'Binary2RegisterBitRangeMsbGeLsb',
//       constraints: ,
//       pattern: 'cccc0111110mmmmmddddlllll001nnnn',
//       rule: 'Bfi_Rule_18_A1_P48',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = {actual: Defs12To15CondsDontCareMsbGeLsb,
//       baseline: Binary2RegisterBitRangeMsbGeLsb,
//       constraints: ,
//       pattern: cccc0111110mmmmmddddlllll001nnnn,
//       rule: Bfi_Rule_18_A1_P48,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeMsbGeLsbTester_Case4_TestCase4) {
  Binary2RegisterBitRangeMsbGeLsbTester_Case4 baseline_tester;
  NamedDefs12To15CondsDontCareMsbGeLsb_Bfi_Rule_18_A1_P48 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111110mmmmmddddlllll001nnnn");
}

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=1111
//    = {actual: 'Unary1RegisterBitRangeMsbGeLsb',
//       baseline: 'Unary1RegisterBitRangeMsbGeLsb',
//       constraints: ,
//       pattern: 'cccc0111110mmmmmddddlllll0011111',
//       rule: 'Bfc_17_A1_P46',
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = {actual: Unary1RegisterBitRangeMsbGeLsb,
//       baseline: Unary1RegisterBitRangeMsbGeLsb,
//       constraints: ,
//       pattern: cccc0111110mmmmmddddlllll0011111,
//       rule: Bfc_17_A1_P46,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterBitRangeMsbGeLsbTester_Case5_TestCase5) {
  Unary1RegisterBitRangeMsbGeLsbTester_Case5 tester;
  tester.Test("cccc0111110mmmmmddddlllll0011111");
}

// Neutral case:
// inst(24:20)=1111x & inst(7:5)=x10
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPcBitfieldExtract',
//       baseline: 'Binary2RegisterBitRangeNotRnIsPcBitfieldExtract',
//       constraints: ,
//       pattern: 'cccc0111111mmmmmddddlllll101nnnn',
//       rule: 'Ubfx_Rule_236_A1_P466',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1111x & op2(7:5)=x10
//    = {actual: Defs12To15CondsDontCareRdRnNotPcBitfieldExtract,
//       baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//       constraints: ,
//       pattern: cccc0111111mmmmmddddlllll101nnnn,
//       rule: Ubfx_Rule_236_A1_P466,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case6_TestCase6) {
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case6 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Ubfx_Rule_236_A1_P466 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111111mmmmmddddlllll101nnnn");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
