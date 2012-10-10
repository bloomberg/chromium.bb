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
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'Unary1RegisterUse',
//       constraints: }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: Unary1RegisterUse,
//       constraints: }
class Unary1RegisterUseTesterCase0
    : public Unary1RegisterUseTester {
 public:
  Unary1RegisterUseTesterCase0(const NamedClassDecoder& decoder)
    : Unary1RegisterUseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterUseTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00000000 /* op1(19:16)=~xx00 */) return false;
  if ((inst.Bits() & 0x0000FD00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterUseTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
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
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00010000 /* op1(19:16)=~xx01 */) return false;
  if ((inst.Bits() & 0x0000FD00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
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
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00020000) != 0x00020000 /* op1(19:16)=~xx1x */) return false;
  if ((inst.Bits() & 0x0000FD00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
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
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x0000FD00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {baseline: 'Unary1RegisterSet',
//       constraints: }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {baseline: Unary1RegisterSet,
//       constraints: }
class Unary1RegisterSetTesterCase4
    : public Unary1RegisterSetTester {
 public:
  Unary1RegisterSetTesterCase4(const NamedClassDecoder& decoder)
    : Unary1RegisterSetTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterSetTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* op(22:21)=~x0 */) return false;
  if ((inst.Bits() & 0x000F0D0F) != 0x000F0000 /* $pattern(31:0)=~xxxxxxxxxxxx1111xxxx00x0xxxx0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterSetTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
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
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000200 /* B(9)=~1 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* op(22:21)=~x0 */) return false;
  if ((inst.Bits() & 0x00000C0F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx00xxxxxx0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
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
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000200 /* B(9)=~1 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00200000 /* op(22:21)=~x1 */) return false;
  if ((inst.Bits() & 0x0000FC00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100xxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'BranchToRegister',
//       constraints: }
//
// Representaive case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: BranchToRegister,
//       constraints: }
class BranchToRegisterTesterCase7
    : public BranchToRegisterTester {
 public:
  BranchToRegisterTesterCase7(const NamedClassDecoder& decoder)
    : BranchToRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchToRegisterTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000010 /* op2(6:4)=~001 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FFF00 /* $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPc',
//       constraints: }
//
// Representaive case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPc,
//       constraints: }
class Unary2RegisterOpNotRmIsPcTesterCase8
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase8(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000010 /* op2(6:4)=~001 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x000F0F00) != 0x000F0F00 /* $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
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
  if ((inst.Bits() & 0x00000070) != 0x00000020 /* op2(6:4)=~010 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FFF00 /* $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'BranchToRegister',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: BranchToRegister,
//       constraints: ,
//       safety: ['RegsNotPc']}
class BranchToRegisterTesterCase10
    : public BranchToRegisterTesterRegsNotPc {
 public:
  BranchToRegisterTesterCase10(const NamedClassDecoder& decoder)
    : BranchToRegisterTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchToRegisterTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000030 /* op2(6:4)=~011 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FFF00 /* $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase11
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase11(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000060 /* op2(6:4)=~110 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x000FFF0F) != 0x0000000E /* $pattern(31:0)=~xxxxxxxxxxxx000000000000xxxx1110 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = {baseline: 'BreakPointAndConstantPoolHead',
//       constraints: }
//
// Representaive case:
// op2(6:4)=111 & op(22:21)=01
//    = {baseline: BreakPointAndConstantPoolHead,
//       constraints: }
class Immediate16UseTesterCase12
    : public Immediate16UseTester {
 public:
  Immediate16UseTesterCase12(const NamedClassDecoder& decoder)
    : Immediate16UseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Immediate16UseTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4)=~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Immediate16UseTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=111 & op(22:21)=10
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase13
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase13(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4)=~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op(22:21)=~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase14
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase14(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4)=~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx000000000000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'Unary1RegisterUse',
//       constraints: ,
//       rule: 'Msr_Rule_104_A1_P210'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: Unary1RegisterUse,
//       constraints: ,
//       rule: Msr_Rule_104_A1_P210}
class Unary1RegisterUseTester_Case0
    : public Unary1RegisterUseTesterCase0 {
 public:
  Unary1RegisterUseTester_Case0()
    : Unary1RegisterUseTesterCase0(
      state_.Unary1RegisterUse_Msr_Rule_104_A1_P210_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Msr_Rule_B6_1_7_P14}
class ForbiddenCondDecoderTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenCondDecoderTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Msr_Rule_B6_1_7_P14}
class ForbiddenCondDecoderTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenCondDecoderTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Msr_Rule_B6_1_7_P14}
class ForbiddenCondDecoderTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenCondDecoderTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {baseline: 'Unary1RegisterSet',
//       constraints: ,
//       rule: 'Mrs_Rule_102_A1_P206_Or_B6_10'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {baseline: Unary1RegisterSet,
//       constraints: ,
//       rule: Mrs_Rule_102_A1_P206_Or_B6_10}
class Unary1RegisterSetTester_Case4
    : public Unary1RegisterSetTesterCase4 {
 public:
  Unary1RegisterSetTester_Case4()
    : Unary1RegisterSetTesterCase4(
      state_.Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Msr_Rule_Banked_register_A1_B9_1990'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Msr_Rule_Banked_register_A1_B9_1990}
class ForbiddenCondDecoderTester_Case5
    : public UnsafeCondDecoderTesterCase5 {
 public:
  ForbiddenCondDecoderTester_Case5()
    : UnsafeCondDecoderTesterCase5(
      state_.ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Msr_Rule_Banked_register_A1_B9_1992'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Msr_Rule_Banked_register_A1_B9_1992}
class ForbiddenCondDecoderTester_Case6
    : public UnsafeCondDecoderTesterCase6 {
 public:
  ForbiddenCondDecoderTester_Case6()
    : UnsafeCondDecoderTesterCase6(
      state_.ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992_instance_)
  {}
};

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'BranchToRegister',
//       constraints: ,
//       rule: 'Bx_Rule_25_A1_P62'}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: BranchToRegister,
//       constraints: ,
//       rule: Bx_Rule_25_A1_P62}
class BranchToRegisterTester_Case7
    : public BranchToRegisterTesterCase7 {
 public:
  BranchToRegisterTester_Case7()
    : BranchToRegisterTesterCase7(
      state_.BranchToRegister_Bx_Rule_25_A1_P62_instance_)
  {}
};

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPc',
//       constraints: ,
//       rule: 'Clz_Rule_31_A1_P72'}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPc,
//       constraints: ,
//       rule: Clz_Rule_31_A1_P72}
class Unary2RegisterOpNotRmIsPcTester_Case8
    : public Unary2RegisterOpNotRmIsPcTesterCase8 {
 public:
  Unary2RegisterOpNotRmIsPcTester_Case8()
    : Unary2RegisterOpNotRmIsPcTesterCase8(
      state_.Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72_instance_)
  {}
};

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Bxj_Rule_26_A1_P64'}
//
// Representative case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Bxj_Rule_26_A1_P64}
class ForbiddenCondDecoderTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenCondDecoderTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.ForbiddenCondDecoder_Bxj_Rule_26_A1_P64_instance_)
  {}
};

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'BranchToRegister',
//       constraints: ,
//       rule: 'Blx_Rule_24_A1_P60',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: BranchToRegister,
//       constraints: ,
//       rule: Blx_Rule_24_A1_P60,
//       safety: ['RegsNotPc']}
class BranchToRegisterTester_Case10
    : public BranchToRegisterTesterCase10 {
 public:
  BranchToRegisterTester_Case10()
    : BranchToRegisterTesterCase10(
      state_.BranchToRegister_Blx_Rule_24_A1_P60_instance_)
  {}
};

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Eret_Rule_A1'}
//
// Representative case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Eret_Rule_A1}
class ForbiddenCondDecoderTester_Case11
    : public UnsafeCondDecoderTesterCase11 {
 public:
  ForbiddenCondDecoderTester_Case11()
    : UnsafeCondDecoderTesterCase11(
      state_.ForbiddenCondDecoder_Eret_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = {baseline: 'BreakPointAndConstantPoolHead',
//       constraints: ,
//       rule: 'Bkpt_Rule_22_A1_P56'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=01
//    = {baseline: BreakPointAndConstantPoolHead,
//       constraints: ,
//       rule: Bkpt_Rule_22_A1_P56}
class BreakPointAndConstantPoolHeadTester_Case12
    : public Immediate16UseTesterCase12 {
 public:
  BreakPointAndConstantPoolHeadTester_Case12()
    : Immediate16UseTesterCase12(
      state_.BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Hvc_Rule_A1'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=10
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Hvc_Rule_A1}
class ForbiddenCondDecoderTester_Case13
    : public UnsafeCondDecoderTesterCase13 {
 public:
  ForbiddenCondDecoderTester_Case13()
    : UnsafeCondDecoderTesterCase13(
      state_.ForbiddenCondDecoder_Hvc_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'Smc_Rule_B6_1_9_P18'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: Smc_Rule_B6_1_9_P18}
class ForbiddenCondDecoderTester_Case14
    : public UnsafeCondDecoderTesterCase14 {
 public:
  ForbiddenCondDecoderTester_Case14()
    : UnsafeCondDecoderTesterCase14(
      state_.ForbiddenCondDecoder_Smc_Rule_B6_1_9_P18_instance_)
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
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: 'Unary1RegisterUse',
//       baseline: 'Unary1RegisterUse',
//       constraints: ,
//       pattern: 'cccc00010010mm00111100000000nnnn',
//       rule: 'Msr_Rule_104_A1_P210'}
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Unary1RegisterUse,
//       baseline: Unary1RegisterUse,
//       constraints: ,
//       pattern: cccc00010010mm00111100000000nnnn,
//       rule: Msr_Rule_104_A1_P210}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterUseTester_Case0_TestCase0) {
  Unary1RegisterUseTester_Case0 tester;
  tester.Test("cccc00010010mm00111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010010mm01111100000000nnnn',
//       rule: 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010010mm01111100000000nnnn,
//       rule: Msr_Rule_B6_1_7_P14}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case1_TestCase1) {
  ForbiddenCondDecoderTester_Case1 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm01111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010010mm1m111100000000nnnn',
//       rule: 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010010mm1m111100000000nnnn,
//       rule: Msr_Rule_B6_1_7_P14}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case2_TestCase2) {
  ForbiddenCondDecoderTester_Case2 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm1m111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010110mmmm111100000000nnnn',
//       rule: 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010110mmmm111100000000nnnn,
//       rule: Msr_Rule_B6_1_7_P14}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case3_TestCase3) {
  ForbiddenCondDecoderTester_Case3 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110mmmm111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {actual: 'Unary1RegisterSet',
//       baseline: 'Unary1RegisterSet',
//       constraints: ,
//       pattern: 'cccc00010r001111dddd000000000000',
//       rule: 'Mrs_Rule_102_A1_P206_Or_B6_10'}
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {actual: Unary1RegisterSet,
//       baseline: Unary1RegisterSet,
//       constraints: ,
//       pattern: cccc00010r001111dddd000000000000,
//       rule: Mrs_Rule_102_A1_P206_Or_B6_10}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterSetTester_Case4_TestCase4) {
  Unary1RegisterSetTester_Case4 tester;
  tester.Test("cccc00010r001111dddd000000000000");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010r00mmmmdddd001m00000000',
//       rule: 'Msr_Rule_Banked_register_A1_B9_1990'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r00mmmmdddd001m00000000,
//       rule: Msr_Rule_Banked_register_A1_B9_1990}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case5_TestCase5) {
  ForbiddenCondDecoderTester_Case5 baseline_tester;
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1990 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r00mmmmdddd001m00000000");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010r10mmmm1111001m0000nnnn',
//       rule: 'Msr_Rule_Banked_register_A1_B9_1992'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm1111001m0000nnnn,
//       rule: Msr_Rule_Banked_register_A1_B9_1992}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case6_TestCase6) {
  ForbiddenCondDecoderTester_Case6 baseline_tester;
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1992 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm1111001m0000nnnn");
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: 'BxBlx',
//       baseline: 'BranchToRegister',
//       constraints: ,
//       pattern: 'cccc000100101111111111110001mmmm',
//       rule: 'Bx_Rule_25_A1_P62'}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: BxBlx,
//       baseline: BranchToRegister,
//       constraints: ,
//       pattern: cccc000100101111111111110001mmmm,
//       rule: Bx_Rule_25_A1_P62}
TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_Case7_TestCase7) {
  BranchToRegisterTester_Case7 baseline_tester;
  NamedBxBlx_Bx_Rule_25_A1_P62 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110001mmmm");
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Defs12To15RdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPc',
//       constraints: ,
//       pattern: 'cccc000101101111dddd11110001mmmm',
//       rule: 'Clz_Rule_31_A1_P72'}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: Defs12To15RdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPc,
//       constraints: ,
//       pattern: cccc000101101111dddd11110001mmmm,
//       rule: Clz_Rule_31_A1_P72}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_Case8_TestCase8) {
  Unary2RegisterOpNotRmIsPcTester_Case8 baseline_tester;
  NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101101111dddd11110001mmmm");
}

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc000100101111111111110010mmmm',
//       rule: 'Bxj_Rule_26_A1_P64'}
//
// Representative case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000100101111111111110010mmmm,
//       rule: Bxj_Rule_26_A1_P64}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case9_TestCase9) {
  ForbiddenCondDecoderTester_Case9 baseline_tester;
  NamedForbidden_Bxj_Rule_26_A1_P64 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110010mmmm");
}

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: 'BxBlx',
//       baseline: 'BranchToRegister',
//       constraints: ,
//       pattern: 'cccc000100101111111111110011mmmm',
//       rule: 'Blx_Rule_24_A1_P60',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: BxBlx,
//       baseline: BranchToRegister,
//       constraints: ,
//       pattern: cccc000100101111111111110011mmmm,
//       rule: Blx_Rule_24_A1_P60,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_Case10_TestCase10) {
  BranchToRegisterTester_Case10 baseline_tester;
  NamedBxBlx_Blx_Rule_24_A1_P60 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110011mmmm");
}

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0001011000000000000001101110',
//       rule: 'Eret_Rule_A1'}
//
// Representative case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0001011000000000000001101110,
//       rule: Eret_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case11_TestCase11) {
  ForbiddenCondDecoderTester_Case11 baseline_tester;
  NamedForbidden_Eret_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001011000000000000001101110");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = {actual: 'Breakpoint',
//       baseline: 'BreakPointAndConstantPoolHead',
//       constraints: ,
//       pattern: 'cccc00010010iiiiiiiiiiii0111iiii',
//       rule: 'Bkpt_Rule_22_A1_P56'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=01
//    = {actual: Breakpoint,
//       baseline: BreakPointAndConstantPoolHead,
//       constraints: ,
//       pattern: cccc00010010iiiiiiiiiiii0111iiii,
//       rule: Bkpt_Rule_22_A1_P56}
TEST_F(Arm32DecoderStateTests,
       BreakPointAndConstantPoolHeadTester_Case12_TestCase12) {
  BreakPointAndConstantPoolHeadTester_Case12 baseline_tester;
  NamedBreakpoint_Bkpt_Rule_22_A1_P56 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010iiiiiiiiiiii0111iiii");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010100iiiiiiiiiiii0111iiii',
//       rule: 'Hvc_Rule_A1'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=10
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010100iiiiiiiiiiii0111iiii,
//       rule: Hvc_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case13_TestCase13) {
  ForbiddenCondDecoderTester_Case13 baseline_tester;
  NamedForbidden_Hvc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100iiiiiiiiiiii0111iiii");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc000101100000000000000111iiii',
//       rule: 'Smc_Rule_B6_1_9_P18'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000101100000000000000111iiii,
//       rule: Smc_Rule_B6_1_9_P18}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case14_TestCase14) {
  ForbiddenCondDecoderTester_Case14 baseline_tester;
  NamedForbidden_Smc_Rule_B6_1_9_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101100000000000000111iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
