/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
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
#include "native_client/src/trusted/validator_arm/arm_helpers.h"

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
// inst(11:8)=~101x & inst(25:20)=000100
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=000100
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~000100
  if ((inst.Bits() & 0x03F00000)  !=
          0x00400000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000101
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=000101
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~000101
  if ((inst.Bits() & 0x03F00000)  !=
          0x00500000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx0 & inst(4)=1
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase2
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~10xxx0
  if ((inst.Bits() & 0x03100000)  !=
          0x02000000) return false;
  // op(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx1 & inst(4)=1
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase3
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~10xxx1
  if ((inst.Bits() & 0x03100000)  !=
          0x02100000) return false;
  // op(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx0 & inst(25:20)=~000x00
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase4
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase4(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~0xxxx0
  if ((inst.Bits() & 0x02100000)  !=
          0x00000000) return false;
  // op1_repeated(25:20)=000x00
  if ((inst.Bits() & 0x03B00000)  ==
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=~1111 & inst(25:20)=~000x01
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase5
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase5(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~0xxxx1
  if ((inst.Bits() & 0x02100000)  !=
          0x00100000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // op1_repeated(25:20)=000x01
  if ((inst.Bits() & 0x03B00000)  ==
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=1111 & inst(25:20)=~000x01
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase6
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase6(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~0xxxx1
  if ((inst.Bits() & 0x02100000)  !=
          0x00100000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // op1_repeated(25:20)=000x01
  if ((inst.Bits() & 0x03B00000)  ==
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxxx & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase7
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase7(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~10xxxx
  if ((inst.Bits() & 0x03000000)  !=
          0x02000000) return false;
  // op(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=00000x
//    = {baseline: 'UndefinedCondDecoder',
//       constraints: }
//
// Representaive case:
// op1(25:20)=00000x
//    = {baseline: UndefinedCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase8
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase8(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(25:20)=~00000x
  if ((inst.Bits() & 0x03E00000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=11xxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op1(25:20)=11xxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase9
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(25:20)=~11xxxx
  if ((inst.Bits() & 0x03000000)  !=
          0x03000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000100
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Mcrr_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=000100
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Mcrr_Rule_A1}
class ForbiddenCondDecoderTester_Case0
    : public UnsafeCondDecoderTesterCase0 {
 public:
  ForbiddenCondDecoderTester_Case0()
    : UnsafeCondDecoderTesterCase0(
      state_.ForbiddenCondDecoder_Mcrr_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000101
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Mrrc_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=000101
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Mrrc_Rule_A1}
class ForbiddenCondDecoderTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenCondDecoderTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.ForbiddenCondDecoder_Mrrc_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx0 & inst(4)=1
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Mcr_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Mcr_Rule_A1}
class ForbiddenCondDecoderTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenCondDecoderTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.ForbiddenCondDecoder_Mcr_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx1 & inst(4)=1
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Mrc_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Mrc_Rule_A1}
class ForbiddenCondDecoderTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenCondDecoderTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.ForbiddenCondDecoder_Mrc_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx0 & inst(25:20)=~000x00
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Stc_Rule_A2'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Stc_Rule_A2}
class ForbiddenCondDecoderTester_Case4
    : public UnsafeCondDecoderTesterCase4 {
 public:
  ForbiddenCondDecoderTester_Case4()
    : UnsafeCondDecoderTesterCase4(
      state_.ForbiddenCondDecoder_Stc_Rule_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=~1111 & inst(25:20)=~000x01
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Ldc_immediate_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Ldc_immediate_Rule_A1}
class ForbiddenCondDecoderTester_Case5
    : public UnsafeCondDecoderTesterCase5 {
 public:
  ForbiddenCondDecoderTester_Case5()
    : UnsafeCondDecoderTesterCase5(
      state_.ForbiddenCondDecoder_Ldc_immediate_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=1111 & inst(25:20)=~000x01
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Ldc_literal_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Ldc_literal_Rule_A1}
class ForbiddenCondDecoderTester_Case6
    : public UnsafeCondDecoderTesterCase6 {
 public:
  ForbiddenCondDecoderTester_Case6()
    : UnsafeCondDecoderTesterCase6(
      state_.ForbiddenCondDecoder_Ldc_literal_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxxx & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Cdp_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Cdp_Rule_A1}
class ForbiddenCondDecoderTester_Case7
    : public UnsafeCondDecoderTesterCase7 {
 public:
  ForbiddenCondDecoderTester_Case7()
    : UnsafeCondDecoderTesterCase7(
      state_.ForbiddenCondDecoder_Cdp_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(25:20)=00000x
//    = {baseline: 'UndefinedCondDecoder',
//       constraints: ,
//       rule: 'Undefined_A5_6'}
//
// Representative case:
// op1(25:20)=00000x
//    = {baseline: UndefinedCondDecoder,
//       constraints: ,
//       rule: Undefined_A5_6}
class UndefinedCondDecoderTester_Case8
    : public UnsafeCondDecoderTesterCase8 {
 public:
  UndefinedCondDecoderTester_Case8()
    : UnsafeCondDecoderTesterCase8(
      state_.UndefinedCondDecoder_Undefined_A5_6_instance_)
  {}
};

// Neutral case:
// inst(25:20)=11xxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Svc_Rule_A1'}
//
// Representative case:
// op1(25:20)=11xxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Svc_Rule_A1}
class ForbiddenCondDecoderTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenCondDecoderTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.ForbiddenCondDecoder_Svc_Rule_A1_instance_)
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
// inst(11:8)=~101x & inst(25:20)=000100
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc11000100ttttttttccccoooommmm',
//       rule: 'Mcrr_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=000100
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc11000100ttttttttccccoooommmm,
//       rule: Mcrr_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case0_TestCase0) {
  ForbiddenCondDecoderTester_Case0 baseline_tester;
  NamedForbidden_Mcrr_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11000100ttttttttccccoooommmm");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000101
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc11000101ttttttttccccoooommmm',
//       rule: 'Mrrc_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=000101
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc11000101ttttttttccccoooommmm,
//       rule: Mrrc_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case1_TestCase1) {
  ForbiddenCondDecoderTester_Case1 baseline_tester;
  NamedForbidden_Mrrc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11000101ttttttttccccoooommmm");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx0 & inst(4)=1
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc1110ooo0nnnnttttccccooo1mmmm',
//       rule: 'Mcr_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc1110ooo0nnnnttttccccooo1mmmm,
//       rule: Mcr_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case2_TestCase2) {
  ForbiddenCondDecoderTester_Case2 baseline_tester;
  NamedForbidden_Mcr_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110ooo0nnnnttttccccooo1mmmm");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx1 & inst(4)=1
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc1110ooo1nnnnttttccccooo1mmmm',
//       rule: 'Mrc_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc1110ooo1nnnnttttccccooo1mmmm,
//       rule: Mrc_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case3_TestCase3) {
  ForbiddenCondDecoderTester_Case3 baseline_tester;
  NamedForbidden_Mrc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110ooo1nnnnttttccccooo1mmmm");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx0 & inst(25:20)=~000x00
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc110pudw0nnnnddddcccciiiiiiii',
//       rule: 'Stc_Rule_A2'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc110pudw0nnnnddddcccciiiiiiii,
//       rule: Stc_Rule_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case4_TestCase4) {
  ForbiddenCondDecoderTester_Case4 baseline_tester;
  NamedForbidden_Stc_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw0nnnnddddcccciiiiiiii");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=~1111 & inst(25:20)=~000x01
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc110pudw1nnnnddddcccciiiiiiii',
//       rule: 'Ldc_immediate_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc110pudw1nnnnddddcccciiiiiiii,
//       rule: Ldc_immediate_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case5_TestCase5) {
  ForbiddenCondDecoderTester_Case5 baseline_tester;
  NamedForbidden_Ldc_immediate_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw1nnnnddddcccciiiiiiii");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=1111 & inst(25:20)=~000x01
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc110pudw11111ddddcccciiiiiiii',
//       rule: 'Ldc_literal_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc110pudw11111ddddcccciiiiiiii,
//       rule: Ldc_literal_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case6_TestCase6) {
  ForbiddenCondDecoderTester_Case6 baseline_tester;
  NamedForbidden_Ldc_literal_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw11111ddddcccciiiiiiii");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxxx & inst(4)=0
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc1110oooonnnnddddccccooo0mmmm',
//       rule: 'Cdp_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc1110oooonnnnddddccccooo0mmmm,
//       rule: Cdp_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case7_TestCase7) {
  ForbiddenCondDecoderTester_Case7 baseline_tester;
  NamedForbidden_Cdp_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110oooonnnnddddccccooo0mmmm");
}

// Neutral case:
// inst(25:20)=00000x
//    = {actual: 'Undefined',
//       baseline: 'UndefinedCondDecoder',
//       constraints: ,
//       pattern: 'cccc1100000xnnnnxxxxccccxxxoxxxx',
//       rule: 'Undefined_A5_6'}
//
// Representative case:
// op1(25:20)=00000x
//    = {actual: Undefined,
//       baseline: UndefinedCondDecoder,
//       constraints: ,
//       pattern: cccc1100000xnnnnxxxxccccxxxoxxxx,
//       rule: Undefined_A5_6}
TEST_F(Arm32DecoderStateTests,
       UndefinedCondDecoderTester_Case8_TestCase8) {
  UndefinedCondDecoderTester_Case8 baseline_tester;
  NamedUndefined_Undefined_A5_6 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1100000xnnnnxxxxccccxxxoxxxx");
}

// Neutral case:
// inst(25:20)=11xxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc1111iiiiiiiiiiiiiiiiiiiiiiii',
//       rule: 'Svc_Rule_A1'}
//
// Representative case:
// op1(25:20)=11xxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc1111iiiiiiiiiiiiiiiiiiiiiiii,
//       rule: Svc_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case9_TestCase9) {
  ForbiddenCondDecoderTester_Case9 baseline_tester;
  NamedForbidden_Svc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1111iiiiiiiiiiiiiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
