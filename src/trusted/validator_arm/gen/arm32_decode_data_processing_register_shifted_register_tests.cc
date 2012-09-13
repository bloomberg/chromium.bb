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
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase0
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase0(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
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
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase2
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase2(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
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
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase4
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase4(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
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
class Binary3RegisterShiftedTestTesterCase5
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase5(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase5
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
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase6
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase6(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20)=~10111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
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
class Binary3RegisterShiftedTestTesterCase7
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase7(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase7
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
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase8
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase10
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase10(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase12
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase12(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary4RegisterShiftedOpTesterCase13
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase13(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase13
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
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase14
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase14(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary4RegisterShiftedOpTesterCase15
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase15(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase15
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
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase16
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase16(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase18
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase18(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary4RegisterShiftedOpTesterCase19
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase19(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase19
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
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase20
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase20(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary4RegisterShiftedOpTesterCase21
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase21(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase21
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
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase22
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase22(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase22
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase22
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary4RegisterShiftedOpTesterCase23
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase23(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase23
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
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase24
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase24(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase24
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase24
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary4RegisterShiftedOpTesterCase25
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase25(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase25
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
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(11:7)=00000 => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, imm5(11:7)=00000 => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTesterCase26
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase26(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase26
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op3(6:5)=~00 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase26
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x00000F80) != 0x00000000 /* imm5(11:7)=00000 => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(11:7)=00000 => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, imm5(11:7)=00000 => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTesterCase27
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase27(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase27
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op3(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase27
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x00000F80) != 0x00000000 /* imm5(11:7)=00000 => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterOpTesterCase28
    : public Unary2RegisterOpTester {
 public:
  Unary2RegisterOpTesterCase28(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpTesterCase28
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op3(6:5)=~00 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpTesterCase28
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterOpTesterCase29
    : public Unary2RegisterOpTester {
 public:
  Unary2RegisterOpTesterCase29(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpTesterCase29
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op3(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpTesterCase29
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary3RegisterOpTesterCase30
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase30(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase30
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
class Binary3RegisterOpTesterCase31
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase31(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase31
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
class Binary3RegisterOpTesterCase32
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase32(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase32
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
class Binary3RegisterOpTesterCase33
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase33(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase33
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
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTesterCase34
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase34(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase34
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op3(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase34
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTesterCase35
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase35(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase35
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op3(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase35
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase36
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase36(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase36
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase36
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary4RegisterShiftedOpTesterCase37
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase37(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase37
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
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTesterCase38
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase38(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase38
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase38
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Unary3RegisterShiftedOpTesterCase39
    : public Unary3RegisterShiftedOpTesterRegsNotPc {
 public:
  Unary3RegisterShiftedOpTesterCase39(const NamedClassDecoder& decoder)
    : Unary3RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary3RegisterShiftedOpTesterCase39
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
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Tst_Rule_231_A1_P456'}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Tst_Rule_231_A1_P456}
class Binary2RegisterImmedShiftedTestTester_Case0
    : public Binary2RegisterImmedShiftedTestTesterCase0 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case0()
    : Binary2RegisterImmedShiftedTestTesterCase0(
      state_.Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_)
  {}
};

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
class Binary3RegisterShiftedTestTester_Case1
    : public Binary3RegisterShiftedTestTesterCase1 {
 public:
  Binary3RegisterShiftedTestTester_Case1()
    : Binary3RegisterShiftedTestTesterCase1(
      state_.Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Teq_Rule_228_A1_P450'}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Teq_Rule_228_A1_P450}
class Binary2RegisterImmedShiftedTestTester_Case2
    : public Binary2RegisterImmedShiftedTestTesterCase2 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case2()
    : Binary2RegisterImmedShiftedTestTesterCase2(
      state_.Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_)
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
class Binary3RegisterShiftedTestTester_Case3
    : public Binary3RegisterShiftedTestTesterCase3 {
 public:
  Binary3RegisterShiftedTestTester_Case3()
    : Binary3RegisterShiftedTestTesterCase3(
      state_.Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Cmp_Rule_36_A1_P82'}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Cmp_Rule_36_A1_P82}
class Binary2RegisterImmedShiftedTestTester_Case4
    : public Binary2RegisterImmedShiftedTestTesterCase4 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case4()
    : Binary2RegisterImmedShiftedTestTesterCase4(
      state_.Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_)
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
class Binary3RegisterShiftedTestTester_Case5
    : public Binary3RegisterShiftedTestTesterCase5 {
 public:
  Binary3RegisterShiftedTestTester_Case5()
    : Binary3RegisterShiftedTestTesterCase5(
      state_.Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Cmn_Rule_33_A1_P76'}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Cmn_Rule_33_A1_P76}
class Binary2RegisterImmedShiftedTestTester_Case6
    : public Binary2RegisterImmedShiftedTestTesterCase6 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case6()
    : Binary2RegisterImmedShiftedTestTesterCase6(
      state_.Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_)
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
class Binary3RegisterShiftedTestTester_Case7
    : public Binary3RegisterShiftedTestTesterCase7 {
 public:
  Binary3RegisterShiftedTestTester_Case7()
    : Binary3RegisterShiftedTestTesterCase7(
      state_.Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'And_Rule_7_A1_P36',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: And_Rule_7_A1_P36,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case8
    : public Binary3RegisterImmedShiftedOpTesterCase8 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case8()
    : Binary3RegisterImmedShiftedOpTesterCase8(
      state_.Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_)
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
class Binary4RegisterShiftedOpTester_Case9
    : public Binary4RegisterShiftedOpTesterCase9 {
 public:
  Binary4RegisterShiftedOpTester_Case9()
    : Binary4RegisterShiftedOpTesterCase9(
      state_.Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Eor_Rule_45_A1_P96',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Eor_Rule_45_A1_P96,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case10
    : public Binary3RegisterImmedShiftedOpTesterCase10 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case10()
    : Binary3RegisterImmedShiftedOpTesterCase10(
      state_.Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_)
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
class Binary4RegisterShiftedOpTester_Case11
    : public Binary4RegisterShiftedOpTesterCase11 {
 public:
  Binary4RegisterShiftedOpTester_Case11()
    : Binary4RegisterShiftedOpTesterCase11(
      state_.Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Sub_Rule_213_A1_P422',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Sub_Rule_213_A1_P422,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case12
    : public Binary3RegisterImmedShiftedOpTesterCase12 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case12()
    : Binary3RegisterImmedShiftedOpTesterCase12(
      state_.Binary3RegisterImmedShiftedOp_Sub_Rule_213_A1_P422_instance_)
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
class Binary4RegisterShiftedOpTester_Case13
    : public Binary4RegisterShiftedOpTesterCase13 {
 public:
  Binary4RegisterShiftedOpTester_Case13()
    : Binary4RegisterShiftedOpTesterCase13(
      state_.Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Rsb_Rule_143_P286',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Rsb_Rule_143_P286,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case14
    : public Binary3RegisterImmedShiftedOpTesterCase14 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case14()
    : Binary3RegisterImmedShiftedOpTesterCase14(
      state_.Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_)
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
class Binary4RegisterShiftedOpTester_Case15
    : public Binary4RegisterShiftedOpTesterCase15 {
 public:
  Binary4RegisterShiftedOpTester_Case15()
    : Binary4RegisterShiftedOpTesterCase15(
      state_.Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Add_Rule_6_A1_P24',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Add_Rule_6_A1_P24,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case16
    : public Binary3RegisterImmedShiftedOpTesterCase16 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case16()
    : Binary3RegisterImmedShiftedOpTesterCase16(
      state_.Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_)
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
class Binary4RegisterShiftedOpTester_Case17
    : public Binary4RegisterShiftedOpTesterCase17 {
 public:
  Binary4RegisterShiftedOpTester_Case17()
    : Binary4RegisterShiftedOpTesterCase17(
      state_.Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Adc_Rule_2_A1_P16',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Adc_Rule_2_A1_P16,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case18
    : public Binary3RegisterImmedShiftedOpTesterCase18 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case18()
    : Binary3RegisterImmedShiftedOpTesterCase18(
      state_.Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_)
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
class Binary4RegisterShiftedOpTester_Case19
    : public Binary4RegisterShiftedOpTesterCase19 {
 public:
  Binary4RegisterShiftedOpTester_Case19()
    : Binary4RegisterShiftedOpTesterCase19(
      state_.Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Sbc_Rule_152_A1_P304',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Sbc_Rule_152_A1_P304,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case20
    : public Binary3RegisterImmedShiftedOpTesterCase20 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case20()
    : Binary3RegisterImmedShiftedOpTesterCase20(
      state_.Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_)
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
class Binary4RegisterShiftedOpTester_Case21
    : public Binary4RegisterShiftedOpTesterCase21 {
 public:
  Binary4RegisterShiftedOpTester_Case21()
    : Binary4RegisterShiftedOpTesterCase21(
      state_.Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Rsc_Rule_146_A1_P292',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Rsc_Rule_146_A1_P292,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case22
    : public Binary3RegisterImmedShiftedOpTesterCase22 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case22()
    : Binary3RegisterImmedShiftedOpTesterCase22(
      state_.Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_)
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
class Binary4RegisterShiftedOpTester_Case23
    : public Binary4RegisterShiftedOpTesterCase23 {
 public:
  Binary4RegisterShiftedOpTester_Case23()
    : Binary4RegisterShiftedOpTesterCase23(
      state_.Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Orr_Rule_114_A1_P230',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Orr_Rule_114_A1_P230,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case24
    : public Binary3RegisterImmedShiftedOpTesterCase24 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case24()
    : Binary3RegisterImmedShiftedOpTesterCase24(
      state_.Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_)
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
class Binary4RegisterShiftedOpTester_Case25
    : public Binary4RegisterShiftedOpTesterCase25 {
 public:
  Binary4RegisterShiftedOpTester_Case25()
    : Binary4RegisterShiftedOpTesterCase25(
      state_.Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Lsl_Rule_88_A1_P178',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(11:7)=00000 => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Lsl_Rule_88_A1_P178,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, imm5(11:7)=00000 => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTester_Case26
    : public Unary2RegisterImmedShiftedOpTesterCase26 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case26()
    : Unary2RegisterImmedShiftedOpTesterCase26(
      state_.Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Ror_Rule_139_A1_P278',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(11:7)=00000 => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Ror_Rule_139_A1_P278,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, imm5(11:7)=00000 => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTester_Case27
    : public Unary2RegisterImmedShiftedOpTesterCase27 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case27()
    : Unary2RegisterImmedShiftedOpTesterCase27(
      state_.Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {'constraints': ,
//     'rule': 'Mov_Rule_97_A1_P196',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {constraints: ,
//     rule: Mov_Rule_97_A1_P196,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterOpTester_Case28
    : public Unary2RegisterOpTesterCase28 {
 public:
  Unary2RegisterOpTester_Case28()
    : Unary2RegisterOpTesterCase28(
      state_.Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {'constraints': ,
//     'rule': 'Rrx_Rule_141_A1_P282',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {constraints: ,
//     rule: Rrx_Rule_141_A1_P282,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterOpTester_Case29
    : public Unary2RegisterOpTesterCase29 {
 public:
  Unary2RegisterOpTester_Case29()
    : Unary2RegisterOpTesterCase29(
      state_.Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_)
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
class Binary3RegisterOpTester_Case30
    : public Binary3RegisterOpTesterCase30 {
 public:
  Binary3RegisterOpTester_Case30()
    : Binary3RegisterOpTesterCase30(
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
class Binary3RegisterOpTester_Case31
    : public Binary3RegisterOpTesterCase31 {
 public:
  Binary3RegisterOpTester_Case31()
    : Binary3RegisterOpTesterCase31(
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
class Binary3RegisterOpTester_Case32
    : public Binary3RegisterOpTesterCase32 {
 public:
  Binary3RegisterOpTester_Case32()
    : Binary3RegisterOpTesterCase32(
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
class Binary3RegisterOpTester_Case33
    : public Binary3RegisterOpTesterCase33 {
 public:
  Binary3RegisterOpTester_Case33()
    : Binary3RegisterOpTesterCase33(
      state_.Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Lsr_Rule_90_A1_P182',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Lsr_Rule_90_A1_P182,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTester_Case34
    : public Unary2RegisterImmedShiftedOpTesterCase34 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case34()
    : Unary2RegisterImmedShiftedOpTesterCase34(
      state_.Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Asr_Rule_14_A1_P40',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Asr_Rule_14_A1_P40,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTester_Case35
    : public Unary2RegisterImmedShiftedOpTesterCase35 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case35()
    : Unary2RegisterImmedShiftedOpTesterCase35(
      state_.Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Bic_Rule_20_A1_P52',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Bic_Rule_20_A1_P52,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTester_Case36
    : public Binary3RegisterImmedShiftedOpTesterCase36 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case36()
    : Binary3RegisterImmedShiftedOpTesterCase36(
      state_.Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_)
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
class Binary4RegisterShiftedOpTester_Case37
    : public Binary4RegisterShiftedOpTesterCase37 {
 public:
  Binary4RegisterShiftedOpTester_Case37()
    : Binary4RegisterShiftedOpTesterCase37(
      state_.Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Mvn_Rule_107_A1_P216',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Mvn_Rule_107_A1_P216,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTester_Case38
    : public Unary2RegisterImmedShiftedOpTesterCase38 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case38()
    : Unary2RegisterImmedShiftedOpTesterCase38(
      state_.Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_)
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
class Unary3RegisterShiftedOpTester_Case39
    : public Unary3RegisterShiftedOpTesterCase39 {
 public:
  Unary3RegisterShiftedOpTester_Case39()
    : Unary3RegisterShiftedOpTesterCase39(
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
//    = Binary2RegisterImmedShiftedTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00010001nnnn0000iiiiitt0mmmm',
//     'rule': 'Tst_Rule_231_A1_P456'}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//     rule: Tst_Rule_231_A1_P456}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case0_TestCase0) {
  Binary2RegisterImmedShiftedTestTester_Case0 baseline_tester;
  NamedDontCareInst_Tst_Rule_231_A1_P456 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000iiiiitt0mmmm");
}

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
       Binary3RegisterShiftedTestTester_Case1_TestCase1) {
  Binary3RegisterShiftedTestTester_Case1 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00010011nnnn0000iiiiitt0mmmm',
//     'rule': 'Teq_Rule_228_A1_P450'}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//     rule: Teq_Rule_228_A1_P450}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case2_TestCase2) {
  Binary2RegisterImmedShiftedTestTester_Case2 baseline_tester;
  NamedDontCareInst_Teq_Rule_228_A1_P450 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
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
       Binary3RegisterShiftedTestTester_Case3_TestCase3) {
  Binary3RegisterShiftedTestTester_Case3 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00010101nnnn0000iiiiitt0mmmm',
//     'rule': 'Cmp_Rule_36_A1_P82'}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: cccc00010101nnnn0000iiiiitt0mmmm,
//     rule: Cmp_Rule_36_A1_P82}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case4_TestCase4) {
  Binary2RegisterImmedShiftedTestTester_Case4 baseline_tester;
  NamedDontCareInst_Cmp_Rule_36_A1_P82 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
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
       Binary3RegisterShiftedTestTester_Case5_TestCase5) {
  Binary3RegisterShiftedTestTester_Case5 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00010111nnnn0000iiiiitt0mmmm',
//     'rule': 'Cmn_Rule_33_A1_P76'}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: cccc00010111nnnn0000iiiiitt0mmmm,
//     rule: Cmn_Rule_33_A1_P76}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case6_TestCase6) {
  Binary2RegisterImmedShiftedTestTester_Case6 baseline_tester;
  NamedDontCareInst_Cmn_Rule_33_A1_P76 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
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
       Binary3RegisterShiftedTestTester_Case7_TestCase7) {
  Binary3RegisterShiftedTestTester_Case7 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000000unnnnddddiiiiitt0mmmm',
//     'rule': 'And_Rule_7_A1_P36',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000000unnnnddddiiiiitt0mmmm,
//     rule: And_Rule_7_A1_P36,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case8_TestCase8) {
  Binary3RegisterImmedShiftedOpTester_Case8 baseline_tester;
  NamedDefs12To15_And_Rule_7_A1_P36 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case9_TestCase9) {
  Binary4RegisterShiftedOpTester_Case9 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000001unnnnddddiiiiitt0mmmm',
//     'rule': 'Eor_Rule_45_A1_P96',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000001unnnnddddiiiiitt0mmmm,
//     rule: Eor_Rule_45_A1_P96,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case10_TestCase10) {
  Binary3RegisterImmedShiftedOpTester_Case10 baseline_tester;
  NamedDefs12To15_Eor_Rule_45_A1_P96 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case11_TestCase11) {
  Binary4RegisterShiftedOpTester_Case11 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000010unnnnddddiiiiitt0mmmm',
//     'rule': 'Sub_Rule_213_A1_P422',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000010unnnnddddiiiiitt0mmmm,
//     rule: Sub_Rule_213_A1_P422,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case12_TestCase12) {
  Binary3RegisterImmedShiftedOpTester_Case12 baseline_tester;
  NamedDefs12To15_Sub_Rule_213_A1_P422 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case13_TestCase13) {
  Binary4RegisterShiftedOpTester_Case13 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000011unnnnddddiiiiitt0mmmm',
//     'rule': 'Rsb_Rule_143_P286',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000011unnnnddddiiiiitt0mmmm,
//     rule: Rsb_Rule_143_P286,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case14_TestCase14) {
  Binary3RegisterImmedShiftedOpTester_Case14 baseline_tester;
  NamedDefs12To15_Rsb_Rule_143_P286 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case15_TestCase15) {
  Binary4RegisterShiftedOpTester_Case15 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000100unnnnddddiiiiitt0mmmm',
//     'rule': 'Add_Rule_6_A1_P24',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000100unnnnddddiiiiitt0mmmm,
//     rule: Add_Rule_6_A1_P24,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case16_TestCase16) {
  Binary3RegisterImmedShiftedOpTester_Case16 baseline_tester;
  NamedDefs12To15_Add_Rule_6_A1_P24 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case17_TestCase17) {
  Binary4RegisterShiftedOpTester_Case17 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000101unnnnddddiiiiitt0mmmm',
//     'rule': 'Adc_Rule_2_A1_P16',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000101unnnnddddiiiiitt0mmmm,
//     rule: Adc_Rule_2_A1_P16,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case18_TestCase18) {
  Binary3RegisterImmedShiftedOpTester_Case18 baseline_tester;
  NamedDefs12To15_Adc_Rule_2_A1_P16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case19_TestCase19) {
  Binary4RegisterShiftedOpTester_Case19 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000110unnnnddddiiiiitt0mmmm',
//     'rule': 'Sbc_Rule_152_A1_P304',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000110unnnnddddiiiiitt0mmmm,
//     rule: Sbc_Rule_152_A1_P304,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case20_TestCase20) {
  Binary3RegisterImmedShiftedOpTester_Case20 baseline_tester;
  NamedDefs12To15_Sbc_Rule_152_A1_P304 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case21_TestCase21) {
  Binary4RegisterShiftedOpTester_Case21 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000111unnnnddddiiiiitt0mmmm',
//     'rule': 'Rsc_Rule_146_A1_P292',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000111unnnnddddiiiiitt0mmmm,
//     rule: Rsc_Rule_146_A1_P292,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case22_TestCase22) {
  Binary3RegisterImmedShiftedOpTester_Case22 baseline_tester;
  NamedDefs12To15_Rsc_Rule_146_A1_P292 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case23_TestCase23) {
  Binary4RegisterShiftedOpTester_Case23 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001100unnnnddddiiiiitt0mmmm',
//     'rule': 'Orr_Rule_114_A1_P230',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001100unnnnddddiiiiitt0mmmm,
//     rule: Orr_Rule_114_A1_P230,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case24_TestCase24) {
  Binary3RegisterImmedShiftedOpTester_Case24 baseline_tester;
  NamedDefs12To15_Orr_Rule_114_A1_P230 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case25_TestCase25) {
  Binary4RegisterShiftedOpTester_Case25 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000ddddiiiii000mmmm',
//     'rule': 'Lsl_Rule_88_A1_P178',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(11:7)=00000 => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000ddddiiiii000mmmm,
//     rule: Lsl_Rule_88_A1_P178,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, imm5(11:7)=00000 => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case26_TestCase26) {
  Unary2RegisterImmedShiftedOpTester_Case26 baseline_tester;
  NamedDefs12To15_Lsl_Rule_88_A1_P178 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii000mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000ddddiiiii110mmmm',
//     'rule': 'Ror_Rule_139_A1_P278',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(11:7)=00000 => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000ddddiiiii110mmmm,
//     rule: Ror_Rule_139_A1_P278,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, imm5(11:7)=00000 => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case27_TestCase27) {
  Unary2RegisterImmedShiftedOpTester_Case27 baseline_tester;
  NamedDefs12To15_Ror_Rule_139_A1_P278 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii110mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000dddd00000000mmmm',
//     'rule': 'Mov_Rule_97_A1_P196',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000dddd00000000mmmm,
//     rule: Mov_Rule_97_A1_P196,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Case28_TestCase28) {
  Unary2RegisterOpTester_Case28 baseline_tester;
  NamedDefs12To15_Mov_Rule_97_A1_P196 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000000mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000dddd00000110mmmm',
//     'rule': 'Rrx_Rule_141_A1_P282',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000dddd00000110mmmm,
//     rule: Rrx_Rule_141_A1_P282,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Case29_TestCase29) {
  Unary2RegisterOpTester_Case29 baseline_tester;
  NamedDefs12To15_Rrx_Rule_141_A1_P282 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000110mmmm");
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
       Binary3RegisterOpTester_Case30_TestCase30) {
  Binary3RegisterOpTester_Case30 baseline_tester;
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
       Binary3RegisterOpTester_Case31_TestCase31) {
  Binary3RegisterOpTester_Case31 baseline_tester;
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
       Binary3RegisterOpTester_Case32_TestCase32) {
  Binary3RegisterOpTester_Case32 baseline_tester;
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
       Binary3RegisterOpTester_Case33_TestCase33) {
  Binary3RegisterOpTester_Case33 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0111nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000ddddiiiii010mmmm',
//     'rule': 'Lsr_Rule_90_A1_P182',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000ddddiiiii010mmmm,
//     rule: Lsr_Rule_90_A1_P182,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case34_TestCase34) {
  Unary2RegisterImmedShiftedOpTester_Case34 baseline_tester;
  NamedDefs12To15_Lsr_Rule_90_A1_P182 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii010mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000ddddiiiii100mmmm',
//     'rule': 'Asr_Rule_14_A1_P40',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000ddddiiiii100mmmm,
//     rule: Asr_Rule_14_A1_P40,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case35_TestCase35) {
  Unary2RegisterImmedShiftedOpTester_Case35 baseline_tester;
  NamedDefs12To15_Asr_Rule_14_A1_P40 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii100mmmm");
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001110unnnnddddiiiiitt0mmmm',
//     'rule': 'Bic_Rule_20_A1_P52',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001110unnnnddddiiiiitt0mmmm,
//     rule: Bic_Rule_20_A1_P52,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case36_TestCase36) {
  Binary3RegisterImmedShiftedOpTester_Case36 baseline_tester;
  NamedDefs12To15_Bic_Rule_20_A1_P52 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110unnnnddddiiiiitt0mmmm");
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
       Binary4RegisterShiftedOpTester_Case37_TestCase37) {
  Binary4RegisterShiftedOpTester_Case37 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001111u0000ddddiiiiitt0mmmm',
//     'rule': 'Mvn_Rule_107_A1_P216',
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001111u0000ddddiiiiitt0mmmm,
//     rule: Mvn_Rule_107_A1_P216,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case38_TestCase38) {
  Unary2RegisterImmedShiftedOpTester_Case38 baseline_tester;
  NamedDefs12To15_Mvn_Rule_107_A1_P216 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111u0000ddddiiiiitt0mmmm");
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
       Unary3RegisterShiftedOpTester_Case39_TestCase39) {
  Unary3RegisterShiftedOpTester_Case39 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111s0000ddddssss0tt1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
