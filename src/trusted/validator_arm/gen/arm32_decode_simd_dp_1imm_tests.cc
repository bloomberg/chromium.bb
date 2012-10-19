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
// inst(5)=0 & inst(11:8)=10x0
//    = {baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase0
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase0(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5)=~0 */) return false;
  if ((inst.Bits() & 0x00000D00) != 0x00000800 /* cmode(11:8)=~10x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((inst.Bits() & 0x00000020) == 0x00000000) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000001) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) != 0x0000000C)) /* op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000020) == 0x00000020) && ((inst.Bits() & 0x00000F00) != 0x00000E00)) /* op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=0 & inst(11:8)=10x1
//    = {baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase1
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase1(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5)=~0 */) return false;
  if ((inst.Bits() & 0x00000D00) != 0x00000900 /* cmode(11:8)=~10x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000000) || ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) == 0x0000000C)) /* cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=0 & inst(11:8)=0xx0
//    = {baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase2
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase2(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5)=~0 */) return false;
  if ((inst.Bits() & 0x00000900) != 0x00000000 /* cmode(11:8)=~0xx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((inst.Bits() & 0x00000020) == 0x00000000) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000001) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) != 0x0000000C)) /* op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000020) == 0x00000020) && ((inst.Bits() & 0x00000F00) != 0x00000E00)) /* op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=0 & inst(11:8)=0xx1
//    = {baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase3
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase3(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5)=~0 */) return false;
  if ((inst.Bits() & 0x00000900) != 0x00000100 /* cmode(11:8)=~0xx1 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000000) || ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) == 0x0000000C)) /* cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=0 & inst(11:8)=11xx
//    = {baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=11xx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase4
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase4(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5)=~0 */) return false;
  if ((inst.Bits() & 0x00000C00) != 0x00000C00 /* cmode(11:8)=~11xx */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((inst.Bits() & 0x00000020) == 0x00000000) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000001) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) != 0x0000000C)) /* op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000020) == 0x00000020) && ((inst.Bits() & 0x00000F00) != 0x00000E00)) /* op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=1 & inst(11:8)=1110
//    = {baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=1110
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase5
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase5(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000E00 /* cmode(11:8)=~1110 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((inst.Bits() & 0x00000020) == 0x00000000) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000001) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) != 0x0000000C)) /* op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000020) == 0x00000020) && ((inst.Bits() & 0x00000F00) != 0x00000E00)) /* op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=1 & inst(11:8)=10x0
//    = {baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase6
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase6(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;
  if ((inst.Bits() & 0x00000D00) != 0x00000800 /* cmode(11:8)=~10x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000001) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) != 0x0000000C))) || ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000E) == 0x0000000E)) /* (cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=1 & inst(11:8)=10x1
//    = {baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase7
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase7(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;
  if ((inst.Bits() & 0x00000D00) != 0x00000900 /* cmode(11:8)=~10x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000000) || ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) == 0x0000000C)) /* cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=1 & inst(11:8)=110x
//    = {baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase8
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase8(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;
  if ((inst.Bits() & 0x00000E00) != 0x00000C00 /* cmode(11:8)=~110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000001) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) != 0x0000000C))) || ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000E) == 0x0000000E)) /* (cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=1 & inst(11:8)=0xx0
//    = {baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase9
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase9(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;
  if ((inst.Bits() & 0x00000900) != 0x00000000 /* cmode(11:8)=~0xx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000001) && ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) != 0x0000000C))) || ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000E) == 0x0000000E)) /* (cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(5)=1 & inst(11:8)=0xx1
//    = {baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediateTesterCase10
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediateTesterCase10(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediateTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;
  if ((inst.Bits() & 0x00000900) != 0x00000100 /* cmode(11:8)=~0xx1 */) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector1RegisterImmediateTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector1RegisterImmediateTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001) == 0x00000000) || ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C) == 0x0000000C)) /* cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040) == 0x00000040) && ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) == 0x00000001)) /* Q(6)=1 && Vd(0)=1 => UNDEFINED */);
  return Vector1RegisterImmediateTester::
    ApplySanityChecks(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(5)=0 & inst(11:8)=10x0
//    = {baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       rule: 'VMOV_immediate_A1',
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=0 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_MOVTester_Case0
    : public Vector1RegisterImmediateTesterCase0 {
 public:
  Vector1RegisterImmediate_MOVTester_Case0()
    : Vector1RegisterImmediateTesterCase0(
      state_.Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(5)=0 & inst(11:8)=10x1
//    = {baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       rule: 'VORR_immediate',
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=0 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_BITTester_Case1
    : public Vector1RegisterImmediateTesterCase1 {
 public:
  Vector1RegisterImmediate_BITTester_Case1()
    : Vector1RegisterImmediateTesterCase1(
      state_.Vector1RegisterImmediate_BIT_VORR_immediate_instance_)
  {}
};

// Neutral case:
// inst(5)=0 & inst(11:8)=0xx0
//    = {baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       rule: 'VMOV_immediate_A1',
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=0 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_MOVTester_Case2
    : public Vector1RegisterImmediateTesterCase2 {
 public:
  Vector1RegisterImmediate_MOVTester_Case2()
    : Vector1RegisterImmediateTesterCase2(
      state_.Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(5)=0 & inst(11:8)=0xx1
//    = {baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       rule: 'VORR_immediate',
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=0 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_BITTester_Case3
    : public Vector1RegisterImmediateTesterCase3 {
 public:
  Vector1RegisterImmediate_BITTester_Case3()
    : Vector1RegisterImmediateTesterCase3(
      state_.Vector1RegisterImmediate_BIT_VORR_immediate_instance_)
  {}
};

// Neutral case:
// inst(5)=0 & inst(11:8)=11xx
//    = {baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       rule: 'VMOV_immediate_A1',
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=0 & cmode(11:8)=11xx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_MOVTester_Case4
    : public Vector1RegisterImmediateTesterCase4 {
 public:
  Vector1RegisterImmediate_MOVTester_Case4()
    : Vector1RegisterImmediateTesterCase4(
      state_.Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(5)=1 & inst(11:8)=1110
//    = {baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       rule: 'VMOV_immediate_A1',
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=1 & cmode(11:8)=1110
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_MOVTester_Case5
    : public Vector1RegisterImmediateTesterCase5 {
 public:
  Vector1RegisterImmediate_MOVTester_Case5()
    : Vector1RegisterImmediateTesterCase5(
      state_.Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(5)=1 & inst(11:8)=10x0
//    = {baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       rule: 'VMVN_immediate',
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=1 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_MVNTester_Case6
    : public Vector1RegisterImmediateTesterCase6 {
 public:
  Vector1RegisterImmediate_MVNTester_Case6()
    : Vector1RegisterImmediateTesterCase6(
      state_.Vector1RegisterImmediate_MVN_VMVN_immediate_instance_)
  {}
};

// Neutral case:
// inst(5)=1 & inst(11:8)=10x1
//    = {baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       rule: 'VBIC_immediate',
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=1 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_BITTester_Case7
    : public Vector1RegisterImmediateTesterCase7 {
 public:
  Vector1RegisterImmediate_BITTester_Case7()
    : Vector1RegisterImmediateTesterCase7(
      state_.Vector1RegisterImmediate_BIT_VBIC_immediate_instance_)
  {}
};

// Neutral case:
// inst(5)=1 & inst(11:8)=110x
//    = {baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       rule: 'VMVN_immediate',
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=1 & cmode(11:8)=110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_MVNTester_Case8
    : public Vector1RegisterImmediateTesterCase8 {
 public:
  Vector1RegisterImmediate_MVNTester_Case8()
    : Vector1RegisterImmediateTesterCase8(
      state_.Vector1RegisterImmediate_MVN_VMVN_immediate_instance_)
  {}
};

// Neutral case:
// inst(5)=1 & inst(11:8)=0xx0
//    = {baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       rule: 'VMVN_immediate',
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=1 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_MVNTester_Case9
    : public Vector1RegisterImmediateTesterCase9 {
 public:
  Vector1RegisterImmediate_MVNTester_Case9()
    : Vector1RegisterImmediateTesterCase9(
      state_.Vector1RegisterImmediate_MVN_VMVN_immediate_instance_)
  {}
};

// Neutral case:
// inst(5)=1 & inst(11:8)=0xx1
//    = {baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       rule: 'VBIC_immediate',
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representative case:
// op(5)=1 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
class Vector1RegisterImmediate_BITTester_Case10
    : public Vector1RegisterImmediateTesterCase10 {
 public:
  Vector1RegisterImmediate_BITTester_Case10()
    : Vector1RegisterImmediateTesterCase10(
      state_.Vector1RegisterImmediate_BIT_VBIC_immediate_instance_)
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
// inst(5)=0 & inst(11:8)=10x0
//    = {actual: 'Vector1RegisterImmediate_MOV',
//       baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       pattern: '1111001m1d000mmmddddcccc0qp1mmmm',
//       rule: 'VMOV_immediate_A1',
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_MOV,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MOVTester_Case0_TestCase0) {
  Vector1RegisterImmediate_MOVTester_Case0 tester;
  tester.Test("1111001m1d000mmmddddcccc0qp1mmmm");
}

// Neutral case:
// inst(5)=0 & inst(11:8)=10x1
//    = {actual: 'Vector1RegisterImmediate_BIT',
//       baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       pattern: '1111001i1d000mmmddddcccc0q01mmmm',
//       rule: 'VORR_immediate',
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_BIT,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_BITTester_Case1_TestCase1) {
  Vector1RegisterImmediate_BITTester_Case1 tester;
  tester.Test("1111001i1d000mmmddddcccc0q01mmmm");
}

// Neutral case:
// inst(5)=0 & inst(11:8)=0xx0
//    = {actual: 'Vector1RegisterImmediate_MOV',
//       baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       pattern: '1111001m1d000mmmddddcccc0qp1mmmm',
//       rule: 'VMOV_immediate_A1',
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_MOV,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MOVTester_Case2_TestCase2) {
  Vector1RegisterImmediate_MOVTester_Case2 tester;
  tester.Test("1111001m1d000mmmddddcccc0qp1mmmm");
}

// Neutral case:
// inst(5)=0 & inst(11:8)=0xx1
//    = {actual: 'Vector1RegisterImmediate_BIT',
//       baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       pattern: '1111001i1d000mmmddddcccc0q01mmmm',
//       rule: 'VORR_immediate',
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_BIT,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_BITTester_Case3_TestCase3) {
  Vector1RegisterImmediate_BITTester_Case3 tester;
  tester.Test("1111001i1d000mmmddddcccc0q01mmmm");
}

// Neutral case:
// inst(5)=0 & inst(11:8)=11xx
//    = {actual: 'Vector1RegisterImmediate_MOV',
//       baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       pattern: '1111001m1d000mmmddddcccc0qp1mmmm',
//       rule: 'VMOV_immediate_A1',
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=0 & cmode(11:8)=11xx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_MOV,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MOVTester_Case4_TestCase4) {
  Vector1RegisterImmediate_MOVTester_Case4 tester;
  tester.Test("1111001m1d000mmmddddcccc0qp1mmmm");
}

// Neutral case:
// inst(5)=1 & inst(11:8)=1110
//    = {actual: 'Vector1RegisterImmediate_MOV',
//       baseline: 'Vector1RegisterImmediate_MOV',
//       constraints: ,
//       pattern: '1111001m1d000mmmddddcccc0qp1mmmm',
//       rule: 'VMOV_immediate_A1',
//       safety: ['inst(5)=0 && inst(11:8)(0)=1 && inst(11:8)(3:2)=~11 => DECODER_ERROR', 'inst(5)=1 && inst(11:8)=~1110 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=1110
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_MOV,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 && cmode(0)=1 && cmode(3:2)=~11 => DECODER_ERROR, op(5)=1 && cmode(11:8)=~1110 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MOVTester_Case5_TestCase5) {
  Vector1RegisterImmediate_MOVTester_Case5 tester;
  tester.Test("1111001m1d000mmmddddcccc0qp1mmmm");
}

// Neutral case:
// inst(5)=1 & inst(11:8)=10x0
//    = {actual: 'Vector1RegisterImmediate_MVN',
//       baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       pattern: '1111001i1d000mmmddddcccc0q11mmmm',
//       rule: 'VMVN_immediate',
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_MVN,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MVNTester_Case6_TestCase6) {
  Vector1RegisterImmediate_MVNTester_Case6 tester;
  tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

// Neutral case:
// inst(5)=1 & inst(11:8)=10x1
//    = {actual: 'Vector1RegisterImmediate_BIT',
//       baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       pattern: '1111001i1d000mmmddddcccc0q11mmmm',
//       rule: 'VBIC_immediate',
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_BIT,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_BITTester_Case7_TestCase7) {
  Vector1RegisterImmediate_BITTester_Case7 tester;
  tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

// Neutral case:
// inst(5)=1 & inst(11:8)=110x
//    = {actual: 'Vector1RegisterImmediate_MVN',
//       baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       pattern: '1111001i1d000mmmddddcccc0q11mmmm',
//       rule: 'VMVN_immediate',
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_MVN,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MVNTester_Case8_TestCase8) {
  Vector1RegisterImmediate_MVNTester_Case8 tester;
  tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

// Neutral case:
// inst(5)=1 & inst(11:8)=0xx0
//    = {actual: 'Vector1RegisterImmediate_MVN',
//       baseline: 'Vector1RegisterImmediate_MVN',
//       constraints: ,
//       pattern: '1111001i1d000mmmddddcccc0q11mmmm',
//       rule: 'VMVN_immediate',
//       safety: ['(inst(11:8)(0)=1 && inst(11:8)(3:2)=~11) || inst(11:8)(3:1)=111 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_MVN,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 && cmode(3:2)=~11) || cmode(3:1)=111 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MVNTester_Case9_TestCase9) {
  Vector1RegisterImmediate_MVNTester_Case9 tester;
  tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

// Neutral case:
// inst(5)=1 & inst(11:8)=0xx1
//    = {actual: 'Vector1RegisterImmediate_BIT',
//       baseline: 'Vector1RegisterImmediate_BIT',
//       constraints: ,
//       pattern: '1111001i1d000mmmddddcccc0q11mmmm',
//       rule: 'VBIC_immediate',
//       safety: ['inst(11:8)(0)=0 || inst(11:8)(3:2)=11 => DECODER_ERROR', 'inst(6)=1 && inst(15:12)(0)=1 => UNDEFINED']}
//
// Representaive case:
// op(5)=1 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Vector1RegisterImmediate_BIT,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 || cmode(3:2)=11 => DECODER_ERROR, Q(6)=1 && Vd(0)=1 => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_BITTester_Case10_TestCase10) {
  Vector1RegisterImmediate_BITTester_Case10 tester;
  tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
