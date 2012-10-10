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
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000000 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'CondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: CondDecoder,
//       constraints: }
class CondDecoderTesterCase0
    : public CondDecoderTester {
 public:
  CondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000000 /* op2(7:0)=~00000000 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return CondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'CondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: CondDecoder,
//       constraints: }
class CondDecoderTesterCase1
    : public CondDecoderTester {
 public:
  CondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000001 /* op2(7:0)=~00000001 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return CondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
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
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000002 /* op2(7:0)=~00000010 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
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
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000003 /* op2(7:0)=~00000011 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
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
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000004 /* op2(7:0)=~00000100 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
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
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x000000F0 /* op2(7:0)=~1111xxxx */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'MoveImmediate12ToApsr',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: MoveImmediate12ToApsr,
//       constraints: }
class MoveImmediate12ToApsrTesterCase6
    : public MoveImmediate12ToApsrTester {
 public:
  MoveImmediate12ToApsrTesterCase6(const NamedClassDecoder& decoder)
    : MoveImmediate12ToApsrTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveImmediate12ToApsrTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00040000 /* op1(19:16)=~0100 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'MoveImmediate12ToApsr',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: MoveImmediate12ToApsr,
//       constraints: }
class MoveImmediate12ToApsrTesterCase7
    : public MoveImmediate12ToApsrTester {
 public:
  MoveImmediate12ToApsrTesterCase7(const NamedClassDecoder& decoder)
    : MoveImmediate12ToApsrTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveImmediate12ToApsrTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000B0000) != 0x00080000 /* op1(19:16)=~1x00 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
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
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00010000 /* op1(19:16)=~xx01 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
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
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x00020000) != 0x00020000 /* op1(19:16)=~xx1x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase10
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00400000 /* op(22)=~1 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000000 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'CondDecoder',
//       constraints: ,
//       rule: 'Nop_Rule_110_A1_P222'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: CondDecoder,
//       constraints: ,
//       rule: Nop_Rule_110_A1_P222}
class CondDecoderTester_Case0
    : public CondDecoderTesterCase0 {
 public:
  CondDecoderTester_Case0()
    : CondDecoderTesterCase0(
      state_.CondDecoder_Nop_Rule_110_A1_P222_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'CondDecoder',
//       constraints: ,
//       rule: 'Yield_Rule_413_A1_P812'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: CondDecoder,
//       constraints: ,
//       rule: Yield_Rule_413_A1_P812}
class CondDecoderTester_Case1
    : public CondDecoderTesterCase1 {
 public:
  CondDecoderTester_Case1()
    : CondDecoderTesterCase1(
      state_.CondDecoder_Yield_Rule_413_A1_P812_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Wfe_Rule_411_A1_P808'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Wfe_Rule_411_A1_P808}
class ForbiddenCondDecoderTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenCondDecoderTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.ForbiddenCondDecoder_Wfe_Rule_411_A1_P808_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Wfi_Rule_412_A1_P810'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Wfi_Rule_412_A1_P810}
class ForbiddenCondDecoderTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenCondDecoderTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.ForbiddenCondDecoder_Wfi_Rule_412_A1_P810_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Sev_Rule_158_A1_P316'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Sev_Rule_158_A1_P316}
class ForbiddenCondDecoderTester_Case4
    : public UnsafeCondDecoderTesterCase4 {
 public:
  ForbiddenCondDecoderTester_Case4()
    : UnsafeCondDecoderTesterCase4(
      state_.ForbiddenCondDecoder_Sev_Rule_158_A1_P316_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Dbg_Rule_40_A1_P88'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Dbg_Rule_40_A1_P88}
class ForbiddenCondDecoderTester_Case5
    : public UnsafeCondDecoderTesterCase5 {
 public:
  ForbiddenCondDecoderTester_Case5()
    : UnsafeCondDecoderTesterCase5(
      state_.ForbiddenCondDecoder_Dbg_Rule_40_A1_P88_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       rule: 'Msr_Rule_103_A1_P208'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       rule: Msr_Rule_103_A1_P208}
class MoveImmediate12ToApsrTester_Case6
    : public MoveImmediate12ToApsrTesterCase6 {
 public:
  MoveImmediate12ToApsrTester_Case6()
    : MoveImmediate12ToApsrTesterCase6(
      state_.MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       rule: 'Msr_Rule_103_A1_P208'}
//
// Representative case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       rule: Msr_Rule_103_A1_P208}
class MoveImmediate12ToApsrTester_Case7
    : public MoveImmediate12ToApsrTesterCase7 {
 public:
  MoveImmediate12ToApsrTester_Case7()
    : MoveImmediate12ToApsrTesterCase7(
      state_.MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Msr_Rule_B6_1_6_A1_PB6_12}
class ForbiddenCondDecoderTester_Case8
    : public UnsafeCondDecoderTesterCase8 {
 public:
  ForbiddenCondDecoderTester_Case8()
    : UnsafeCondDecoderTesterCase8(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Msr_Rule_B6_1_6_A1_PB6_12}
class ForbiddenCondDecoderTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenCondDecoderTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Msr_Rule_B6_1_6_A1_PB6_12}
class ForbiddenCondDecoderTester_Case10
    : public UnsafeCondDecoderTesterCase10 {
 public:
  ForbiddenCondDecoderTester_Case10()
    : UnsafeCondDecoderTesterCase10(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
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
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000000 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'CondDecoder',
//       constraints: ,
//       pattern: 'cccc0011001000001111000000000000',
//       rule: 'Nop_Rule_110_A1_P222'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: DontCareInst,
//       baseline: CondDecoder,
//       constraints: ,
//       pattern: cccc0011001000001111000000000000,
//       rule: Nop_Rule_110_A1_P222}
TEST_F(Arm32DecoderStateTests,
       CondDecoderTester_Case0_TestCase0) {
  CondDecoderTester_Case0 baseline_tester;
  NamedDontCareInst_Nop_Rule_110_A1_P222 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000000");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'CondDecoder',
//       constraints: ,
//       pattern: 'cccc0011001000001111000000000001',
//       rule: 'Yield_Rule_413_A1_P812'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: DontCareInst,
//       baseline: CondDecoder,
//       constraints: ,
//       pattern: cccc0011001000001111000000000001,
//       rule: Yield_Rule_413_A1_P812}
TEST_F(Arm32DecoderStateTests,
       CondDecoderTester_Case1_TestCase1) {
  CondDecoderTester_Case1 baseline_tester;
  NamedDontCareInst_Yield_Rule_413_A1_P812 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000001");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0011001000001111000000000010',
//       rule: 'Wfe_Rule_411_A1_P808'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0011001000001111000000000010,
//       rule: Wfe_Rule_411_A1_P808}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case2_TestCase2) {
  ForbiddenCondDecoderTester_Case2 baseline_tester;
  NamedForbidden_Wfe_Rule_411_A1_P808 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000010");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0011001000001111000000000011',
//       rule: 'Wfi_Rule_412_A1_P810'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0011001000001111000000000011,
//       rule: Wfi_Rule_412_A1_P810}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case3_TestCase3) {
  ForbiddenCondDecoderTester_Case3 baseline_tester;
  NamedForbidden_Wfi_Rule_412_A1_P810 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000011");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0011001000001111000000000100',
//       rule: 'Sev_Rule_158_A1_P316'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0011001000001111000000000100,
//       rule: Sev_Rule_158_A1_P316}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case4_TestCase4) {
  ForbiddenCondDecoderTester_Case4 baseline_tester;
  NamedForbidden_Sev_Rule_158_A1_P316 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000100");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc001100100000111100001111iiii',
//       rule: 'Dbg_Rule_40_A1_P88'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc001100100000111100001111iiii,
//       rule: Dbg_Rule_40_A1_P88}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case5_TestCase5) {
  ForbiddenCondDecoderTester_Case5 baseline_tester;
  NamedForbidden_Dbg_Rule_40_A1_P88 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100100000111100001111iiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       pattern: 'cccc0011001001001111iiiiiiiiiiii',
//       rule: 'Msr_Rule_103_A1_P208'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: DontCareInst,
//       baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       pattern: cccc0011001001001111iiiiiiiiiiii,
//       rule: Msr_Rule_103_A1_P208}
TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_Case6_TestCase6) {
  MoveImmediate12ToApsrTester_Case6 baseline_tester;
  NamedDontCareInst_Msr_Rule_103_A1_P208 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001001001111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       pattern: 'cccc001100101x001111iiiiiiiiiiii',
//       rule: 'Msr_Rule_103_A1_P208'}
//
// Representative case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: DontCareInst,
//       baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       pattern: cccc001100101x001111iiiiiiiiiiii,
//       rule: Msr_Rule_103_A1_P208}
TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_Case7_TestCase7) {
  MoveImmediate12ToApsrTester_Case7 baseline_tester;
  NamedDontCareInst_Msr_Rule_103_A1_P208 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100101x001111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00110010ii011111iiiiiiiiiiii',
//       rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00110010ii011111iiiiiiiiiiii,
//       rule: Msr_Rule_B6_1_6_A1_PB6_12}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case8_TestCase8) {
  ForbiddenCondDecoderTester_Case8 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii011111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00110010ii1i1111iiiiiiiiiiii',
//       rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00110010ii1i1111iiiiiiiiiiii,
//       rule: Msr_Rule_B6_1_6_A1_PB6_12}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case9_TestCase9) {
  ForbiddenCondDecoderTester_Case9 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii1i1111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00110110iiii1111iiiiiiiiiiii',
//       rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00110110iiii1111iiiiiiiiiiii,
//       rule: Msr_Rule_B6_1_6_A1_PB6_12}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case10_TestCase10) {
  ForbiddenCondDecoderTester_Case10 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110110iiii1111iiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
