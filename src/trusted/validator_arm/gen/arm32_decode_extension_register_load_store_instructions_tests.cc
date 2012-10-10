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
// inst(24:20)=01x00
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: }
//
// Representaive case:
// opcode(24:20)=01x00
//    = {baseline: StoreVectorRegisterList,
//       constraints: }
class StoreVectorRegisterListTesterCase0
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase0(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00800000 /* opcode(24:20)=~01x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01x01
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: }
//
// Representaive case:
// opcode(24:20)=01x01
//    = {baseline: LoadVectorRegisterList,
//       constraints: }
class LoadStoreVectorRegisterListTesterCase1
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase1(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00900000 /* opcode(24:20)=~01x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01x10
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: }
//
// Representaive case:
// opcode(24:20)=01x10
//    = {baseline: StoreVectorRegisterList,
//       constraints: }
class StoreVectorRegisterListTesterCase2
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase2(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00A00000 /* opcode(24:20)=~01x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       safety: ["'NotRnIsSp'"]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = {baseline: LoadVectorRegisterList,
//       constraints: ,
//       safety: ['NotRnIsSp']}
class LoadStoreVectorRegisterListTesterCase3
    : public LoadStoreVectorRegisterListTesterNotRnIsSp {
 public:
  LoadStoreVectorRegisterListTesterCase3(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTesterNotRnIsSp(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00B00000 /* opcode(24:20)=~01x11 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTesterNotRnIsSp::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: }
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = {baseline: LoadVectorRegisterList,
//       constraints: }
class LoadStoreVectorRegisterListTesterCase4
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase4(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00B00000 /* opcode(24:20)=~01x11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       safety: ["'NotRnIsSp'"]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = {baseline: StoreVectorRegisterList,
//       constraints: ,
//       safety: ['NotRnIsSp']}
class StoreVectorRegisterListTesterCase5
    : public StoreVectorRegisterListTesterNotRnIsSp {
 public:
  StoreVectorRegisterListTesterCase5(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTesterNotRnIsSp(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x01200000 /* opcode(24:20)=~10x10 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTesterNotRnIsSp::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: }
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = {baseline: StoreVectorRegisterList,
//       constraints: }
class StoreVectorRegisterListTesterCase6
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase6(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x01200000 /* opcode(24:20)=~10x10 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10x11
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: }
//
// Representaive case:
// opcode(24:20)=10x11
//    = {baseline: LoadVectorRegisterList,
//       constraints: }
class LoadStoreVectorRegisterListTesterCase7
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase7(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x01300000 /* opcode(24:20)=~10x11 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1xx00
//    = {baseline: 'StoreVectorRegister',
//       constraints: }
//
// Representaive case:
// opcode(24:20)=1xx00
//    = {baseline: StoreVectorRegister,
//       constraints: }
class StoreVectorRegisterTesterCase8
    : public StoreVectorRegisterTester {
 public:
  StoreVectorRegisterTesterCase8(const NamedClassDecoder& decoder)
    : StoreVectorRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01300000) != 0x01000000 /* opcode(24:20)=~1xx00 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1xx01
//    = {baseline: 'LoadVectorRegister',
//       constraints: }
//
// Representaive case:
// opcode(24:20)=1xx01
//    = {baseline: LoadVectorRegister,
//       constraints: }
class LoadStoreVectorOpTesterCase9
    : public LoadStoreVectorOpTester {
 public:
  LoadStoreVectorOpTesterCase9(const NamedClassDecoder& decoder)
    : LoadStoreVectorOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01300000) != 0x01100000 /* opcode(24:20)=~1xx01 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorOpTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:20)=01x00
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       rule: 'Vstm_Rule_399_A1_A2_P784'}
//
// Representative case:
// opcode(24:20)=01x00
//    = {baseline: StoreVectorRegisterList,
//       constraints: ,
//       rule: Vstm_Rule_399_A1_A2_P784}
class StoreVectorRegisterListTester_Case0
    : public StoreVectorRegisterListTesterCase0 {
 public:
  StoreVectorRegisterListTester_Case0()
    : StoreVectorRegisterListTesterCase0(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x01
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       rule: 'Vldm_Rule_319_A1_A2_P626'}
//
// Representative case:
// opcode(24:20)=01x01
//    = {baseline: LoadVectorRegisterList,
//       constraints: ,
//       rule: Vldm_Rule_319_A1_A2_P626}
class LoadVectorRegisterListTester_Case1
    : public LoadStoreVectorRegisterListTesterCase1 {
 public:
  LoadVectorRegisterListTester_Case1()
    : LoadStoreVectorRegisterListTesterCase1(
      state_.LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x10
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       rule: 'Vstm_Rule_399_A1_A2_P784'}
//
// Representative case:
// opcode(24:20)=01x10
//    = {baseline: StoreVectorRegisterList,
//       constraints: ,
//       rule: Vstm_Rule_399_A1_A2_P784}
class StoreVectorRegisterListTester_Case2
    : public StoreVectorRegisterListTesterCase2 {
 public:
  StoreVectorRegisterListTester_Case2()
    : StoreVectorRegisterListTesterCase2(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       rule: 'Vldm_Rule_319_A1_A2_P626',
//       safety: ["'NotRnIsSp'"]}
//
// Representative case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = {baseline: LoadVectorRegisterList,
//       constraints: ,
//       rule: Vldm_Rule_319_A1_A2_P626,
//       safety: ['NotRnIsSp']}
class LoadVectorRegisterListTester_Case3
    : public LoadStoreVectorRegisterListTesterCase3 {
 public:
  LoadVectorRegisterListTester_Case3()
    : LoadStoreVectorRegisterListTesterCase3(
      state_.LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       rule: 'Vpop_Rule_354_A1_A2_P694'}
//
// Representative case:
// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = {baseline: LoadVectorRegisterList,
//       constraints: ,
//       rule: Vpop_Rule_354_A1_A2_P694}
class LoadVectorRegisterListTester_Case4
    : public LoadStoreVectorRegisterListTesterCase4 {
 public:
  LoadVectorRegisterListTester_Case4()
    : LoadStoreVectorRegisterListTesterCase4(
      state_.LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       rule: 'Vstm_Rule_399_A1_A2_P784',
//       safety: ["'NotRnIsSp'"]}
//
// Representative case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = {baseline: StoreVectorRegisterList,
//       constraints: ,
//       rule: Vstm_Rule_399_A1_A2_P784,
//       safety: ['NotRnIsSp']}
class StoreVectorRegisterListTester_Case5
    : public StoreVectorRegisterListTesterCase5 {
 public:
  StoreVectorRegisterListTester_Case5()
    : StoreVectorRegisterListTesterCase5(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       rule: 'Vpush_355_A1_A2_P696'}
//
// Representative case:
// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = {baseline: StoreVectorRegisterList,
//       constraints: ,
//       rule: Vpush_355_A1_A2_P696}
class StoreVectorRegisterListTester_Case6
    : public StoreVectorRegisterListTesterCase6 {
 public:
  StoreVectorRegisterListTester_Case6()
    : StoreVectorRegisterListTesterCase6(
      state_.StoreVectorRegisterList_Vpush_355_A1_A2_P696_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x11
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       rule: 'Vldm_Rule_318_A1_A2_P626'}
//
// Representative case:
// opcode(24:20)=10x11
//    = {baseline: LoadVectorRegisterList,
//       constraints: ,
//       rule: Vldm_Rule_318_A1_A2_P626}
class LoadVectorRegisterListTester_Case7
    : public LoadStoreVectorRegisterListTesterCase7 {
 public:
  LoadVectorRegisterListTester_Case7()
    : LoadStoreVectorRegisterListTesterCase7(
      state_.LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1xx00
//    = {baseline: 'StoreVectorRegister',
//       constraints: ,
//       rule: 'Vstr_Rule_400_A1_A2_P786'}
//
// Representative case:
// opcode(24:20)=1xx00
//    = {baseline: StoreVectorRegister,
//       constraints: ,
//       rule: Vstr_Rule_400_A1_A2_P786}
class StoreVectorRegisterTester_Case8
    : public StoreVectorRegisterTesterCase8 {
 public:
  StoreVectorRegisterTester_Case8()
    : StoreVectorRegisterTesterCase8(
      state_.StoreVectorRegister_Vstr_Rule_400_A1_A2_P786_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1xx01
//    = {baseline: 'LoadVectorRegister',
//       constraints: ,
//       rule: 'Vldr_Rule_320_A1_A2_P628'}
//
// Representative case:
// opcode(24:20)=1xx01
//    = {baseline: LoadVectorRegister,
//       constraints: ,
//       rule: Vldr_Rule_320_A1_A2_P628}
class LoadVectorRegisterTester_Case9
    : public LoadStoreVectorOpTesterCase9 {
 public:
  LoadVectorRegisterTester_Case9()
    : LoadStoreVectorOpTesterCase9(
      state_.LoadVectorRegister_Vldr_Rule_320_A1_A2_P628_instance_)
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
// inst(24:20)=01x00
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       pattern: 'cccc11001d00nnnndddd101xiiiiiiii',
//       rule: 'Vstm_Rule_399_A1_A2_P784'}
//
// Representaive case:
// opcode(24:20)=01x00
//    = {actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       pattern: cccc11001d00nnnndddd101xiiiiiiii,
//       rule: Vstm_Rule_399_A1_A2_P784}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case0_TestCase0) {
  StoreVectorRegisterListTester_Case0 tester;
  tester.Test("cccc11001d00nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=01x01
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       pattern: 'cccc11001d01nnnndddd101xiiiiiiii',
//       rule: 'Vldm_Rule_319_A1_A2_P626'}
//
// Representaive case:
// opcode(24:20)=01x01
//    = {actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       pattern: cccc11001d01nnnndddd101xiiiiiiii,
//       rule: Vldm_Rule_319_A1_A2_P626}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case1_TestCase1) {
  LoadVectorRegisterListTester_Case1 tester;
  tester.Test("cccc11001d01nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=01x10
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       pattern: 'cccc11001d10nnnndddd101xiiiiiiii',
//       rule: 'Vstm_Rule_399_A1_A2_P784'}
//
// Representaive case:
// opcode(24:20)=01x10
//    = {actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       pattern: cccc11001d10nnnndddd101xiiiiiiii,
//       rule: Vstm_Rule_399_A1_A2_P784}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case2_TestCase2) {
  StoreVectorRegisterListTester_Case2 tester;
  tester.Test("cccc11001d10nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       pattern: 'cccc11001d11nnnndddd101xiiiiiiii',
//       rule: 'Vldm_Rule_319_A1_A2_P626',
//       safety: ["'NotRnIsSp'"]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = {actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       pattern: cccc11001d11nnnndddd101xiiiiiiii,
//       rule: Vldm_Rule_319_A1_A2_P626,
//       safety: ['NotRnIsSp']}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case3_TestCase3) {
  LoadVectorRegisterListTester_Case3 tester;
  tester.Test("cccc11001d11nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       pattern: 'cccc11001d111101dddd101xiiiiiiii',
//       rule: 'Vpop_Rule_354_A1_A2_P694'}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = {actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       pattern: cccc11001d111101dddd101xiiiiiiii,
//       rule: Vpop_Rule_354_A1_A2_P694}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case4_TestCase4) {
  LoadVectorRegisterListTester_Case4 tester;
  tester.Test("cccc11001d111101dddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       pattern: 'cccc11010d10nnnndddd101xiiiiiiii',
//       rule: 'Vstm_Rule_399_A1_A2_P784',
//       safety: ["'NotRnIsSp'"]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = {actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       pattern: cccc11010d10nnnndddd101xiiiiiiii,
//       rule: Vstm_Rule_399_A1_A2_P784,
//       safety: ['NotRnIsSp']}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case5_TestCase5) {
  StoreVectorRegisterListTester_Case5 tester;
  tester.Test("cccc11010d10nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       pattern: 'cccc11010d101101dddd101xiiiiiiii',
//       rule: 'Vpush_355_A1_A2_P696'}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = {actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       pattern: cccc11010d101101dddd101xiiiiiiii,
//       rule: Vpush_355_A1_A2_P696}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case6_TestCase6) {
  StoreVectorRegisterListTester_Case6 tester;
  tester.Test("cccc11010d101101dddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=10x11
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       pattern: 'cccc11010d11nnnndddd101xiiiiiiii',
//       rule: 'Vldm_Rule_318_A1_A2_P626'}
//
// Representaive case:
// opcode(24:20)=10x11
//    = {actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       pattern: cccc11010d11nnnndddd101xiiiiiiii,
//       rule: Vldm_Rule_318_A1_A2_P626}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case7_TestCase7) {
  LoadVectorRegisterListTester_Case7 tester;
  tester.Test("cccc11010d11nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=1xx00
//    = {actual: 'StoreVectorRegister',
//       baseline: 'StoreVectorRegister',
//       constraints: ,
//       pattern: 'cccc1101ud00nnnndddd101xiiiiiiii',
//       rule: 'Vstr_Rule_400_A1_A2_P786'}
//
// Representaive case:
// opcode(24:20)=1xx00
//    = {actual: StoreVectorRegister,
//       baseline: StoreVectorRegister,
//       constraints: ,
//       pattern: cccc1101ud00nnnndddd101xiiiiiiii,
//       rule: Vstr_Rule_400_A1_A2_P786}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterTester_Case8_TestCase8) {
  StoreVectorRegisterTester_Case8 tester;
  tester.Test("cccc1101ud00nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=1xx01
//    = {actual: 'LoadVectorRegister',
//       baseline: 'LoadVectorRegister',
//       constraints: ,
//       pattern: 'cccc1101ud01nnnndddd101xiiiiiiii',
//       rule: 'Vldr_Rule_320_A1_A2_P628'}
//
// Representaive case:
// opcode(24:20)=1xx01
//    = {actual: LoadVectorRegister,
//       baseline: LoadVectorRegister,
//       constraints: ,
//       pattern: cccc1101ud01nnnndddd101xiiiiiiii,
//       rule: Vldr_Rule_320_A1_A2_P628}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterTester_Case9_TestCase9) {
  LoadVectorRegisterTester_Case9 tester;
  tester.Test("cccc1101ud01nnnndddd101xiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
