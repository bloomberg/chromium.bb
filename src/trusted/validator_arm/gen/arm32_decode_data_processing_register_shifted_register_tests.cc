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
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTesterCase0
    : public Binary3RegisterShiftedTestTester {
 public:
  Binary3RegisterShiftedTestTesterCase0(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Binary3RegisterShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedTestTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rn,Rm,Rs} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTesterCase1
    : public Binary3RegisterShiftedTestTester {
 public:
  Binary3RegisterShiftedTestTesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Binary3RegisterShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedTestTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rn,Rm,Rs} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTesterCase2
    : public Binary3RegisterShiftedTestTester {
 public:
  Binary3RegisterShiftedTestTesterCase2(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Binary3RegisterShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedTestTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rn,Rm,Rs} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTesterCase3
    : public Binary3RegisterShiftedTestTester {
 public:
  Binary3RegisterShiftedTestTesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Binary3RegisterShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedTestTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rn,Rm,Rs} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase4
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase4(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase5
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase5(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase6
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase6(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase7
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase7(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase8
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase9
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase9(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase10
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase10(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase11
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase12
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase12(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpTesterCase13
    : public Binary3RegisterOpTester {
 public:
  Binary3RegisterOpTesterCase13(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Binary3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rd,Rn,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpTesterCase14
    : public Binary3RegisterOpTester {
 public:
  Binary3RegisterOpTesterCase14(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Binary3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rd,Rn,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpTesterCase15
    : public Binary3RegisterOpTester {
 public:
  Binary3RegisterOpTesterCase15(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Binary3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rd,Rn,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpTesterCase16
    : public Binary3RegisterOpTester {
 public:
  Binary3RegisterOpTesterCase16(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Binary3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rd,Rn,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTesterCase17
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase17(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00070000) >> 16)))) || (((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8)))) || (((15) == ((inst.Bits() & 0x00000007))))) /* Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {'constraints': ,
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {constraints: ,
//     safety: [Pc in {Rd,Rm,Rs} => UNPREDICTABLE]}
class Unary3RegisterShiftedOpTesterCase18
    : public Unary3RegisterShiftedOpTester {
 public:
  Unary3RegisterShiftedOpTesterCase18(const NamedClassDecoder& decoder)
    : Unary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  return Unary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary3RegisterShiftedOpTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary3RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x00007000) >> 12)))) || (((15) == ((inst.Bits() & 0x00000007)))) || (((15) == (((inst.Bits() & 0x00000700) >> 8))))) /* Pc in {Rd,Rm,Rs} => UNPREDICTABLE */);
  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'TST_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: TST_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTester_Case0
    : public Binary3RegisterShiftedTestTesterCase0 {
 public:
  Binary3RegisterShiftedTestTester_Case0()
    : Binary3RegisterShiftedTestTesterCase0(
      state_.Binary3RegisterShiftedTest_TST_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'TEQ_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: TEQ_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTester_Case1
    : public Binary3RegisterShiftedTestTesterCase1 {
 public:
  Binary3RegisterShiftedTestTester_Case1()
    : Binary3RegisterShiftedTestTesterCase1(
      state_.Binary3RegisterShiftedTest_TEQ_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'CMP_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: CMP_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTester_Case2
    : public Binary3RegisterShiftedTestTesterCase2 {
 public:
  Binary3RegisterShiftedTestTester_Case2()
    : Binary3RegisterShiftedTestTesterCase2(
      state_.Binary3RegisterShiftedTest_CMP_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'CMN_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: CMN_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTester_Case3
    : public Binary3RegisterShiftedTestTesterCase3 {
 public:
  Binary3RegisterShiftedTestTester_Case3()
    : Binary3RegisterShiftedTestTesterCase3(
      state_.Binary3RegisterShiftedTest_CMN_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'AND_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: AND_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case4
    : public Binary4RegisterShiftedOpTesterCase4 {
 public:
  Binary4RegisterShiftedOpTester_Case4()
    : Binary4RegisterShiftedOpTesterCase4(
      state_.Binary4RegisterShiftedOp_AND_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'EOR_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: EOR_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case5
    : public Binary4RegisterShiftedOpTesterCase5 {
 public:
  Binary4RegisterShiftedOpTester_Case5()
    : Binary4RegisterShiftedOpTesterCase5(
      state_.Binary4RegisterShiftedOp_EOR_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'SUB_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: SUB_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case6
    : public Binary4RegisterShiftedOpTesterCase6 {
 public:
  Binary4RegisterShiftedOpTester_Case6()
    : Binary4RegisterShiftedOpTesterCase6(
      state_.Binary4RegisterShiftedOp_SUB_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'RSB_register_shfited_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: RSB_register_shfited_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case7
    : public Binary4RegisterShiftedOpTesterCase7 {
 public:
  Binary4RegisterShiftedOpTester_Case7()
    : Binary4RegisterShiftedOpTesterCase7(
      state_.Binary4RegisterShiftedOp_RSB_register_shfited_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'ADD_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: ADD_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case8
    : public Binary4RegisterShiftedOpTesterCase8 {
 public:
  Binary4RegisterShiftedOpTester_Case8()
    : Binary4RegisterShiftedOpTesterCase8(
      state_.Binary4RegisterShiftedOp_ADD_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'ADC_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: ADC_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case9
    : public Binary4RegisterShiftedOpTesterCase9 {
 public:
  Binary4RegisterShiftedOpTester_Case9()
    : Binary4RegisterShiftedOpTesterCase9(
      state_.Binary4RegisterShiftedOp_ADC_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'SBC_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: SBC_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case10
    : public Binary4RegisterShiftedOpTesterCase10 {
 public:
  Binary4RegisterShiftedOpTester_Case10()
    : Binary4RegisterShiftedOpTesterCase10(
      state_.Binary4RegisterShiftedOp_SBC_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'RSC_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: RSC_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case11
    : public Binary4RegisterShiftedOpTesterCase11 {
 public:
  Binary4RegisterShiftedOpTester_Case11()
    : Binary4RegisterShiftedOpTesterCase11(
      state_.Binary4RegisterShiftedOp_RSC_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'ORR_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: ORR_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case12
    : public Binary4RegisterShiftedOpTesterCase12 {
 public:
  Binary4RegisterShiftedOpTester_Case12()
    : Binary4RegisterShiftedOpTesterCase12(
      state_.Binary4RegisterShiftedOp_ORR_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'LSL_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: LSL_register_A1,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpTester_Case13
    : public Binary3RegisterOpTesterCase13 {
 public:
  Binary3RegisterOpTester_Case13()
    : Binary3RegisterOpTesterCase13(
      state_.Binary3RegisterOp_LSL_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'LSR_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: LSR_register_A1,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpTester_Case14
    : public Binary3RegisterOpTesterCase14 {
 public:
  Binary3RegisterOpTester_Case14()
    : Binary3RegisterOpTesterCase14(
      state_.Binary3RegisterOp_LSR_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'ASR_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: ASR_register_A1,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpTester_Case15
    : public Binary3RegisterOpTesterCase15 {
 public:
  Binary3RegisterOpTester_Case15()
    : Binary3RegisterOpTesterCase15(
      state_.Binary3RegisterOp_ASR_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'ROR_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: ROR_register_A1,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpTester_Case16
    : public Binary3RegisterOpTesterCase16 {
 public:
  Binary3RegisterOpTester_Case16()
    : Binary3RegisterOpTesterCase16(
      state_.Binary3RegisterOp_ROR_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'BIC_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: BIC_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
class Binary4RegisterShiftedOpTester_Case17
    : public Binary4RegisterShiftedOpTesterCase17 {
 public:
  Binary4RegisterShiftedOpTester_Case17()
    : Binary4RegisterShiftedOpTesterCase17(
      state_.Binary4RegisterShiftedOp_BIC_register_shifted_register_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {'constraints': ,
//     'rule': 'MVN_register_shifted_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {constraints: ,
//     rule: MVN_register_shifted_register_A1,
//     safety: [Pc in {Rd,Rm,Rs} => UNPREDICTABLE]}
class Unary3RegisterShiftedOpTester_Case18
    : public Unary3RegisterShiftedOpTesterCase18 {
 public:
  Unary3RegisterShiftedOpTester_Case18()
    : Unary3RegisterShiftedOpTesterCase18(
      state_.Unary3RegisterShiftedOp_MVN_register_shifted_register_A1_instance_)
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
//     'rule': 'TST_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010001nnnn0000ssss0tt1mmmm,
//     rule: TST_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case0_TestCase0) {
  Binary3RegisterShiftedTestTester_Case0 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_TST_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010011nnnn0000ssss0tt1mmmm',
//     'rule': 'TEQ_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010011nnnn0000ssss0tt1mmmm,
//     rule: TEQ_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case1_TestCase1) {
  Binary3RegisterShiftedTestTester_Case1 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_TEQ_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010101nnnn0000ssss0tt1mmmm',
//     'rule': 'CMP_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010101nnnn0000ssss0tt1mmmm,
//     rule: CMP_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case2_TestCase2) {
  Binary3RegisterShiftedTestTester_Case2 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_CMP_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010111nnnn0000ssss0tt1mmmm',
//     'rule': 'CMN_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010111nnnn0000ssss0tt1mmmm,
//     rule: CMN_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case3_TestCase3) {
  Binary3RegisterShiftedTestTester_Case3 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_CMN_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000000snnnnddddssss0tt1mmmm',
//     'rule': 'AND_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000000snnnnddddssss0tt1mmmm,
//     rule: AND_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case4_TestCase4) {
  Binary4RegisterShiftedOpTester_Case4 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_AND_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000001snnnnddddssss0tt1mmmm',
//     'rule': 'EOR_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000001snnnnddddssss0tt1mmmm,
//     rule: EOR_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case5_TestCase5) {
  Binary4RegisterShiftedOpTester_Case5 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_EOR_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000010snnnnddddssss0tt1mmmm',
//     'rule': 'SUB_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000010snnnnddddssss0tt1mmmm,
//     rule: SUB_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case6_TestCase6) {
  Binary4RegisterShiftedOpTester_Case6 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_SUB_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000011snnnnddddssss0tt1mmmm',
//     'rule': 'RSB_register_shfited_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000011snnnnddddssss0tt1mmmm,
//     rule: RSB_register_shfited_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case7_TestCase7) {
  Binary4RegisterShiftedOpTester_Case7 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_RSB_register_shfited_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000100snnnnddddssss0tt1mmmm',
//     'rule': 'ADD_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000100snnnnddddssss0tt1mmmm,
//     rule: ADD_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case8_TestCase8) {
  Binary4RegisterShiftedOpTester_Case8 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_ADD_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000101snnnnddddssss0tt1mmmm',
//     'rule': 'ADC_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000101snnnnddddssss0tt1mmmm,
//     rule: ADC_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case9_TestCase9) {
  Binary4RegisterShiftedOpTester_Case9 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_ADC_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000110snnnnddddssss0tt1mmmm',
//     'rule': 'SBC_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000110snnnnddddssss0tt1mmmm,
//     rule: SBC_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case10_TestCase10) {
  Binary4RegisterShiftedOpTester_Case10 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_SBC_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000111snnnnddddssss0tt1mmmm',
//     'rule': 'RSC_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000111snnnnddddssss0tt1mmmm,
//     rule: RSC_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case11_TestCase11) {
  Binary4RegisterShiftedOpTester_Case11 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_RSC_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0001100snnnnddddssss0tt1mmmm',
//     'rule': 'ORR_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0001100snnnnddddssss0tt1mmmm,
//     rule: ORR_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case12_TestCase12) {
  Binary4RegisterShiftedOpTester_Case12 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_ORR_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0001nnnn',
//     'rule': 'LSL_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0001nnnn,
//     rule: LSL_register_A1,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case13_TestCase13) {
  Binary3RegisterOpTester_Case13 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_LSL_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0001nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0011nnnn',
//     'rule': 'LSR_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0011nnnn,
//     rule: LSR_register_A1,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case14_TestCase14) {
  Binary3RegisterOpTester_Case14 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_LSR_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0011nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0101nnnn',
//     'rule': 'ASR_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0101nnnn,
//     rule: ASR_register_A1,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case15_TestCase15) {
  Binary3RegisterOpTester_Case15 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_ASR_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0101nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0111nnnn',
//     'rule': 'ROR_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0111nnnn,
//     rule: ROR_register_A1,
//     safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case16_TestCase16) {
  Binary3RegisterOpTester_Case16 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_ROR_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0111nnnn");
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0001110snnnnddddssss0tt1mmmm',
//     'rule': 'BIC_register_shifted_register_A1',
//     'safety': ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(11:8) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0001110snnnnddddssss0tt1mmmm,
//     rule: BIC_register_shifted_register_A1,
//     safety: [Pc in {Rn,Rd,Rs,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case17_TestCase17) {
  Binary4RegisterShiftedOpTester_Case17 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_BIC_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001111s0000ddddssss0tt1mmmm',
//     'rule': 'MVN_register_shifted_register_A1',
//     'safety': ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001111s0000ddddssss0tt1mmmm,
//     rule: MVN_register_shifted_register_A1,
//     safety: [Pc in {Rd,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary3RegisterShiftedOpTester_Case18_TestCase18) {
  Unary3RegisterShiftedOpTester_Case18 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_MVN_register_shifted_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111s0000ddddssss0tt1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
