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
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTesterCase0
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase0(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTesterCase1
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTesterCase2
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase2(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTesterCase3
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20)=~10111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase4
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase4(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase5
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase5(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase6
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase6(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase7
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase7(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase8
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase9
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase9(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase10
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase10(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase11
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase12
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase12(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTesterCase13
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase13(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(6:5)=~00 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTesterCase14
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase14(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTesterCase15
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase15(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTesterCase16
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase16(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase17
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase17(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary3RegisterShiftedOpTesterCase18
    : public Unary3RegisterShiftedOpTesterRegsNotPc {
 public:
  Unary3RegisterShiftedOpTesterCase18(const NamedClassDecoder& decoder)
    : Unary3RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary3RegisterShiftedOpTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary3RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'Tst_Rule_232_A1_P458',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: Tst_Rule_232_A1_P458,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_Case0
    : public Binary3RegisterShiftedTestTesterCase0 {
 public:
  Binary3RegisterShiftedTestTester_Case0()
    : Binary3RegisterShiftedTestTesterCase0(
      state_.Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'Teq_Rule_229_A1_P452',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: Teq_Rule_229_A1_P452,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_Case1
    : public Binary3RegisterShiftedTestTesterCase1 {
 public:
  Binary3RegisterShiftedTestTester_Case1()
    : Binary3RegisterShiftedTestTesterCase1(
      state_.Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'Cmp_Rule_37_A1_P84',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: Cmp_Rule_37_A1_P84,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_Case2
    : public Binary3RegisterShiftedTestTesterCase2 {
 public:
  Binary3RegisterShiftedTestTester_Case2()
    : Binary3RegisterShiftedTestTesterCase2(
      state_.Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'Cmn_Rule_34_A1_P78',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: Cmn_Rule_34_A1_P78,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_Case3
    : public Binary3RegisterShiftedTestTesterCase3 {
 public:
  Binary3RegisterShiftedTestTester_Case3()
    : Binary3RegisterShiftedTestTesterCase3(
      state_.Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'And_Rule_13_A1_P38',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: And_Rule_13_A1_P38,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case4
    : public Binary4RegisterShiftedOpTesterCase4 {
 public:
  Binary4RegisterShiftedOpTester_Case4()
    : Binary4RegisterShiftedOpTesterCase4(
      state_.Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Eor_Rule_46_A1_P98',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Eor_Rule_46_A1_P98,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case5
    : public Binary4RegisterShiftedOpTesterCase5 {
 public:
  Binary4RegisterShiftedOpTester_Case5()
    : Binary4RegisterShiftedOpTesterCase5(
      state_.Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Sub_Rule_214_A1_P424',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Sub_Rule_214_A1_P424,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case6
    : public Binary4RegisterShiftedOpTesterCase6 {
 public:
  Binary4RegisterShiftedOpTester_Case6()
    : Binary4RegisterShiftedOpTesterCase6(
      state_.Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Rsb_Rule_144_A1_P288',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Rsb_Rule_144_A1_P288,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case7
    : public Binary4RegisterShiftedOpTesterCase7 {
 public:
  Binary4RegisterShiftedOpTester_Case7()
    : Binary4RegisterShiftedOpTesterCase7(
      state_.Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Add_Rule_7_A1_P26',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Add_Rule_7_A1_P26,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case8
    : public Binary4RegisterShiftedOpTesterCase8 {
 public:
  Binary4RegisterShiftedOpTester_Case8()
    : Binary4RegisterShiftedOpTesterCase8(
      state_.Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Adc_Rule_3_A1_P18',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Adc_Rule_3_A1_P18,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case9
    : public Binary4RegisterShiftedOpTesterCase9 {
 public:
  Binary4RegisterShiftedOpTester_Case9()
    : Binary4RegisterShiftedOpTesterCase9(
      state_.Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Sbc_Rule_153_A1_P306',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Sbc_Rule_153_A1_P306,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case10
    : public Binary4RegisterShiftedOpTesterCase10 {
 public:
  Binary4RegisterShiftedOpTester_Case10()
    : Binary4RegisterShiftedOpTesterCase10(
      state_.Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Rsc_Rule_147_A1_P294',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Rsc_Rule_147_A1_P294,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case11
    : public Binary4RegisterShiftedOpTesterCase11 {
 public:
  Binary4RegisterShiftedOpTester_Case11()
    : Binary4RegisterShiftedOpTesterCase11(
      state_.Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Orr_Rule_115_A1_P212',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Orr_Rule_115_A1_P212,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case12
    : public Binary4RegisterShiftedOpTesterCase12 {
 public:
  Binary4RegisterShiftedOpTester_Case12()
    : Binary4RegisterShiftedOpTesterCase12(
      state_.Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'Lsl_Rule_89_A1_P180',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: Lsl_Rule_89_A1_P180,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_Case13
    : public Binary3RegisterOpTesterCase13 {
 public:
  Binary3RegisterOpTester_Case13()
    : Binary3RegisterOpTesterCase13(
      state_.Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'Lsr_Rule_91_A1_P184',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: Lsr_Rule_91_A1_P184,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_Case14
    : public Binary3RegisterOpTesterCase14 {
 public:
  Binary3RegisterOpTester_Case14()
    : Binary3RegisterOpTesterCase14(
      state_.Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'Asr_Rule_15_A1_P42',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: Asr_Rule_15_A1_P42,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_Case15
    : public Binary3RegisterOpTesterCase15 {
 public:
  Binary3RegisterOpTester_Case15()
    : Binary3RegisterOpTesterCase15(
      state_.Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'Ror_Rule_140_A1_P280',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: Ror_Rule_140_A1_P280,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_Case16
    : public Binary3RegisterOpTesterCase16 {
 public:
  Binary3RegisterOpTester_Case16()
    : Binary3RegisterOpTesterCase16(
      state_.Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Bic_Rule_21_A1_P54',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Bic_Rule_21_A1_P54,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case17
    : public Binary4RegisterShiftedOpTesterCase17 {
 public:
  Binary4RegisterShiftedOpTester_Case17()
    : Binary4RegisterShiftedOpTesterCase17(
      state_.Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {'constraints': ,
//     'rule': 'Mvn_Rule_108_A1_P218',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {constraints: ,
//     rule: Mvn_Rule_108_A1_P218,
//     safety: ['RegsNotPc']}
class Unary3RegisterShiftedOpTester_Case18
    : public Unary3RegisterShiftedOpTesterCase18 {
 public:
  Unary3RegisterShiftedOpTester_Case18()
    : Unary3RegisterShiftedOpTesterCase18(
      state_.Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_)
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
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010001nnnn0000ssss0tt1mmmm',
//     'rule': 'Tst_Rule_232_A1_P458',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010001nnnn0000ssss0tt1mmmm,
//     rule: Tst_Rule_232_A1_P458,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case0_TestCase0) {
  Binary3RegisterShiftedTestTester_Case0 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010011nnnn0000ssss0tt1mmmm',
//     'rule': 'Teq_Rule_229_A1_P452',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010011nnnn0000ssss0tt1mmmm,
//     rule: Teq_Rule_229_A1_P452,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case1_TestCase1) {
  Binary3RegisterShiftedTestTester_Case1 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010101nnnn0000ssss0tt1mmmm',
//     'rule': 'Cmp_Rule_37_A1_P84',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010101nnnn0000ssss0tt1mmmm,
//     rule: Cmp_Rule_37_A1_P84,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case2_TestCase2) {
  Binary3RegisterShiftedTestTester_Case2 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010111nnnn0000ssss0tt1mmmm',
//     'rule': 'Cmn_Rule_34_A1_P78',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010111nnnn0000ssss0tt1mmmm,
//     rule: Cmn_Rule_34_A1_P78,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case3_TestCase3) {
  Binary3RegisterShiftedTestTester_Case3 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000000snnnnddddssss0tt1mmmm',
//     'rule': 'And_Rule_13_A1_P38',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000000snnnnddddssss0tt1mmmm,
//     rule: And_Rule_13_A1_P38,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case4_TestCase4) {
  Binary4RegisterShiftedOpTester_Case4 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp => Defs12To15CondsDontCareRdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000001snnnnddddssss0tt1mmmm',
//     'rule': 'Eor_Rule_46_A1_P98',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp => Defs12To15CondsDontCareRdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000001snnnnddddssss0tt1mmmm,
//     rule: Eor_Rule_46_A1_P98,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case5_TestCase5) {
  Binary4RegisterShiftedOpTester_Case5 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000010snnnnddddssss0tt1mmmm',
//     'rule': 'Sub_Rule_214_A1_P424',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000010snnnnddddssss0tt1mmmm,
//     rule: Sub_Rule_214_A1_P424,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case6_TestCase6) {
  Binary4RegisterShiftedOpTester_Case6 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000011snnnnddddssss0tt1mmmm',
//     'rule': 'Rsb_Rule_144_A1_P288',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000011snnnnddddssss0tt1mmmm,
//     rule: Rsb_Rule_144_A1_P288,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case7_TestCase7) {
  Binary4RegisterShiftedOpTester_Case7 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000100snnnnddddssss0tt1mmmm',
//     'rule': 'Add_Rule_7_A1_P26',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000100snnnnddddssss0tt1mmmm,
//     rule: Add_Rule_7_A1_P26,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case8_TestCase8) {
  Binary4RegisterShiftedOpTester_Case8 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000101snnnnddddssss0tt1mmmm',
//     'rule': 'Adc_Rule_3_A1_P18',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000101snnnnddddssss0tt1mmmm,
//     rule: Adc_Rule_3_A1_P18,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case9_TestCase9) {
  Binary4RegisterShiftedOpTester_Case9 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000110snnnnddddssss0tt1mmmm',
//     'rule': 'Sbc_Rule_153_A1_P306',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000110snnnnddddssss0tt1mmmm,
//     rule: Sbc_Rule_153_A1_P306,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case10_TestCase10) {
  Binary4RegisterShiftedOpTester_Case10 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000111snnnnddddssss0tt1mmmm',
//     'rule': 'Rsc_Rule_147_A1_P294',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000111snnnnddddssss0tt1mmmm,
//     rule: Rsc_Rule_147_A1_P294,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case11_TestCase11) {
  Binary4RegisterShiftedOpTester_Case11 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0001100snnnnddddssss0tt1mmmm',
//     'rule': 'Orr_Rule_115_A1_P212',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0001100snnnnddddssss0tt1mmmm,
//     rule: Orr_Rule_115_A1_P212,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case12_TestCase12) {
  Binary4RegisterShiftedOpTester_Case12 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0001nnnn',
//     'rule': 'Lsl_Rule_89_A1_P180',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0001nnnn,
//     rule: Lsl_Rule_89_A1_P180,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case13_TestCase13) {
  Binary3RegisterOpTester_Case13 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0001nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0011nnnn',
//     'rule': 'Lsr_Rule_91_A1_P184',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0011nnnn,
//     rule: Lsr_Rule_91_A1_P184,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case14_TestCase14) {
  Binary3RegisterOpTester_Case14 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0011nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0101nnnn',
//     'rule': 'Asr_Rule_15_A1_P42',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0101nnnn,
//     rule: Asr_Rule_15_A1_P42,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case15_TestCase15) {
  Binary3RegisterOpTester_Case15 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0101nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0111nnnn',
//     'rule': 'Ror_Rule_140_A1_P280',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0111nnnn,
//     rule: Ror_Rule_140_A1_P280,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case16_TestCase16) {
  Binary3RegisterOpTester_Case16 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0111nnnn");
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0001110snnnnddddssss0tt1mmmm',
//     'rule': 'Bic_Rule_21_A1_P54',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0001110snnnnddddssss0tt1mmmm,
//     rule: Bic_Rule_21_A1_P54,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case17_TestCase17) {
  Binary4RegisterShiftedOpTester_Case17 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001111s0000ddddssss0tt1mmmm',
//     'rule': 'Mvn_Rule_108_A1_P218',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001111s0000ddddssss0tt1mmmm,
//     rule: Mvn_Rule_108_A1_P218,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary3RegisterShiftedOpTester_Case18_TestCase18) {
  Unary3RegisterShiftedOpTester_Case18 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111s0000ddddssss0tt1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
