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
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase0
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase0(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00000000 /* op(22:21)=~00 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase1
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase2(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op(22:21)=~10 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase3
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qadd_Rule_124_A1_P250',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qadd_Rule_124_A1_P250,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case0
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase0 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase0(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd_Rule_124_A1_P250_instance_)
  {}
};

// Neutral case:
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qsub_Rule_131_A1_P264',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qsub_Rule_131_A1_P264,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case1
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase1 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase1(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub_Rule_131_A1_P264_instance_)
  {}
};

// Neutral case:
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qdadd_Rule_128_A1_P258',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qdadd_Rule_128_A1_P258,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase2 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase2(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qdadd_Rule_128_A1_P258_instance_)
  {}
};

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qdsub_Rule_129_A1_P260',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qdsub_Rule_129_A1_P260,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case3
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase3 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase3(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qdsub_Rule_129_A1_P260_instance_)
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
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc00010000nnnndddd00000101mmmm',
//       rule: 'Qadd_Rule_124_A1_P250',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc00010000nnnndddd00000101mmmm,
//       rule: Qadd_Rule_124_A1_P250,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case0_TestCase0) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000nnnndddd00000101mmmm");
}

// Neutral case:
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc00010010nnnndddd00000101mmmm',
//       rule: 'Qsub_Rule_131_A1_P264',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc00010010nnnndddd00000101mmmm,
//       rule: Qsub_Rule_131_A1_P264,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case1_TestCase1) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010nnnndddd00000101mmmm");
}

// Neutral case:
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc00010100nnnndddd00000101mmmm',
//       rule: 'Qdadd_Rule_128_A1_P258',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc00010100nnnndddd00000101mmmm,
//       rule: Qdadd_Rule_128_A1_P258,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case2_TestCase2) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100nnnndddd00000101mmmm");
}

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc00010110nnnndddd00000101mmmm',
//       rule: 'Qdsub_Rule_129_A1_P260',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc00010110nnnndddd00000101mmmm,
//       rule: Qdsub_Rule_129_A1_P260,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case3_TestCase3) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110nnnndddd00000101mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
