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
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32}}
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)]}
class Binary2RegisterImmedShiftedTestTesterCase0
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase0(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10001
  if ((inst.Bits() & 0x01F00000)  !=
          0x01100000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmedShiftedTestTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmedShiftedTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32}}
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)]}
class Binary2RegisterImmedShiftedTestTesterCase1
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase1(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10011
  if ((inst.Bits() & 0x01F00000)  !=
          0x01300000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmedShiftedTestTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmedShiftedTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32}}
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)]}
class Binary2RegisterImmedShiftedTestTesterCase2
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase2(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10101
  if ((inst.Bits() & 0x01F00000)  !=
          0x01500000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmedShiftedTestTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmedShiftedTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32}}
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)]}
class Binary2RegisterImmedShiftedTestTesterCase3
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase3(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10111
  if ((inst.Bits() & 0x01F00000)  !=
          0x01700000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmedShiftedTestTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmedShiftedTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0000x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase4
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0000x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0001x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase5
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase5(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0001x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0010x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase6
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase6(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0010x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00400000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0011x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase7
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase7(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0011x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00600000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0100x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase8
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00800000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0101x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase9
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase9(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00A00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0110x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase10
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase10(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0110x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00C00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0111x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase11
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0111x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00E00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1100x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase12
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase12(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01800000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOpImmNotZero',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(11:7)=00000 => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7)],
//       imm5: imm5(11:7),
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpTesterCase13
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase13(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=00000
  if ((inst.Bits() & 0x00000F80)  ==
          0x00000000) return false;
  // op3(6:5)=~00
  if ((inst.Bits() & 0x00000060)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: imm5(11:7)=00000 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00000F80)  !=
          0x00000000);

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOpImmNotZero',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(11:7)=00000 => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7)],
//       imm5: imm5(11:7),
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpTesterCase14
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase14(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=00000
  if ((inst.Bits() & 0x00000F80)  ==
          0x00000000) return false;
  // op3(6:5)=~11
  if ((inst.Bits() & 0x00000060)  !=
          0x00000060) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: imm5(11:7)=00000 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00000F80)  !=
          0x00000000);

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
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
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=~00000
  if ((inst.Bits() & 0x00000F80)  !=
          0x00000000) return false;
  // op3(6:5)=~00
  if ((inst.Bits() & 0x00000060)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
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
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=~00000
  if ((inst.Bits() & 0x00000F80)  !=
          0x00000000) return false;
  // op3(6:5)=~11
  if ((inst.Bits() & 0x00000060)  !=
          0x00000060) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpTesterCase17
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase17(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op3(6:5)=~01
  if ((inst.Bits() & 0x00000060)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpTesterCase18
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase18(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op3(6:5)=~10
  if ((inst.Bits() & 0x00000060)  !=
          0x00000040) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1110x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTesterCase19
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase19(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1110x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01C00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpTesterCase20
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase20(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1111x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01E00000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32},
//       rule: 'TST_register'}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)],
//       rule: TST_register}
class Binary2RegisterImmedShiftedTestTester_Case0
    : public Binary2RegisterImmedShiftedTestTesterCase0 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case0()
    : Binary2RegisterImmedShiftedTestTesterCase0(
      state_.Binary2RegisterImmedShiftedTest_TST_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32},
//       rule: 'TEQ_register'}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)],
//       rule: TEQ_register}
class Binary2RegisterImmedShiftedTestTester_Case1
    : public Binary2RegisterImmedShiftedTestTesterCase1 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case1()
    : Binary2RegisterImmedShiftedTestTesterCase1(
      state_.Binary2RegisterImmedShiftedTest_TEQ_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32},
//       rule: 'CMP_register'}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)],
//       rule: CMP_register}
class Binary2RegisterImmedShiftedTestTester_Case2
    : public Binary2RegisterImmedShiftedTestTesterCase2 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case2()
    : Binary2RegisterImmedShiftedTestTesterCase2(
      state_.Binary2RegisterImmedShiftedTest_CMP_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32},
//       rule: 'CMN_register'}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)],
//       rule: CMN_register}
class Binary2RegisterImmedShiftedTestTester_Case3
    : public Binary2RegisterImmedShiftedTestTesterCase3 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case3()
    : Binary2RegisterImmedShiftedTestTesterCase3(
      state_.Binary2RegisterImmedShiftedTest_CMN_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'AND_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: AND_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case4
    : public Binary3RegisterShiftedOpTesterCase4 {
 public:
  Binary3RegisterShiftedOpTester_Case4()
    : Binary3RegisterShiftedOpTesterCase4(
      state_.Binary3RegisterShiftedOp_AND_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'EOR_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: EOR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case5
    : public Binary3RegisterShiftedOpTesterCase5 {
 public:
  Binary3RegisterShiftedOpTester_Case5()
    : Binary3RegisterShiftedOpTesterCase5(
      state_.Binary3RegisterShiftedOp_EOR_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'SUB_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: SUB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case6
    : public Binary3RegisterShiftedOpTesterCase6 {
 public:
  Binary3RegisterShiftedOpTester_Case6()
    : Binary3RegisterShiftedOpTesterCase6(
      state_.Binary3RegisterShiftedOp_SUB_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'RSB_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: RSB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case7
    : public Binary3RegisterShiftedOpTesterCase7 {
 public:
  Binary3RegisterShiftedOpTester_Case7()
    : Binary3RegisterShiftedOpTesterCase7(
      state_.Binary3RegisterShiftedOp_RSB_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'ADD_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: ADD_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case8
    : public Binary3RegisterShiftedOpTesterCase8 {
 public:
  Binary3RegisterShiftedOpTester_Case8()
    : Binary3RegisterShiftedOpTesterCase8(
      state_.Binary3RegisterShiftedOp_ADD_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'ADC_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: ADC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case9
    : public Binary3RegisterShiftedOpTesterCase9 {
 public:
  Binary3RegisterShiftedOpTester_Case9()
    : Binary3RegisterShiftedOpTesterCase9(
      state_.Binary3RegisterShiftedOp_ADC_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'SBC_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: SBC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case10
    : public Binary3RegisterShiftedOpTesterCase10 {
 public:
  Binary3RegisterShiftedOpTester_Case10()
    : Binary3RegisterShiftedOpTesterCase10(
      state_.Binary3RegisterShiftedOp_SBC_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'RSC_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: RSC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case11
    : public Binary3RegisterShiftedOpTesterCase11 {
 public:
  Binary3RegisterShiftedOpTester_Case11()
    : Binary3RegisterShiftedOpTesterCase11(
      state_.Binary3RegisterShiftedOp_RSC_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'ORR_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: ORR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case12
    : public Binary3RegisterShiftedOpTesterCase12 {
 public:
  Binary3RegisterShiftedOpTester_Case12()
    : Binary3RegisterShiftedOpTesterCase12(
      state_.Binary3RegisterShiftedOp_ORR_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOpImmNotZero',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'LSL_immediate',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(11:7)=00000 => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7)],
//       imm5: imm5(11:7),
//       rule: LSL_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpImmNotZeroTester_Case13
    : public Unary2RegisterShiftedOpTesterCase13 {
 public:
  Unary2RegisterShiftedOpImmNotZeroTester_Case13()
    : Unary2RegisterShiftedOpTesterCase13(
      state_.Unary2RegisterShiftedOpImmNotZero_LSL_immediate_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOpImmNotZero',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'ROR_immediate',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(11:7)=00000 => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7)],
//       imm5: imm5(11:7),
//       rule: ROR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpImmNotZeroTester_Case14
    : public Unary2RegisterShiftedOpTesterCase14 {
 public:
  Unary2RegisterShiftedOpImmNotZeroTester_Case14()
    : Unary2RegisterShiftedOpTesterCase14(
      state_.Unary2RegisterShiftedOpImmNotZero_ROR_immediate_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'MOV_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: MOV_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterOpTester_Case15
    : public Unary2RegisterOpTesterCase15 {
 public:
  Unary2RegisterOpTester_Case15()
    : Unary2RegisterOpTesterCase15(
      state_.Unary2RegisterOp_MOV_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'RRX',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: RRX,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterOpTester_Case16
    : public Unary2RegisterOpTesterCase16 {
 public:
  Unary2RegisterOpTester_Case16()
    : Unary2RegisterOpTesterCase16(
      state_.Unary2RegisterOp_RRX_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'LSR_immediate',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: LSR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpTester_Case17
    : public Unary2RegisterShiftedOpTesterCase17 {
 public:
  Unary2RegisterShiftedOpTester_Case17()
    : Unary2RegisterShiftedOpTesterCase17(
      state_.Unary2RegisterShiftedOp_LSR_immediate_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'ASR_immediate',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: ASR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpTester_Case18
    : public Unary2RegisterShiftedOpTesterCase18 {
 public:
  Unary2RegisterShiftedOpTester_Case18()
    : Unary2RegisterShiftedOpTesterCase18(
      state_.Unary2RegisterShiftedOp_ASR_immediate_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = {baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'BIC_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: BIC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Binary3RegisterShiftedOpTester_Case19
    : public Binary3RegisterShiftedOpTesterCase19 {
 public:
  Binary3RegisterShiftedOpTester_Case19()
    : Binary3RegisterShiftedOpTesterCase19(
      state_.Binary3RegisterShiftedOp_BIC_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'MVN_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: MVN_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary2RegisterShiftedOpTester_Case20
    : public Unary2RegisterShiftedOpTesterCase20 {
 public:
  Unary2RegisterShiftedOpTester_Case20()
    : Unary2RegisterShiftedOpTesterCase20(
      state_.Unary2RegisterShiftedOp_MVN_register_instance_)
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
//    = {actual: 'Binary2RegisterImmedShiftedTest',
//       baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32},
//       pattern: 'cccc00010001nnnn0000iiiiitt0mmmm',
//       rule: 'TST_register'}
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       actual: Binary2RegisterImmedShiftedTest,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)],
//       pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//       rule: TST_register}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case0_TestCase0) {
  Binary2RegisterImmedShiftedTestTester_Case0 tester;
  tester.Test("cccc00010001nnnn0000iiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'Binary2RegisterImmedShiftedTest',
//       baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32},
//       pattern: 'cccc00010011nnnn0000iiiiitt0mmmm',
//       rule: 'TEQ_register'}
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       actual: Binary2RegisterImmedShiftedTest,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)],
//       pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//       rule: TEQ_register}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case1_TestCase1) {
  Binary2RegisterImmedShiftedTestTester_Case1 tester;
  tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'Binary2RegisterImmedShiftedTest',
//       baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32},
//       pattern: 'cccc00010101nnnn0000iiiiitt0mmmm',
//       rule: 'CMP_register'}
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       actual: Binary2RegisterImmedShiftedTest,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)],
//       pattern: cccc00010101nnnn0000iiiiitt0mmmm,
//       rule: CMP_register}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case2_TestCase2) {
  Binary2RegisterImmedShiftedTestTester_Case2 tester;
  tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'Binary2RegisterImmedShiftedTest',
//       baseline: 'Binary2RegisterImmedShiftedTest',
//       constraints: ,
//       defs: {16
//            if inst(20)
//            else 32},
//       pattern: 'cccc00010111nnnn0000iiiiitt0mmmm',
//       rule: 'CMN_register'}
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       S: S(20),
//       actual: Binary2RegisterImmedShiftedTest,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20)],
//       pattern: cccc00010111nnnn0000iiiiitt0mmmm,
//       rule: CMN_register}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case3_TestCase3) {
  Binary2RegisterImmedShiftedTestTester_Case3 tester;
  tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0000x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0000000snnnnddddiiiiitt0mmmm',
//       rule: 'AND_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0000000snnnnddddiiiiitt0mmmm,
//       rule: AND_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case4_TestCase4) {
  Binary3RegisterShiftedOpTester_Case4 tester;
  tester.Test("cccc0000000snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0001x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0000001snnnnddddiiiiitt0mmmm',
//       rule: 'EOR_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0000001snnnnddddiiiiitt0mmmm,
//       rule: EOR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case5_TestCase5) {
  Binary3RegisterShiftedOpTester_Case5 tester;
  tester.Test("cccc0000001snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0010x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0000010snnnnddddiiiiitt0mmmm',
//       rule: 'SUB_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0000010snnnnddddiiiiitt0mmmm,
//       rule: SUB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case6_TestCase6) {
  Binary3RegisterShiftedOpTester_Case6 tester;
  tester.Test("cccc0000010snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0011x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0000011snnnnddddiiiiitt0mmmm',
//       rule: 'RSB_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0000011snnnnddddiiiiitt0mmmm,
//       rule: RSB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case7_TestCase7) {
  Binary3RegisterShiftedOpTester_Case7 tester;
  tester.Test("cccc0000011snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0100x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0000100snnnnddddiiiiitt0mmmm',
//       rule: 'ADD_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0000100snnnnddddiiiiitt0mmmm,
//       rule: ADD_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case8_TestCase8) {
  Binary3RegisterShiftedOpTester_Case8 tester;
  tester.Test("cccc0000100snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0101x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0000101snnnnddddiiiiitt0mmmm',
//       rule: 'ADC_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0000101snnnnddddiiiiitt0mmmm,
//       rule: ADC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case9_TestCase9) {
  Binary3RegisterShiftedOpTester_Case9 tester;
  tester.Test("cccc0000101snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0110x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0000110snnnnddddiiiiitt0mmmm',
//       rule: 'SBC_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0000110snnnnddddiiiiitt0mmmm,
//       rule: SBC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case10_TestCase10) {
  Binary3RegisterShiftedOpTester_Case10 tester;
  tester.Test("cccc0000110snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0111x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0000111snnnnddddiiiiitt0mmmm',
//       rule: 'RSC_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0000111snnnnddddiiiiitt0mmmm,
//       rule: RSC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case11_TestCase11) {
  Binary3RegisterShiftedOpTester_Case11 tester;
  tester.Test("cccc0000111snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=1100x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001100snnnnddddiiiiitt0mmmm',
//       rule: 'ORR_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0001100snnnnddddiiiiitt0mmmm,
//       rule: ORR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case12_TestCase12) {
  Binary3RegisterShiftedOpTester_Case12 tester;
  tester.Test("cccc0001100snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary2RegisterShiftedOpImmNotZero',
//       baseline: 'Unary2RegisterShiftedOpImmNotZero',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001101s0000ddddiiiii000mmmm',
//       rule: 'LSL_immediate',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(11:7)=00000 => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary2RegisterShiftedOpImmNotZero,
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7)],
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii000mmmm,
//       rule: LSL_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpImmNotZeroTester_Case13_TestCase13) {
  Unary2RegisterShiftedOpImmNotZeroTester_Case13 tester;
  tester.Test("cccc0001101s0000ddddiiiii000mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary2RegisterShiftedOpImmNotZero',
//       baseline: 'Unary2RegisterShiftedOpImmNotZero',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001101s0000ddddiiiii110mmmm',
//       rule: 'ROR_immediate',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(11:7)=00000 => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary2RegisterShiftedOpImmNotZero,
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7)],
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii110mmmm,
//       rule: ROR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpImmNotZeroTester_Case14_TestCase14) {
  Unary2RegisterShiftedOpImmNotZeroTester_Case14 tester;
  tester.Test("cccc0001101s0000ddddiiiii110mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary2RegisterOp',
//       baseline: 'Unary2RegisterOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001101s0000dddd00000000mmmm',
//       rule: 'MOV_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary2RegisterOp,
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0001101s0000dddd00000000mmmm,
//       rule: MOV_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Case15_TestCase15) {
  Unary2RegisterOpTester_Case15 tester;
  tester.Test("cccc0001101s0000dddd00000000mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary2RegisterOp',
//       baseline: 'Unary2RegisterOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001101s0000dddd00000110mmmm',
//       rule: 'RRX',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary2RegisterOp,
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0001101s0000dddd00000110mmmm,
//       rule: RRX,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Case16_TestCase16) {
  Unary2RegisterOpTester_Case16 tester;
  tester.Test("cccc0001101s0000dddd00000110mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary2RegisterShiftedOp',
//       baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001101s0000ddddiiiii010mmmm',
//       rule: 'LSR_immediate',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary2RegisterShiftedOp,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0001101s0000ddddiiiii010mmmm,
//       rule: LSR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpTester_Case17_TestCase17) {
  Unary2RegisterShiftedOpTester_Case17 tester;
  tester.Test("cccc0001101s0000ddddiiiii010mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary2RegisterShiftedOp',
//       baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001101s0000ddddiiiii100mmmm',
//       rule: 'ASR_immediate',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary2RegisterShiftedOp,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0001101s0000ddddiiiii100mmmm,
//       rule: ASR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpTester_Case18_TestCase18) {
  Unary2RegisterShiftedOpTester_Case18 tester;
  tester.Test("cccc0001101s0000ddddiiiii100mmmm");
}

// Neutral case:
// inst(24:20)=1110x
//    = {actual: 'Binary3RegisterShiftedOp',
//       baseline: 'Binary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001110snnnnddddiiiiitt0mmmm',
//       rule: 'BIC_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary3RegisterShiftedOp,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0001110snnnnddddiiiiitt0mmmm,
//       rule: BIC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case19_TestCase19) {
  Binary3RegisterShiftedOpTester_Case19 tester;
  tester.Test("cccc0001110snnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary2RegisterShiftedOp',
//       baseline: 'Unary2RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc0001111s0000ddddiiiiitt0mmmm',
//       rule: 'MVN_register',
//       safety: [(inst(15:12)=1111 &&
//            inst(20)=1) => DECODER_ERROR,
//         inst(15:12)=1111 => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary2RegisterShiftedOp,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0001111s0000ddddiiiiitt0mmmm,
//       rule: MVN_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpTester_Case20_TestCase20) {
  Unary2RegisterShiftedOpTester_Case20 tester;
  tester.Test("cccc0001111s0000ddddiiiiitt0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
