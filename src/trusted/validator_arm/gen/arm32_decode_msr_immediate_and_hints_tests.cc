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
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000000 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'CondDecoder',
//       constraints: ,
//       defs: {}}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: CondDecoder,
//       constraints: ,
//       defs: {}}
class CondDecoderTesterCase0
    : public CondDecoderTester {
 public:
  CondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // op2(7:0)=~00000000
  if ((inst.Bits() & 0x000000FF)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx
  if ((inst.Bits() & 0x0000FF00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return CondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondDecoderTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'CondDecoder',
//       constraints: ,
//       defs: {}}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: CondDecoder,
//       constraints: ,
//       defs: {}}
class CondDecoderTesterCase1
    : public CondDecoderTester {
 public:
  CondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // op2(7:0)=~00000001
  if ((inst.Bits() & 0x000000FF)  !=
          0x00000001) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx
  if ((inst.Bits() & 0x0000FF00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return CondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondDecoderTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN],
//       true: true}
class UnsafeCondDecoderTesterCase2
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // op2(7:0)=~00000010
  if ((inst.Bits() & 0x000000FF)  !=
          0x00000002) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx
  if ((inst.Bits() & 0x0000FF00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN],
//       true: true}
class UnsafeCondDecoderTesterCase3
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // op2(7:0)=~00000011
  if ((inst.Bits() & 0x000000FF)  !=
          0x00000003) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx
  if ((inst.Bits() & 0x0000FF00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN],
//       true: true}
class UnsafeCondDecoderTesterCase4
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase4(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // op2(7:0)=~00000100
  if ((inst.Bits() & 0x000000FF)  !=
          0x00000004) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx
  if ((inst.Bits() & 0x0000FF00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN],
//       true: true}
class UnsafeCondDecoderTesterCase5
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase5(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // op2(7:0)=~1111xxxx
  if ((inst.Bits() & 0x000000F0)  !=
          0x000000F0) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx
  if ((inst.Bits() & 0x0000FF00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       safety: [inst(19:18)=00 => DECODER_ERROR]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18)],
//       mask: mask(19:18),
//       safety: [mask(19:18)=00 => DECODER_ERROR],
//       write_nzcvq: mask(1)=1}
class MoveImmediate12ToApsrTesterCase6
    : public MoveImmediate12ToApsrTester {
 public:
  MoveImmediate12ToApsrTesterCase6(const NamedClassDecoder& decoder)
    : MoveImmediate12ToApsrTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool MoveImmediate12ToApsrTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~0100
  if ((inst.Bits() & 0x000F0000)  !=
          0x00040000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

bool MoveImmediate12ToApsrTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveImmediate12ToApsrTester::
               ApplySanityChecks(inst, decoder));

  // safety: mask(19:18)=00 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x00000000);

  // defs: {NZCV
  //       if write_nzcvq
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x000C0000) >> 18) & 0x00000002)  ==
          0x00000002
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       safety: [inst(19:18)=00 => DECODER_ERROR]}
//
// Representative case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18)],
//       mask: mask(19:18),
//       safety: [mask(19:18)=00 => DECODER_ERROR],
//       write_nzcvq: mask(1)=1}
class MoveImmediate12ToApsrTesterCase7
    : public MoveImmediate12ToApsrTester {
 public:
  MoveImmediate12ToApsrTesterCase7(const NamedClassDecoder& decoder)
    : MoveImmediate12ToApsrTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool MoveImmediate12ToApsrTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~1x00
  if ((inst.Bits() & 0x000B0000)  !=
          0x00080000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

bool MoveImmediate12ToApsrTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveImmediate12ToApsrTester::
               ApplySanityChecks(inst, decoder));

  // safety: mask(19:18)=00 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x00000000);

  // defs: {NZCV
  //       if write_nzcvq
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x000C0000) >> 18) & 0x00000002)  ==
          0x00000002
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN],
//       true: true}
class UnsafeCondDecoderTesterCase8
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase8(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~xx01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN],
//       true: true}
class UnsafeCondDecoderTesterCase9
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~0
  if ((inst.Bits() & 0x00400000)  !=
          0x00000000) return false;
  // op1(19:16)=~xx1x
  if ((inst.Bits() & 0x00020000)  !=
          0x00020000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       safety: [true => FORBIDDEN],
//       true: true}
class UnsafeCondDecoderTesterCase10
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(22)=~1
  if ((inst.Bits() & 0x00400000)  !=
          0x00400000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000000 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'CondDecoder',
//       constraints: ,
//       defs: {},
//       rule: 'NOP'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: CondDecoder,
//       constraints: ,
//       defs: {},
//       rule: NOP}
class CondDecoderTester_Case0
    : public CondDecoderTesterCase0 {
 public:
  CondDecoderTester_Case0()
    : CondDecoderTesterCase0(
      state_.CondDecoder_NOP_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'CondDecoder',
//       constraints: ,
//       defs: {},
//       rule: 'YIELD'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: CondDecoder,
//       constraints: ,
//       defs: {},
//       rule: YIELD}
class CondDecoderTester_Case1
    : public CondDecoderTesterCase1 {
 public:
  CondDecoderTester_Case1()
    : CondDecoderTesterCase1(
      state_.CondDecoder_YIELD_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       rule: 'WFE',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       rule: WFE,
//       safety: [true => FORBIDDEN],
//       true: true}
class ForbiddenTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.Forbidden_WFE_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       rule: 'WFI',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       rule: WFI,
//       safety: [true => FORBIDDEN],
//       true: true}
class ForbiddenTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.Forbidden_WFI_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       rule: 'SEV',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       rule: SEV,
//       safety: [true => FORBIDDEN],
//       true: true}
class ForbiddenTester_Case4
    : public UnsafeCondDecoderTesterCase4 {
 public:
  ForbiddenTester_Case4()
    : UnsafeCondDecoderTesterCase4(
      state_.Forbidden_SEV_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       rule: 'DBG',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       rule: DBG,
//       safety: [true => FORBIDDEN],
//       true: true}
class ForbiddenTester_Case5
    : public UnsafeCondDecoderTesterCase5 {
 public:
  ForbiddenTester_Case5()
    : UnsafeCondDecoderTesterCase5(
      state_.Forbidden_DBG_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       rule: 'MSR_immediate',
//       safety: [inst(19:18)=00 => DECODER_ERROR]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18)],
//       mask: mask(19:18),
//       rule: MSR_immediate,
//       safety: [mask(19:18)=00 => DECODER_ERROR],
//       write_nzcvq: mask(1)=1}
class MoveImmediate12ToApsrTester_Case6
    : public MoveImmediate12ToApsrTesterCase6 {
 public:
  MoveImmediate12ToApsrTester_Case6()
    : MoveImmediate12ToApsrTesterCase6(
      state_.MoveImmediate12ToApsr_MSR_immediate_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       rule: 'MSR_immediate',
//       safety: [inst(19:18)=00 => DECODER_ERROR]}
//
// Representative case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18)],
//       mask: mask(19:18),
//       rule: MSR_immediate,
//       safety: [mask(19:18)=00 => DECODER_ERROR],
//       write_nzcvq: mask(1)=1}
class MoveImmediate12ToApsrTester_Case7
    : public MoveImmediate12ToApsrTesterCase7 {
 public:
  MoveImmediate12ToApsrTester_Case7()
    : MoveImmediate12ToApsrTesterCase7(
      state_.MoveImmediate12ToApsr_MSR_immediate_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       rule: 'MSR_immediate',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       rule: MSR_immediate,
//       safety: [true => FORBIDDEN],
//       true: true}
class ForbiddenTester_Case8
    : public UnsafeCondDecoderTesterCase8 {
 public:
  ForbiddenTester_Case8()
    : UnsafeCondDecoderTesterCase8(
      state_.Forbidden_MSR_immediate_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       rule: 'MSR_immediate',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       rule: MSR_immediate,
//       safety: [true => FORBIDDEN],
//       true: true}
class ForbiddenTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.Forbidden_MSR_immediate_instance_)
  {}
};

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       rule: 'MSR_immediate',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       rule: MSR_immediate,
//       safety: [true => FORBIDDEN],
//       true: true}
class ForbiddenTester_Case10
    : public UnsafeCondDecoderTesterCase10 {
 public:
  ForbiddenTester_Case10()
    : UnsafeCondDecoderTesterCase10(
      state_.Forbidden_MSR_immediate_instance_)
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
//    = {actual: 'CondDecoder',
//       baseline: 'CondDecoder',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc0011001000001111000000000000',
//       rule: 'NOP'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: CondDecoder,
//       baseline: CondDecoder,
//       constraints: ,
//       defs: {},
//       pattern: cccc0011001000001111000000000000,
//       rule: NOP}
TEST_F(Arm32DecoderStateTests,
       CondDecoderTester_Case0_TestCase0) {
  CondDecoderTester_Case0 tester;
  tester.Test("cccc0011001000001111000000000000");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'CondDecoder',
//       baseline: 'CondDecoder',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc0011001000001111000000000001',
//       rule: 'YIELD'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: CondDecoder,
//       baseline: CondDecoder,
//       constraints: ,
//       defs: {},
//       pattern: cccc0011001000001111000000000001,
//       rule: YIELD}
TEST_F(Arm32DecoderStateTests,
       CondDecoderTester_Case1_TestCase1) {
  CondDecoderTester_Case1 tester;
  tester.Test("cccc0011001000001111000000000001");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc0011001000001111000000000010',
//       rule: 'WFE',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       pattern: cccc0011001000001111000000000010,
//       rule: WFE,
//       safety: [true => FORBIDDEN],
//       true: true}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case2_TestCase2) {
  ForbiddenTester_Case2 tester;
  tester.Test("cccc0011001000001111000000000010");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc0011001000001111000000000011',
//       rule: 'WFI',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       pattern: cccc0011001000001111000000000011,
//       rule: WFI,
//       safety: [true => FORBIDDEN],
//       true: true}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case3_TestCase3) {
  ForbiddenTester_Case3 tester;
  tester.Test("cccc0011001000001111000000000011");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc0011001000001111000000000100',
//       rule: 'SEV',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       pattern: cccc0011001000001111000000000100,
//       rule: SEV,
//       safety: [true => FORBIDDEN],
//       true: true}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case4_TestCase4) {
  ForbiddenTester_Case4 tester;
  tester.Test("cccc0011001000001111000000000100");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc001100100000111100001111iiii',
//       rule: 'DBG',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       pattern: cccc001100100000111100001111iiii,
//       rule: DBG,
//       safety: [true => FORBIDDEN],
//       true: true}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case5_TestCase5) {
  ForbiddenTester_Case5 tester;
  tester.Test("cccc001100100000111100001111iiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'MoveImmediate12ToApsr',
//       baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       pattern: 'cccc00110010mm001111iiiiiiiiiiii',
//       rule: 'MSR_immediate',
//       safety: [inst(19:18)=00 => DECODER_ERROR]}
//
// Representative case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       actual: MoveImmediate12ToApsr,
//       baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18)],
//       mask: mask(19:18),
//       pattern: cccc00110010mm001111iiiiiiiiiiii,
//       rule: MSR_immediate,
//       safety: [mask(19:18)=00 => DECODER_ERROR],
//       write_nzcvq: mask(1)=1}
TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_Case6_TestCase6) {
  MoveImmediate12ToApsrTester_Case6 tester;
  tester.Test("cccc00110010mm001111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'MoveImmediate12ToApsr',
//       baseline: 'MoveImmediate12ToApsr',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       pattern: 'cccc00110010mm001111iiiiiiiiiiii',
//       rule: 'MSR_immediate',
//       safety: [inst(19:18)=00 => DECODER_ERROR]}
//
// Representative case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       actual: MoveImmediate12ToApsr,
//       baseline: MoveImmediate12ToApsr,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18)],
//       mask: mask(19:18),
//       pattern: cccc00110010mm001111iiiiiiiiiiii,
//       rule: MSR_immediate,
//       safety: [mask(19:18)=00 => DECODER_ERROR],
//       write_nzcvq: mask(1)=1}
TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_Case7_TestCase7) {
  MoveImmediate12ToApsrTester_Case7 tester;
  tester.Test("cccc00110010mm001111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc00110r10mmmm1111iiiiiiiiiiii',
//       rule: 'MSR_immediate',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       pattern: cccc00110r10mmmm1111iiiiiiiiiiii,
//       rule: MSR_immediate,
//       safety: [true => FORBIDDEN],
//       true: true}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case8_TestCase8) {
  ForbiddenTester_Case8 tester;
  tester.Test("cccc00110r10mmmm1111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc00110r10mmmm1111iiiiiiiiiiii',
//       rule: 'MSR_immediate',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       pattern: cccc00110r10mmmm1111iiiiiiiiiiii,
//       rule: MSR_immediate,
//       safety: [true => FORBIDDEN],
//       true: true}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case9_TestCase9) {
  ForbiddenTester_Case9 tester;
  tester.Test("cccc00110r10mmmm1111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'Forbidden',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc00110r10mmmm1111iiiiiiiiiiii',
//       rule: 'MSR_immediate',
//       safety: [true => FORBIDDEN]}
//
// Representative case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       pattern: cccc00110r10mmmm1111iiiiiiiiiiii,
//       rule: MSR_immediate,
//       safety: [true => FORBIDDEN],
//       true: true}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case10_TestCase10) {
  ForbiddenTester_Case10 tester;
  tester.Test("cccc00110r10mmmm1111iiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
