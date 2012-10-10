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
// inst(27:20)=11000100
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=11000100
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase0
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0FF00000) != 0x0C400000 /* op1(27:20)=~11000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=11000101
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=11000101
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase1
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0FF00000) != 0x0C500000 /* op1(27:20)=~11000101 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=100xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase2
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E500000) != 0x08100000 /* op1(27:20)=~100xx0x1 */) return false;
  if ((inst.Bits() & 0x0000FFFF) != 0x00000A00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000101000000000 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=100xx1x0 & inst(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase3
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E500000) != 0x08400000 /* op1(27:20)=~100xx1x0 */) return false;
  if ((inst.Bits() & 0x000FFFE0) != 0x000D0500 /* $pattern(31:0)=~xxxxxxxxxxxx110100000101000xxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=1110xxx0 & inst(4)=1
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=1110xxx0 & op(4)=1
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase4
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase4(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0F100000) != 0x0E000000 /* op1(27:20)=~1110xxx0 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000010 /* op(4)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=1110xxx1 & inst(4)=1
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=1110xxx1 & op(4)=1
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase5
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase5(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0F100000) != 0x0E100000 /* op1(27:20)=~1110xxx1 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000010 /* op(4)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=110xxxx0 & inst(27:20)=~11000x00
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase6
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase6(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E100000) != 0x0C000000 /* op1(27:20)=~110xxxx0 */) return false;
  if ((inst.Bits() & 0x0FB00000) == 0x0C000000 /* op1_repeated(27:20)=11000x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=~1111 & inst(27:20)=~11000x01
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase7
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase7(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E100000) != 0x0C100000 /* op1(27:20)=~110xxxx1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x0FB00000) == 0x0C100000 /* op1_repeated(27:20)=11000x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=1111 & inst(27:20)=~11000x01
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase8
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase8(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E100000) != 0x0C100000 /* op1(27:20)=~110xxxx1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x0FB00000) == 0x0C100000 /* op1_repeated(27:20)=11000x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=1110xxxx & inst(4)=0
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=1110xxxx & op(4)=0
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase9
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0F000000) != 0x0E000000 /* op1(27:20)=~1110xxxx */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op(4)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=101xxxxx
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(27:20)=101xxxxx
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase10
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E000000) != 0x0A000000 /* op1(27:20)=~101xxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(27:20)=11000100
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Mcrr2_Rule_93_A2_P188'}
//
// Representative case:
// op1(27:20)=11000100
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Mcrr2_Rule_93_A2_P188}
class ForbiddenUncondDecoderTester_Case0
    : public UnsafeUncondDecoderTesterCase0 {
 public:
  ForbiddenUncondDecoderTester_Case0()
    : UnsafeUncondDecoderTesterCase0(
      state_.ForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188_instance_)
  {}
};

// Neutral case:
// inst(27:20)=11000101
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Mrrc2_Rule_101_A2_P204'}
//
// Representative case:
// op1(27:20)=11000101
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Mrrc2_Rule_101_A2_P204}
class ForbiddenUncondDecoderTester_Case1
    : public UnsafeUncondDecoderTesterCase1 {
 public:
  ForbiddenUncondDecoderTester_Case1()
    : UnsafeUncondDecoderTesterCase1(
      state_.ForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204_instance_)
  {}
};

// Neutral case:
// inst(27:20)=100xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Rfe_Rule_B6_1_10_A1_B6_16'}
//
// Representative case:
// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Rfe_Rule_B6_1_10_A1_B6_16}
class ForbiddenUncondDecoderTester_Case2
    : public UnsafeUncondDecoderTesterCase2 {
 public:
  ForbiddenUncondDecoderTester_Case2()
    : UnsafeUncondDecoderTesterCase2(
      state_.ForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16_instance_)
  {}
};

// Neutral case:
// inst(27:20)=100xx1x0 & inst(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Srs_Rule_B6_1_10_A1_B6_20'}
//
// Representative case:
// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Srs_Rule_B6_1_10_A1_B6_20}
class ForbiddenUncondDecoderTester_Case3
    : public UnsafeUncondDecoderTesterCase3 {
 public:
  ForbiddenUncondDecoderTester_Case3()
    : UnsafeUncondDecoderTesterCase3(
      state_.ForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20_instance_)
  {}
};

// Neutral case:
// inst(27:20)=1110xxx0 & inst(4)=1
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Mcr2_Rule_92_A2_P186'}
//
// Representative case:
// op1(27:20)=1110xxx0 & op(4)=1
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Mcr2_Rule_92_A2_P186}
class ForbiddenUncondDecoderTester_Case4
    : public UnsafeUncondDecoderTesterCase4 {
 public:
  ForbiddenUncondDecoderTester_Case4()
    : UnsafeUncondDecoderTesterCase4(
      state_.ForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186_instance_)
  {}
};

// Neutral case:
// inst(27:20)=1110xxx1 & inst(4)=1
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Mrc2_Rule_100_A2_P202'}
//
// Representative case:
// op1(27:20)=1110xxx1 & op(4)=1
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Mrc2_Rule_100_A2_P202}
class ForbiddenUncondDecoderTester_Case5
    : public UnsafeUncondDecoderTesterCase5 {
 public:
  ForbiddenUncondDecoderTester_Case5()
    : UnsafeUncondDecoderTesterCase5(
      state_.ForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202_instance_)
  {}
};

// Neutral case:
// inst(27:20)=110xxxx0 & inst(27:20)=~11000x00
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Stc2_Rule_188_A2_P372'}
//
// Representative case:
// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Stc2_Rule_188_A2_P372}
class ForbiddenUncondDecoderTester_Case6
    : public UnsafeUncondDecoderTesterCase6 {
 public:
  ForbiddenUncondDecoderTester_Case6()
    : UnsafeUncondDecoderTesterCase6(
      state_.ForbiddenUncondDecoder_Stc2_Rule_188_A2_P372_instance_)
  {}
};

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=~1111 & inst(27:20)=~11000x01
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Ldc2_Rule_51_A2_P106'}
//
// Representative case:
// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Ldc2_Rule_51_A2_P106}
class ForbiddenUncondDecoderTester_Case7
    : public UnsafeUncondDecoderTesterCase7 {
 public:
  ForbiddenUncondDecoderTester_Case7()
    : UnsafeUncondDecoderTesterCase7(
      state_.ForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106_instance_)
  {}
};

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=1111 & inst(27:20)=~11000x01
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Ldc2_Rule_52_A2_P108'}
//
// Representative case:
// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Ldc2_Rule_52_A2_P108}
class ForbiddenUncondDecoderTester_Case8
    : public UnsafeUncondDecoderTesterCase8 {
 public:
  ForbiddenUncondDecoderTester_Case8()
    : UnsafeUncondDecoderTesterCase8(
      state_.ForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108_instance_)
  {}
};

// Neutral case:
// inst(27:20)=1110xxxx & inst(4)=0
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Cdp2_Rule_28_A2_P68'}
//
// Representative case:
// op1(27:20)=1110xxxx & op(4)=0
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Cdp2_Rule_28_A2_P68}
class ForbiddenUncondDecoderTester_Case9
    : public UnsafeUncondDecoderTesterCase9 {
 public:
  ForbiddenUncondDecoderTester_Case9()
    : UnsafeUncondDecoderTesterCase9(
      state_.ForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68_instance_)
  {}
};

// Neutral case:
// inst(27:20)=101xxxxx
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Blx_Rule_23_A2_P58'}
//
// Representative case:
// op1(27:20)=101xxxxx
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Blx_Rule_23_A2_P58}
class ForbiddenUncondDecoderTester_Case10
    : public UnsafeUncondDecoderTesterCase10 {
 public:
  ForbiddenUncondDecoderTester_Case10()
    : UnsafeUncondDecoderTesterCase10(
      state_.ForbiddenUncondDecoder_Blx_Rule_23_A2_P58_instance_)
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
// inst(27:20)=11000100
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '111111000100ssssttttiiiiiiiiiiii',
//       rule: 'Mcrr2_Rule_93_A2_P188'}
//
// Representative case:
// op1(27:20)=11000100
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 111111000100ssssttttiiiiiiiiiiii,
//       rule: Mcrr2_Rule_93_A2_P188}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case0_TestCase0) {
  ForbiddenUncondDecoderTester_Case0 baseline_tester;
  NamedForbidden_Mcrr2_Rule_93_A2_P188 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111111000100ssssttttiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=11000101
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '111111000101ssssttttiiiiiiiiiiii',
//       rule: 'Mrrc2_Rule_101_A2_P204'}
//
// Representative case:
// op1(27:20)=11000101
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 111111000101ssssttttiiiiiiiiiiii,
//       rule: Mrrc2_Rule_101_A2_P204}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case1_TestCase1) {
  ForbiddenUncondDecoderTester_Case1 baseline_tester;
  NamedForbidden_Mrrc2_Rule_101_A2_P204 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111111000101ssssttttiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=100xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '1111100pu0w1nnnn0000101000000000',
//       rule: 'Rfe_Rule_B6_1_10_A1_B6_16'}
//
// Representative case:
// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 1111100pu0w1nnnn0000101000000000,
//       rule: Rfe_Rule_B6_1_10_A1_B6_16}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case2_TestCase2) {
  ForbiddenUncondDecoderTester_Case2 baseline_tester;
  NamedForbidden_Rfe_Rule_B6_1_10_A1_B6_16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111100pu0w1nnnn0000101000000000");
}

// Neutral case:
// inst(27:20)=100xx1x0 & inst(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '1111100pu1w0110100000101000iiiii',
//       rule: 'Srs_Rule_B6_1_10_A1_B6_20'}
//
// Representative case:
// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 1111100pu1w0110100000101000iiiii,
//       rule: Srs_Rule_B6_1_10_A1_B6_20}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case3_TestCase3) {
  ForbiddenUncondDecoderTester_Case3 baseline_tester;
  NamedForbidden_Srs_Rule_B6_1_10_A1_B6_20 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111100pu1w0110100000101000iiiii");
}

// Neutral case:
// inst(27:20)=1110xxx0 & inst(4)=1
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '11111110iii0iiiittttiiiiiii1iiii',
//       rule: 'Mcr2_Rule_92_A2_P186'}
//
// Representative case:
// op1(27:20)=1110xxx0 & op(4)=1
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 11111110iii0iiiittttiiiiiii1iiii,
//       rule: Mcr2_Rule_92_A2_P186}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case4_TestCase4) {
  ForbiddenUncondDecoderTester_Case4 baseline_tester;
  NamedForbidden_Mcr2_Rule_92_A2_P186 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iii0iiiittttiiiiiii1iiii");
}

// Neutral case:
// inst(27:20)=1110xxx1 & inst(4)=1
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '11111110iii1iiiittttiiiiiii1iiii',
//       rule: 'Mrc2_Rule_100_A2_P202'}
//
// Representative case:
// op1(27:20)=1110xxx1 & op(4)=1
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 11111110iii1iiiittttiiiiiii1iiii,
//       rule: Mrc2_Rule_100_A2_P202}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case5_TestCase5) {
  ForbiddenUncondDecoderTester_Case5 baseline_tester;
  NamedForbidden_Mrc2_Rule_100_A2_P202 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iii1iiiittttiiiiiii1iiii");
}

// Neutral case:
// inst(27:20)=110xxxx0 & inst(27:20)=~11000x00
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '1111110pudw0nnnniiiiiiiiiiiiiiii',
//       rule: 'Stc2_Rule_188_A2_P372'}
//
// Representative case:
// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 1111110pudw0nnnniiiiiiiiiiiiiiii,
//       rule: Stc2_Rule_188_A2_P372}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case6_TestCase6) {
  ForbiddenUncondDecoderTester_Case6 baseline_tester;
  NamedForbidden_Stc2_Rule_188_A2_P372 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw0nnnniiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=~1111 & inst(27:20)=~11000x01
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '1111110pudw1nnnniiiiiiiiiiiiiiii',
//       rule: 'Ldc2_Rule_51_A2_P106'}
//
// Representative case:
// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 1111110pudw1nnnniiiiiiiiiiiiiiii,
//       rule: Ldc2_Rule_51_A2_P106}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case7_TestCase7) {
  ForbiddenUncondDecoderTester_Case7 baseline_tester;
  NamedForbidden_Ldc2_Rule_51_A2_P106 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw1nnnniiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=1111 & inst(27:20)=~11000x01
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '1111110pudw11111iiiiiiiiiiiiiiii',
//       rule: 'Ldc2_Rule_52_A2_P108'}
//
// Representative case:
// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 1111110pudw11111iiiiiiiiiiiiiiii,
//       rule: Ldc2_Rule_52_A2_P108}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case8_TestCase8) {
  ForbiddenUncondDecoderTester_Case8 baseline_tester;
  NamedForbidden_Ldc2_Rule_52_A2_P108 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw11111iiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=1110xxxx & inst(4)=0
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '11111110iiiiiiiiiiiiiiiiiii0iiii',
//       rule: 'Cdp2_Rule_28_A2_P68'}
//
// Representative case:
// op1(27:20)=1110xxxx & op(4)=0
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 11111110iiiiiiiiiiiiiiiiiii0iiii,
//       rule: Cdp2_Rule_28_A2_P68}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case9_TestCase9) {
  ForbiddenUncondDecoderTester_Case9 baseline_tester;
  NamedForbidden_Cdp2_Rule_28_A2_P68 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iiiiiiiiiiiiiiiiiii0iiii");
}

// Neutral case:
// inst(27:20)=101xxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '1111101hiiiiiiiiiiiiiiiiiiiiiiii',
//       rule: 'Blx_Rule_23_A2_P58'}
//
// Representative case:
// op1(27:20)=101xxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 1111101hiiiiiiiiiiiiiiiiiiiiiiii,
//       rule: Blx_Rule_23_A2_P58}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case10_TestCase10) {
  ForbiddenUncondDecoderTester_Case10 baseline_tester;
  NamedForbidden_Blx_Rule_23_A2_P58 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111101hiiiiiiiiiiiiiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
