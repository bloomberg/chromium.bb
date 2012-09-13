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
// inst(24:20)=00100 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp {'constraints': }
//
// Representaive case:
// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: }
class Unary1RegisterImmediateOpTesterCase0
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase0(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00400000 /* op(24:20)=~00100 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=00101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(24:20)=00101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x00500000 /* op(24:20)=~00101 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01000 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp {'constraints': }
//
// Representaive case:
// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: }
class Unary1RegisterImmediateOpTesterCase2
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase2(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00800000 /* op(24:20)=~01000 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01001 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(24:20)=01001 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x00900000 /* op(24:20)=~01001 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest {'constraints': }
//
// Representaive case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest {constraints: }
class BinaryRegisterImmediateTestTesterCase4
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase4(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': }
//
// Representaive case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: }
class BinaryRegisterImmediateTestTesterCase5
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase5(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': }
//
// Representaive case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: }
class BinaryRegisterImmediateTestTesterCase6
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase6(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': }
//
// Representaive case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: }
class BinaryRegisterImmediateTestTesterCase7
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase7(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op(24:20)=~10111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0000x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase8
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0001x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase9
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase9(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representaive case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTesterCase10
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTesterCase10(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op(24:20)=~0010x */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0011x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase11
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representaive case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTesterCase12
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTesterCase12(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op(24:20)=~0100x */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0101x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase13
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase13(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0110x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase14
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase14(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0111x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase15
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase15(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=1100x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase16
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase16(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTesterCase17
    : public Unary1RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTesterCase17(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase18
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase18(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTesterCase19
    : public Unary1RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTesterCase19(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:20)=00100 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Adr_Rule_10_A2_P32'}
//
// Representative case:
// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Adr_Rule_10_A2_P32}
class Unary1RegisterImmediateOpTester_Case0
    : public Unary1RegisterImmediateOpTesterCase0 {
 public:
  Unary1RegisterImmediateOpTester_Case0()
    : Unary1RegisterImmediateOpTesterCase0(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32_instance_)
  {}
};

// Neutral case:
// inst(24:20)=00101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Subs_Pc_Lr_and_related_instructions_Rule_A1a'}
//
// Representative case:
// op(24:20)=00101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Subs_Pc_Lr_and_related_instructions_Rule_A1a}
class ForbiddenCondDecoderTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenCondDecoderTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.ForbiddenCondDecoder_Subs_Pc_Lr_and_related_instructions_Rule_A1a_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01000 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Adr_Rule_10_A1_P32'}
//
// Representative case:
// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Adr_Rule_10_A1_P32}
class Unary1RegisterImmediateOpTester_Case2
    : public Unary1RegisterImmediateOpTesterCase2 {
 public:
  Unary1RegisterImmediateOpTester_Case2()
    : Unary1RegisterImmediateOpTesterCase2(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01001 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Subs_Pc_Lr_and_related_instructions_Rule_A1b'}
//
// Representative case:
// op(24:20)=01001 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Subs_Pc_Lr_and_related_instructions_Rule_A1b}
class ForbiddenCondDecoderTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenCondDecoderTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.ForbiddenCondDecoder_Subs_Pc_Lr_and_related_instructions_Rule_A1b_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest {'constraints': ,
//     'rule': 'Tst_Rule_230_A1_P454'}
//
// Representative case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest {constraints: ,
//     rule: Tst_Rule_230_A1_P454}
class MaskedBinaryRegisterImmediateTestTester_Case4
    : public BinaryRegisterImmediateTestTesterCase4 {
 public:
  MaskedBinaryRegisterImmediateTestTester_Case4()
    : BinaryRegisterImmediateTestTesterCase4(
      state_.MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': ,
//     'rule': 'Teq_Rule_227_A1_P448'}
//
// Representative case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: Teq_Rule_227_A1_P448}
class BinaryRegisterImmediateTestTester_Case5
    : public BinaryRegisterImmediateTestTesterCase5 {
 public:
  BinaryRegisterImmediateTestTester_Case5()
    : BinaryRegisterImmediateTestTesterCase5(
      state_.BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': ,
//     'rule': 'Cmp_Rule_35_A1_P80'}
//
// Representative case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: Cmp_Rule_35_A1_P80}
class BinaryRegisterImmediateTestTester_Case6
    : public BinaryRegisterImmediateTestTesterCase6 {
 public:
  BinaryRegisterImmediateTestTester_Case6()
    : BinaryRegisterImmediateTestTesterCase6(
      state_.BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': ,
//     'rule': 'Cmn_Rule_32_A1_P74'}
//
// Representative case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: Cmn_Rule_32_A1_P74}
class BinaryRegisterImmediateTestTester_Case7
    : public BinaryRegisterImmediateTestTesterCase7 {
 public:
  BinaryRegisterImmediateTestTester_Case7()
    : BinaryRegisterImmediateTestTesterCase7(
      state_.BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'And_Rule_11_A1_P34',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0000x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: And_Rule_11_A1_P34,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case8
    : public Binary2RegisterImmediateOpTesterCase8 {
 public:
  Binary2RegisterImmediateOpTester_Case8()
    : Binary2RegisterImmediateOpTesterCase8(
      state_.Binary2RegisterImmediateOp_And_Rule_11_A1_P34_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Eor_Rule_44_A1_P94',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0001x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Eor_Rule_44_A1_P94,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case9
    : public Binary2RegisterImmediateOpTesterCase9 {
 public:
  Binary2RegisterImmediateOpTester_Case9()
    : Binary2RegisterImmediateOpTesterCase9(
      state_.Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Sub_Rule_212_A1_P420',
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representative case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Sub_Rule_212_A1_P420,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTester_Case10
    : public Binary2RegisterImmediateOpTesterCase10 {
 public:
  Binary2RegisterImmediateOpTester_Case10()
    : Binary2RegisterImmediateOpTesterCase10(
      state_.Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Rsb_Rule_142_A1_P284',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0011x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Rsb_Rule_142_A1_P284,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case11
    : public Binary2RegisterImmediateOpTesterCase11 {
 public:
  Binary2RegisterImmediateOpTester_Case11()
    : Binary2RegisterImmediateOpTesterCase11(
      state_.Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Add_Rule_5_A1_P22',
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representative case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Add_Rule_5_A1_P22,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTester_Case12
    : public Binary2RegisterImmediateOpTesterCase12 {
 public:
  Binary2RegisterImmediateOpTester_Case12()
    : Binary2RegisterImmediateOpTesterCase12(
      state_.Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Adc_Rule_6_A1_P14',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0101x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Adc_Rule_6_A1_P14,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case13
    : public Binary2RegisterImmediateOpTesterCase13 {
 public:
  Binary2RegisterImmediateOpTester_Case13()
    : Binary2RegisterImmediateOpTesterCase13(
      state_.Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Sbc_Rule_151_A1_P302',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0110x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Sbc_Rule_151_A1_P302,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case14
    : public Binary2RegisterImmediateOpTesterCase14 {
 public:
  Binary2RegisterImmediateOpTester_Case14()
    : Binary2RegisterImmediateOpTesterCase14(
      state_.Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Rsc_Rule_145_A1_P290',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0111x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Rsc_Rule_145_A1_P290,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case15
    : public Binary2RegisterImmediateOpTesterCase15 {
 public:
  Binary2RegisterImmediateOpTester_Case15()
    : Binary2RegisterImmediateOpTesterCase15(
      state_.Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Orr_Rule_113_A1_P228',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1100x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Orr_Rule_113_A1_P228,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case16
    : public Binary2RegisterImmediateOpTesterCase16 {
 public:
  Binary2RegisterImmediateOpTester_Case16()
    : Binary2RegisterImmediateOpTesterCase16(
      state_.Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Mov_Rule_96_A1_P194',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Mov_Rule_96_A1_P194,
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTester_Case17
    : public Unary1RegisterImmediateOpTesterCase17 {
 public:
  Unary1RegisterImmediateOpTester_Case17()
    : Unary1RegisterImmediateOpTesterCase17(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Bic_Rule_19_A1_P50',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {constraints: ,
//     rule: Bic_Rule_19_A1_P50,
//     safety: ['NotRdIsPcAndS']}
class MaskedBinary2RegisterImmediateOpTester_Case18
    : public Binary2RegisterImmediateOpTesterCase18 {
 public:
  MaskedBinary2RegisterImmediateOpTester_Case18()
    : Binary2RegisterImmediateOpTesterCase18(
      state_.MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Mvn_Rule_106_A1_P214',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Mvn_Rule_106_A1_P214,
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTester_Case19
    : public Unary1RegisterImmediateOpTesterCase19 {
 public:
  Unary1RegisterImmediateOpTester_Case19()
    : Unary1RegisterImmediateOpTesterCase19(
      state_.Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214_instance_)
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
// inst(24:20)=00100 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc001001001111ddddiiiiiiiiiiii',
//     'rule': 'Adr_Rule_10_A2_P32'}
//
// Representative case:
// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc001001001111ddddiiiiiiiiiiii,
//     rule: Adr_Rule_10_A2_P32}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case0_TestCase0) {
  Unary1RegisterImmediateOpTester_Case0 baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A2_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001001001111ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=00101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00100101nnnn1111iiiiiiiiiiii',
//     'rule': 'Subs_Pc_Lr_and_related_instructions_Rule_A1a'}
//
// Representative case:
// op(24:20)=00101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00100101nnnn1111iiiiiiiiiiii,
//     rule: Subs_Pc_Lr_and_related_instructions_Rule_A1a}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case1_TestCase1) {
  ForbiddenCondDecoderTester_Case1 baseline_tester;
  NamedForbidden_Subs_Pc_Lr_and_related_instructions_Rule_A1a actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00100101nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=01000 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc001010001111ddddiiiiiiiiiiii',
//     'rule': 'Adr_Rule_10_A1_P32'}
//
// Representative case:
// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc001010001111ddddiiiiiiiiiiii,
//     rule: Adr_Rule_10_A1_P32}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case2_TestCase2) {
  Unary1RegisterImmediateOpTester_Case2 baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A1_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001010001111ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=01001 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00101001nnnn1111iiiiiiiiiiii',
//     'rule': 'Subs_Pc_Lr_and_related_instructions_Rule_A1b'}
//
// Representative case:
// op(24:20)=01001 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00101001nnnn1111iiiiiiiiiiii,
//     rule: Subs_Pc_Lr_and_related_instructions_Rule_A1b}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case3_TestCase3) {
  ForbiddenCondDecoderTester_Case3 baseline_tester;
  NamedForbidden_Subs_Pc_Lr_and_related_instructions_Rule_A1b actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00101001nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest => TestIfAddressMasked {'constraints': ,
//     'pattern': 'cccc00110001nnnn0000iiiiiiiiiiii',
//     'rule': 'Tst_Rule_230_A1_P454'}
//
// Representative case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest => TestIfAddressMasked {constraints: ,
//     pattern: cccc00110001nnnn0000iiiiiiiiiiii,
//     rule: Tst_Rule_230_A1_P454}
TEST_F(Arm32DecoderStateTests,
       MaskedBinaryRegisterImmediateTestTester_Case4_TestCase4) {
  MaskedBinaryRegisterImmediateTestTester_Case4 baseline_tester;
  NamedTestIfAddressMasked_Tst_Rule_230_A1_P454 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110001nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00110011nnnn0000iiiiiiiiiiii',
//     'rule': 'Teq_Rule_227_A1_P448'}
//
// Representative case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: cccc00110011nnnn0000iiiiiiiiiiii,
//     rule: Teq_Rule_227_A1_P448}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case5_TestCase5) {
  BinaryRegisterImmediateTestTester_Case5 baseline_tester;
  NamedDontCareInst_Teq_Rule_227_A1_P448 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110011nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00110101nnnn0000iiiiiiiiiiii',
//     'rule': 'Cmp_Rule_35_A1_P80'}
//
// Representative case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: cccc00110101nnnn0000iiiiiiiiiiii,
//     rule: Cmp_Rule_35_A1_P80}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case6_TestCase6) {
  BinaryRegisterImmediateTestTester_Case6 baseline_tester;
  NamedDontCareInst_Cmp_Rule_35_A1_P80 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110101nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00110111nnnn0000iiiiiiiiiiii',
//     'rule': 'Cmn_Rule_32_A1_P74'}
//
// Representative case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: cccc00110111nnnn0000iiiiiiiiiiii,
//     rule: Cmn_Rule_32_A1_P74}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case7_TestCase7) {
  BinaryRegisterImmediateTestTester_Case7 baseline_tester;
  NamedDontCareInst_Cmn_Rule_32_A1_P74 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110111nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010000snnnnddddiiiiiiiiiiii',
//     'rule': 'And_Rule_11_A1_P34',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0000x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010000snnnnddddiiiiiiiiiiii,
//     rule: And_Rule_11_A1_P34,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case8_TestCase8) {
  Binary2RegisterImmediateOpTester_Case8 baseline_tester;
  NamedDefs12To15_And_Rule_11_A1_P34 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010000snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010001snnnnddddiiiiiiiiiiii',
//     'rule': 'Eor_Rule_44_A1_P94',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0001x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010001snnnnddddiiiiiiiiiiii,
//     rule: Eor_Rule_44_A1_P94,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case9_TestCase9) {
  Binary2RegisterImmediateOpTester_Case9 baseline_tester;
  NamedDefs12To15_Eor_Rule_44_A1_P94 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010001snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010010snnnnddddiiiiiiiiiiii',
//     'rule': 'Sub_Rule_212_A1_P420',
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representative case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010010snnnnddddiiiiiiiiiiii,
//     rule: Sub_Rule_212_A1_P420,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case10_TestCase10) {
  Binary2RegisterImmediateOpTester_Case10 baseline_tester;
  NamedDefs12To15_Sub_Rule_212_A1_P420 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010010snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010011snnnnddddiiiiiiiiiiii',
//     'rule': 'Rsb_Rule_142_A1_P284',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0011x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010011snnnnddddiiiiiiiiiiii,
//     rule: Rsb_Rule_142_A1_P284,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case11_TestCase11) {
  Binary2RegisterImmediateOpTester_Case11 baseline_tester;
  NamedDefs12To15_Rsb_Rule_142_A1_P284 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010011snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010100snnnnddddiiiiiiiiiiii',
//     'rule': 'Add_Rule_5_A1_P22',
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representative case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010100snnnnddddiiiiiiiiiiii,
//     rule: Add_Rule_5_A1_P22,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case12_TestCase12) {
  Binary2RegisterImmediateOpTester_Case12 baseline_tester;
  NamedDefs12To15_Add_Rule_5_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010100snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010101snnnnddddiiiiiiiiiiii',
//     'rule': 'Adc_Rule_6_A1_P14',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0101x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010101snnnnddddiiiiiiiiiiii,
//     rule: Adc_Rule_6_A1_P14,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case13_TestCase13) {
  Binary2RegisterImmediateOpTester_Case13 baseline_tester;
  NamedDefs12To15_Adc_Rule_6_A1_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010101snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010110snnnnddddiiiiiiiiiiii',
//     'rule': 'Sbc_Rule_151_A1_P302',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0110x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010110snnnnddddiiiiiiiiiiii,
//     rule: Sbc_Rule_151_A1_P302,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case14_TestCase14) {
  Binary2RegisterImmediateOpTester_Case14 baseline_tester;
  NamedDefs12To15_Sbc_Rule_151_A1_P302 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010110snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010111snnnnddddiiiiiiiiiiii',
//     'rule': 'Rsc_Rule_145_A1_P290',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0111x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010111snnnnddddiiiiiiiiiiii,
//     rule: Rsc_Rule_145_A1_P290,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case15_TestCase15) {
  Binary2RegisterImmediateOpTester_Case15 baseline_tester;
  NamedDefs12To15_Rsc_Rule_145_A1_P290 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010111snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0011100snnnnddddiiiiiiiiiiii',
//     'rule': 'Orr_Rule_113_A1_P228',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1100x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0011100snnnnddddiiiiiiiiiiii,
//     rule: Orr_Rule_113_A1_P228,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case16_TestCase16) {
  Binary2RegisterImmediateOpTester_Case16 baseline_tester;
  NamedDefs12To15_Orr_Rule_113_A1_P228 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011100snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0011101s0000ddddiiiiiiiiiiii',
//     'rule': 'Mov_Rule_96_A1_P194',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0011101s0000ddddiiiiiiiiiiii,
//     rule: Mov_Rule_96_A1_P194,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case17_TestCase17) {
  Unary1RegisterImmediateOpTester_Case17 baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A1_P194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011101s0000ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp => MaskAddress {'constraints': ,
//     'pattern': 'cccc0011110snnnnddddiiiiiiiiiiii',
//     'rule': 'Bic_Rule_19_A1_P50',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp => MaskAddress {constraints: ,
//     pattern: cccc0011110snnnnddddiiiiiiiiiiii,
//     rule: Bic_Rule_19_A1_P50,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       MaskedBinary2RegisterImmediateOpTester_Case18_TestCase18) {
  MaskedBinary2RegisterImmediateOpTester_Case18 baseline_tester;
  NamedMaskAddress_Bic_Rule_19_A1_P50 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011110snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0011111s0000ddddiiiiiiiiiiii',
//     'rule': 'Mvn_Rule_106_A1_P214',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0011111s0000ddddiiiiiiiiiiii,
//     rule: Mvn_Rule_106_A1_P214,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case19_TestCase19) {
  Unary1RegisterImmediateOpTester_Case19 baseline_tester;
  NamedDefs12To15_Mvn_Rule_106_A1_P214 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011111s0000ddddiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
