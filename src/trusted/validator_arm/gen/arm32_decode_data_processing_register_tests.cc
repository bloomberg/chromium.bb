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
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase1
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase1(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase1
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
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
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
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase3
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase3(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase3
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
// inst(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase4
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary3RegisterImmedShiftedOpTesterCase5
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase5(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary3RegisterImmedShiftedOpTesterCase6
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase6(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Binary3RegisterImmedShiftedOpTesterCase7
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase7(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

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
// inst(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase9
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase9(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

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
// inst(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterImmedShiftedOpTesterCase11
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

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
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ['(inst(15:12)=1111 && inst(20)=1) => UNDEFINED', 'inst(11:7)=00000 => UNDEFINED', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: [(Rd(15:12)=1111 && S(20)=1) => UNDEFINED, imm5(11:7)=00000 => UNDEFINED, Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterImmedShiftedOpTesterCase13
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase13(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase13
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

bool Unary2RegisterImmedShiftedOpTesterCase13
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
class Unary2RegisterImmedShiftedOpTesterCase14
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase14(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase14
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

bool Unary2RegisterImmedShiftedOpTesterCase14
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
class Unary2RegisterOpTesterCase15
    : public Unary2RegisterOpTester {
 public:
  Unary2RegisterOpTesterCase15(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpTesterCase15
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

bool Unary2RegisterOpTesterCase15
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
class Unary2RegisterOpTesterCase16
    : public Unary2RegisterOpTester {
 public:
  Unary2RegisterOpTesterCase16(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpTesterCase16
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

bool Unary2RegisterOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Unary2RegisterImmedShiftedOpTesterCase17
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase17(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase17
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

bool Unary2RegisterImmedShiftedOpTesterCase17
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
class Unary2RegisterImmedShiftedOpTesterCase18
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase18(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase18
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

bool Unary2RegisterImmedShiftedOpTesterCase18
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
class Binary3RegisterImmedShiftedOpTesterCase19
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  Binary3RegisterImmedShiftedOpTesterCase19(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterImmedShiftedOpTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
class Unary2RegisterImmedShiftedOpTesterCase20
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase20(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase20
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

bool Unary2RegisterImmedShiftedOpTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))) /* (Rd(15:12)=1111 && S(20)=1) => UNDEFINED */);
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => FORBIDDEN_OPERANDS */);
  return true;
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
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Teq_Rule_228_A1_P450'}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Teq_Rule_228_A1_P450}
class Binary2RegisterImmedShiftedTestTester_Case1
    : public Binary2RegisterImmedShiftedTestTesterCase1 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case1()
    : Binary2RegisterImmedShiftedTestTesterCase1(
      state_.Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_)
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
class Binary2RegisterImmedShiftedTestTester_Case2
    : public Binary2RegisterImmedShiftedTestTesterCase2 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case2()
    : Binary2RegisterImmedShiftedTestTesterCase2(
      state_.Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_)
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
class Binary2RegisterImmedShiftedTestTester_Case3
    : public Binary2RegisterImmedShiftedTestTesterCase3 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case3()
    : Binary2RegisterImmedShiftedTestTesterCase3(
      state_.Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case4
    : public Binary3RegisterImmedShiftedOpTesterCase4 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case4()
    : Binary3RegisterImmedShiftedOpTesterCase4(
      state_.Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case5
    : public Binary3RegisterImmedShiftedOpTesterCase5 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case5()
    : Binary3RegisterImmedShiftedOpTesterCase5(
      state_.Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case6
    : public Binary3RegisterImmedShiftedOpTesterCase6 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case6()
    : Binary3RegisterImmedShiftedOpTesterCase6(
      state_.Binary3RegisterImmedShiftedOp_Sub_Rule_213_A1_P422_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case7
    : public Binary3RegisterImmedShiftedOpTesterCase7 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case7()
    : Binary3RegisterImmedShiftedOpTesterCase7(
      state_.Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case8
    : public Binary3RegisterImmedShiftedOpTesterCase8 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case8()
    : Binary3RegisterImmedShiftedOpTesterCase8(
      state_.Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case9
    : public Binary3RegisterImmedShiftedOpTesterCase9 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case9()
    : Binary3RegisterImmedShiftedOpTesterCase9(
      state_.Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case10
    : public Binary3RegisterImmedShiftedOpTesterCase10 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case10()
    : Binary3RegisterImmedShiftedOpTesterCase10(
      state_.Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case11
    : public Binary3RegisterImmedShiftedOpTesterCase11 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case11()
    : Binary3RegisterImmedShiftedOpTesterCase11(
      state_.Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_)
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
class Binary3RegisterImmedShiftedOpTester_Case12
    : public Binary3RegisterImmedShiftedOpTesterCase12 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case12()
    : Binary3RegisterImmedShiftedOpTesterCase12(
      state_.Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_)
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
class Unary2RegisterImmedShiftedOpTester_Case13
    : public Unary2RegisterImmedShiftedOpTesterCase13 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case13()
    : Unary2RegisterImmedShiftedOpTesterCase13(
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
class Unary2RegisterImmedShiftedOpTester_Case14
    : public Unary2RegisterImmedShiftedOpTesterCase14 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case14()
    : Unary2RegisterImmedShiftedOpTesterCase14(
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
class Unary2RegisterOpTester_Case15
    : public Unary2RegisterOpTesterCase15 {
 public:
  Unary2RegisterOpTester_Case15()
    : Unary2RegisterOpTesterCase15(
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
class Unary2RegisterOpTester_Case16
    : public Unary2RegisterOpTesterCase16 {
 public:
  Unary2RegisterOpTester_Case16()
    : Unary2RegisterOpTesterCase16(
      state_.Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_)
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
class Unary2RegisterImmedShiftedOpTester_Case17
    : public Unary2RegisterImmedShiftedOpTesterCase17 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case17()
    : Unary2RegisterImmedShiftedOpTesterCase17(
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
class Unary2RegisterImmedShiftedOpTester_Case18
    : public Unary2RegisterImmedShiftedOpTesterCase18 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case18()
    : Unary2RegisterImmedShiftedOpTesterCase18(
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
class Binary3RegisterImmedShiftedOpTester_Case19
    : public Binary3RegisterImmedShiftedOpTesterCase19 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case19()
    : Binary3RegisterImmedShiftedOpTesterCase19(
      state_.Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_)
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
class Unary2RegisterImmedShiftedOpTester_Case20
    : public Unary2RegisterImmedShiftedOpTesterCase20 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case20()
    : Unary2RegisterImmedShiftedOpTesterCase20(
      state_.Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_)
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
       Binary2RegisterImmedShiftedTestTester_Case1_TestCase1) {
  Binary2RegisterImmedShiftedTestTester_Case1 baseline_tester;
  NamedDontCareInst_Teq_Rule_228_A1_P450 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
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
       Binary2RegisterImmedShiftedTestTester_Case2_TestCase2) {
  Binary2RegisterImmedShiftedTestTester_Case2 baseline_tester;
  NamedDontCareInst_Cmp_Rule_36_A1_P82 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
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
       Binary2RegisterImmedShiftedTestTester_Case3_TestCase3) {
  Binary2RegisterImmedShiftedTestTester_Case3 baseline_tester;
  NamedDontCareInst_Cmn_Rule_33_A1_P76 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case4_TestCase4) {
  Binary3RegisterImmedShiftedOpTester_Case4 baseline_tester;
  NamedDefs12To15_And_Rule_7_A1_P36 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000unnnnddddiiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case5_TestCase5) {
  Binary3RegisterImmedShiftedOpTester_Case5 baseline_tester;
  NamedDefs12To15_Eor_Rule_45_A1_P96 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001unnnnddddiiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case6_TestCase6) {
  Binary3RegisterImmedShiftedOpTester_Case6 baseline_tester;
  NamedDefs12To15_Sub_Rule_213_A1_P422 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010unnnnddddiiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case7_TestCase7) {
  Binary3RegisterImmedShiftedOpTester_Case7 baseline_tester;
  NamedDefs12To15_Rsb_Rule_143_P286 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011unnnnddddiiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case8_TestCase8) {
  Binary3RegisterImmedShiftedOpTester_Case8 baseline_tester;
  NamedDefs12To15_Add_Rule_6_A1_P24 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100unnnnddddiiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case9_TestCase9) {
  Binary3RegisterImmedShiftedOpTester_Case9 baseline_tester;
  NamedDefs12To15_Adc_Rule_2_A1_P16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101unnnnddddiiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case10_TestCase10) {
  Binary3RegisterImmedShiftedOpTester_Case10 baseline_tester;
  NamedDefs12To15_Sbc_Rule_152_A1_P304 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110unnnnddddiiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case11_TestCase11) {
  Binary3RegisterImmedShiftedOpTester_Case11 baseline_tester;
  NamedDefs12To15_Rsc_Rule_146_A1_P292 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111unnnnddddiiiiitt0mmmm");
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
       Binary3RegisterImmedShiftedOpTester_Case12_TestCase12) {
  Binary3RegisterImmedShiftedOpTester_Case12 baseline_tester;
  NamedDefs12To15_Orr_Rule_114_A1_P230 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100unnnnddddiiiiitt0mmmm");
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
       Unary2RegisterImmedShiftedOpTester_Case13_TestCase13) {
  Unary2RegisterImmedShiftedOpTester_Case13 baseline_tester;
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
       Unary2RegisterImmedShiftedOpTester_Case14_TestCase14) {
  Unary2RegisterImmedShiftedOpTester_Case14 baseline_tester;
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
       Unary2RegisterOpTester_Case15_TestCase15) {
  Unary2RegisterOpTester_Case15 baseline_tester;
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
       Unary2RegisterOpTester_Case16_TestCase16) {
  Unary2RegisterOpTester_Case16 baseline_tester;
  NamedDefs12To15_Rrx_Rule_141_A1_P282 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000110mmmm");
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
       Unary2RegisterImmedShiftedOpTester_Case17_TestCase17) {
  Unary2RegisterImmedShiftedOpTester_Case17 baseline_tester;
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
       Unary2RegisterImmedShiftedOpTester_Case18_TestCase18) {
  Unary2RegisterImmedShiftedOpTester_Case18 baseline_tester;
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
       Binary3RegisterImmedShiftedOpTester_Case19_TestCase19) {
  Binary3RegisterImmedShiftedOpTester_Case19 baseline_tester;
  NamedDefs12To15_Bic_Rule_20_A1_P52 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110unnnnddddiiiiitt0mmmm");
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
       Unary2RegisterImmedShiftedOpTester_Case20_TestCase20) {
  Unary2RegisterImmedShiftedOpTester_Case20 baseline_tester;
  NamedDefs12To15_Mvn_Rule_107_A1_P216 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111u0000ddddiiiiitt0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
