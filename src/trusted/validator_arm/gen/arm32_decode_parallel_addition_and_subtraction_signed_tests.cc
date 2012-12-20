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
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase0
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase0(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase1
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase2
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase2(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase3
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase4
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase5
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase5(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase6
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase6(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase7
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase7(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase8
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase8(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase9
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase9(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase10
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase10(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase11
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase11(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase12
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase12(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase13
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase13(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase14
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase14(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase15
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase15(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase16
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase16(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase17
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase17(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SADD16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SADD16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case0
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase0 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase0(
      state_.Binary3RegisterOpAltBNoCondUpdates_SADD16_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SASX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SASX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case1
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase1 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase1(
      state_.Binary3RegisterOpAltBNoCondUpdates_SASX_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SSAX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SSAX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase2 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase2(
      state_.Binary3RegisterOpAltBNoCondUpdates_SSAX_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SSSUB16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SSSUB16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case3
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase3 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase3(
      state_.Binary3RegisterOpAltBNoCondUpdates_SSSUB16_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SADD8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SADD8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case4
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase4 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case4()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase4(
      state_.Binary3RegisterOpAltBNoCondUpdates_SADD8_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SSUB8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SSUB8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case5
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase5 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase5(
      state_.Binary3RegisterOpAltBNoCondUpdates_SSUB8_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QADD16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QADD16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case6
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase6 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case6()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase6(
      state_.Binary3RegisterOpAltBNoCondUpdates_QADD16_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QASX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QASX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case7
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase7 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case7()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase7(
      state_.Binary3RegisterOpAltBNoCondUpdates_QASX_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QSAX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QSAX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case8
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase8 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase8(
      state_.Binary3RegisterOpAltBNoCondUpdates_QSAX_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QSUB16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QSUB16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case9
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase9 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case9()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase9(
      state_.Binary3RegisterOpAltBNoCondUpdates_QSUB16_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QADD8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QADD8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case10
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase10 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case10()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase10(
      state_.Binary3RegisterOpAltBNoCondUpdates_QADD8_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QSUB8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QSUB8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case11
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase11 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase11(
      state_.Binary3RegisterOpAltBNoCondUpdates_QSUB8_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SHADD16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SHADD16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case12
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase12 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case12()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase12(
      state_.Binary3RegisterOpAltBNoCondUpdates_SHADD16_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SHASX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SHASX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case13
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase13 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case13()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase13(
      state_.Binary3RegisterOpAltBNoCondUpdates_SHASX_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SHSAX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SHSAX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case14
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase14 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase14(
      state_.Binary3RegisterOpAltBNoCondUpdates_SHSAX_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SHSUB16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SHSUB16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case15
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase15 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case15()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase15(
      state_.Binary3RegisterOpAltBNoCondUpdates_SHSUB16_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SHADD8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SHADD8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case16
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase16 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case16()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase16(
      state_.Binary3RegisterOpAltBNoCondUpdates_SHADD8_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SHSUB8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SHSUB8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case17
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase17 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase17(
      state_.Binary3RegisterOpAltBNoCondUpdates_SHSUB8_instance_)
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
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100001nnnndddd11110001mmmm',
//       rule: 'SADD16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100001nnnndddd11110001mmmm,
//       rule: SADD16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case0_TestCase0) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0 tester;
  tester.Test("cccc01100001nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100001nnnndddd11110011mmmm',
//       rule: 'SASX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100001nnnndddd11110011mmmm,
//       rule: SASX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case1_TestCase1) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1 tester;
  tester.Test("cccc01100001nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100001nnnndddd11110101mmmm',
//       rule: 'SSAX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100001nnnndddd11110101mmmm,
//       rule: SSAX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case2_TestCase2) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2 tester;
  tester.Test("cccc01100001nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100001nnnndddd11110111mmmm',
//       rule: 'SSSUB16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100001nnnndddd11110111mmmm,
//       rule: SSSUB16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case3_TestCase3) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3 tester;
  tester.Test("cccc01100001nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100001nnnndddd11111001mmmm',
//       rule: 'SADD8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100001nnnndddd11111001mmmm,
//       rule: SADD8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case4_TestCase4) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case4 tester;
  tester.Test("cccc01100001nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100001nnnndddd11111111mmmm',
//       rule: 'SSUB8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100001nnnndddd11111111mmmm,
//       rule: SSUB8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case5_TestCase5) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5 tester;
  tester.Test("cccc01100001nnnndddd11111111mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100010nnnndddd11110001mmmm',
//       rule: 'QADD16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100010nnnndddd11110001mmmm,
//       rule: QADD16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case6_TestCase6) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case6 tester;
  tester.Test("cccc01100010nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100010nnnndddd11110011mmmm',
//       rule: 'QASX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100010nnnndddd11110011mmmm,
//       rule: QASX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case7_TestCase7) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case7 tester;
  tester.Test("cccc01100010nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100010nnnndddd11110101mmmm',
//       rule: 'QSAX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100010nnnndddd11110101mmmm,
//       rule: QSAX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case8_TestCase8) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8 tester;
  tester.Test("cccc01100010nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100010nnnndddd11110111mmmm',
//       rule: 'QSUB16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100010nnnndddd11110111mmmm,
//       rule: QSUB16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case9_TestCase9) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case9 tester;
  tester.Test("cccc01100010nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100010nnnndddd11111001mmmm',
//       rule: 'QADD8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100010nnnndddd11111001mmmm,
//       rule: QADD8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case10_TestCase10) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case10 tester;
  tester.Test("cccc01100010nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100010nnnndddd11111111mmmm',
//       rule: 'QSUB8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100010nnnndddd11111111mmmm,
//       rule: QSUB8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case11_TestCase11) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11 tester;
  tester.Test("cccc01100010nnnndddd11111111mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100011nnnndddd11110001mmmm',
//       rule: 'SHADD16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100011nnnndddd11110001mmmm,
//       rule: SHADD16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case12_TestCase12) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case12 tester;
  tester.Test("cccc01100011nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100011nnnndddd11110011mmmm',
//       rule: 'SHASX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100011nnnndddd11110011mmmm,
//       rule: SHASX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case13_TestCase13) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case13 tester;
  tester.Test("cccc01100011nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100011nnnndddd11110101mmmm',
//       rule: 'SHSAX',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100011nnnndddd11110101mmmm,
//       rule: SHSAX,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case14_TestCase14) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14 tester;
  tester.Test("cccc01100011nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100011nnnndddd11110111mmmm',
//       rule: 'SHSUB16',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100011nnnndddd11110111mmmm,
//       rule: SHSUB16,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case15_TestCase15) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case15 tester;
  tester.Test("cccc01100011nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100011nnnndddd11111001mmmm',
//       rule: 'SHADD8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100011nnnndddd11111001mmmm,
//       rule: SHADD8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case16_TestCase16) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case16 tester;
  tester.Test("cccc01100011nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01100011nnnndddd11111111mmmm',
//       rule: 'SHSUB8',
//       safety: ['15 == inst(15:12) || 15 == inst(19:16) || 15 == inst(3:0) => UNPREDICTABLE']}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01100011nnnndddd11111111mmmm,
//       rule: SHSUB8,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case17_TestCase17) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17 tester;
  tester.Test("cccc01100011nnnndddd11111111mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
