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
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000)  !=
          0x000F0000);

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase1
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase1(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~101
  if ((inst.Bits() & 0x000000E0)  !=
          0x000000A0) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=xx0
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~xx0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
class Unary2RegisterSatImmedShiftedOpTesterCase4
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase4(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~010
  if ((inst.Bits() & 0x00700000)  !=
          0x00200000) return false;
  // op2(7:5)=~001
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterSatImmedShiftedOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterSatImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~010
  if ((inst.Bits() & 0x00700000)  !=
          0x00200000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000)  !=
          0x000F0000);

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase6
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase6(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~010
  if ((inst.Bits() & 0x00700000)  !=
          0x00200000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase7
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase7(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~001
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000)  !=
          0x000F0000);

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase9
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase9(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase10
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase10(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~101
  if ((inst.Bits() & 0x000000E0)  !=
          0x000000A0) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~100
  if ((inst.Bits() & 0x00700000)  !=
          0x00400000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000)  !=
          0x000F0000);

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase12
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase12(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~100
  if ((inst.Bits() & 0x00700000)  !=
          0x00400000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
class Unary2RegisterSatImmedShiftedOpTesterCase13
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase13(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~110
  if ((inst.Bits() & 0x00700000)  !=
          0x00600000) return false;
  // op2(7:5)=~001
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterSatImmedShiftedOpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterSatImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~110
  if ((inst.Bits() & 0x00700000)  !=
          0x00600000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000)  !=
          0x000F0000);

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase15
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase15(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~110
  if ((inst.Bits() & 0x00700000)  !=
          0x00600000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase16
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase16(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~111
  if ((inst.Bits() & 0x00700000)  !=
          0x00700000) return false;
  // op2(7:5)=~001
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~111
  if ((inst.Bits() & 0x00700000)  !=
          0x00700000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000)  !=
          0x000F0000);

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
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
  // op1(22:20)=~111
  if ((inst.Bits() & 0x00700000)  !=
          0x00700000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTesterCase19
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  Unary2RegisterImmedShiftedOpTesterCase19(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~111
  if ((inst.Bits() & 0x00700000)  !=
          0x00700000) return false;
  // op2(7:5)=~101
  if ((inst.Bits() & 0x000000E0)  !=
          0x000000A0) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterImmedShiftedOpTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=01x & inst(7:5)=xx0
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
class Unary2RegisterSatImmedShiftedOpTesterCase20
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase20(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~01x
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op2(7:5)=~xx0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterSatImmedShiftedOpTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterSatImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(22:20)=11x & inst(7:5)=xx0
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
class Unary2RegisterSatImmedShiftedOpTesterCase21
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase21(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~11x
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // op2(7:5)=~xx0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterSatImmedShiftedOpTesterCase21
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterSatImmedShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SXTAB16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SXTAB16,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case0
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase0 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase0(
      state_.Binary3RegisterOpAltBNoCondUpdates_SXTAB16_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SXTB16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: SXTB16,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case1
    : public Unary2RegisterImmedShiftedOpTesterCase1 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case1()
    : Unary2RegisterImmedShiftedOpTesterCase1(
      state_.Unary2RegisterImmedShiftedOp_SXTB16_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SEL',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SEL,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase2 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase2(
      state_.Binary3RegisterOpAltBNoCondUpdates_SEL_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=xx0
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'PKH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: PKH,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case3
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase3 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase3(
      state_.Binary3RegisterOpAltBNoCondUpdates_PKH_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SSAT16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       rule: SSAT16,
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
class Unary2RegisterSatImmedShiftedOpTester_Case4
    : public Unary2RegisterSatImmedShiftedOpTesterCase4 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case4()
    : Unary2RegisterSatImmedShiftedOpTesterCase4(
      state_.Unary2RegisterSatImmedShiftedOp_SSAT16_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SXTAB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SXTAB,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case5
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase5 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase5(
      state_.Binary3RegisterOpAltBNoCondUpdates_SXTAB_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SXTB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: SXTB,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case6
    : public Unary2RegisterImmedShiftedOpTesterCase6 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case6()
    : Unary2RegisterImmedShiftedOpTesterCase6(
      state_.Unary2RegisterImmedShiftedOp_SXTB_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'REV',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: REV,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case7
    : public Unary2RegisterImmedShiftedOpTesterCase7 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case7()
    : Unary2RegisterImmedShiftedOpTesterCase7(
      state_.Unary2RegisterImmedShiftedOp_REV_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SXTAH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: SXTAH,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case8
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase8 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase8(
      state_.Binary3RegisterOpAltBNoCondUpdates_SXTAH_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SXTH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: SXTH,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case9
    : public Unary2RegisterImmedShiftedOpTesterCase9 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case9()
    : Unary2RegisterImmedShiftedOpTesterCase9(
      state_.Unary2RegisterImmedShiftedOp_SXTH_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'REV16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: REV16,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case10
    : public Unary2RegisterImmedShiftedOpTesterCase10 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case10()
    : Unary2RegisterImmedShiftedOpTesterCase10(
      state_.Unary2RegisterImmedShiftedOp_REV16_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'UXTAB16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: UXTAB16,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case11
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase11 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase11(
      state_.Binary3RegisterOpAltBNoCondUpdates_UXTAB16_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'UXTB16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: UXTB16,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case12
    : public Unary2RegisterImmedShiftedOpTesterCase12 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case12()
    : Unary2RegisterImmedShiftedOpTesterCase12(
      state_.Unary2RegisterImmedShiftedOp_UXTB16_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'USAT16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       rule: USAT16,
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
class Unary2RegisterSatImmedShiftedOpTester_Case13
    : public Unary2RegisterSatImmedShiftedOpTesterCase13 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case13()
    : Unary2RegisterSatImmedShiftedOpTesterCase13(
      state_.Unary2RegisterSatImmedShiftedOp_USAT16_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'UXTAB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: UXTAB,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case14
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase14 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase14(
      state_.Binary3RegisterOpAltBNoCondUpdates_UXTAB_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'UXTB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: UXTB,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case15
    : public Unary2RegisterImmedShiftedOpTesterCase15 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case15()
    : Unary2RegisterImmedShiftedOpTesterCase15(
      state_.Unary2RegisterImmedShiftedOp_UXTB_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'RBIT',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: RBIT,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case16
    : public Unary2RegisterImmedShiftedOpTesterCase16 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case16()
    : Unary2RegisterImmedShiftedOpTesterCase16(
      state_.Unary2RegisterImmedShiftedOp_RBIT_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'UXTAH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: UXTAH,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case17
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase17 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase17(
      state_.Binary3RegisterOpAltBNoCondUpdates_UXTAH_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'UXTH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: UXTH,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case18
    : public Unary2RegisterImmedShiftedOpTesterCase18 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case18()
    : Unary2RegisterImmedShiftedOpTesterCase18(
      state_.Unary2RegisterImmedShiftedOp_UXTH_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'REVSH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: REVSH,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterImmedShiftedOpTester_Case19
    : public Unary2RegisterImmedShiftedOpTesterCase19 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case19()
    : Unary2RegisterImmedShiftedOpTesterCase19(
      state_.Unary2RegisterImmedShiftedOp_REVSH_instance_)
  {}
};

// Neutral case:
// inst(22:20)=01x & inst(7:5)=xx0
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'SSAT',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       rule: SSAT,
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
class Unary2RegisterSatImmedShiftedOpTester_Case20
    : public Unary2RegisterSatImmedShiftedOpTesterCase20 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case20()
    : Unary2RegisterSatImmedShiftedOpTesterCase20(
      state_.Unary2RegisterSatImmedShiftedOp_SSAT_instance_)
  {}
};

// Neutral case:
// inst(22:20)=11x & inst(7:5)=xx0
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'USAT',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       rule: USAT,
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
class Unary2RegisterSatImmedShiftedOpTester_Case21
    : public Unary2RegisterSatImmedShiftedOpTesterCase21 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case21()
    : Unary2RegisterSatImmedShiftedOpTesterCase21(
      state_.Unary2RegisterSatImmedShiftedOp_USAT_instance_)
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
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101000nnnnddddrr000111mmmm',
//       rule: 'SXTAB16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01101000nnnnddddrr000111mmmm,
//       rule: SXTAB16,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case0_TestCase0) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0 tester;
  tester.Test("cccc01101000nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011010001111ddddrr000111mmmm',
//       rule: 'SXTB16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011010001111ddddrr000111mmmm,
//       rule: SXTB16,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case1_TestCase1) {
  Unary2RegisterImmedShiftedOpTester_Case1 tester;
  tester.Test("cccc011010001111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101000nnnndddd11111011mmmm',
//       rule: 'SEL',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01101000nnnndddd11111011mmmm,
//       rule: SEL,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case2_TestCase2) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2 tester;
  tester.Test("cccc01101000nnnndddd11111011mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=xx0
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101000nnnnddddiiiiit01mmmm',
//       rule: 'PKH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01101000nnnnddddiiiiit01mmmm,
//       rule: PKH,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case3_TestCase3) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3 tester;
  tester.Test("cccc01101000nnnnddddiiiiit01mmmm");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Unary2RegisterSatImmedShiftedOp',
//       baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101010iiiidddd11110011nnnn',
//       rule: 'SSAT16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       actual: Unary2RegisterSatImmedShiftedOp,
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       pattern: cccc01101010iiiidddd11110011nnnn,
//       rule: SSAT16,
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case4_TestCase4) {
  Unary2RegisterSatImmedShiftedOpTester_Case4 tester;
  tester.Test("cccc01101010iiiidddd11110011nnnn");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101010nnnnddddrr000111mmmm',
//       rule: 'SXTAB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01101010nnnnddddrr000111mmmm,
//       rule: SXTAB,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case5_TestCase5) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5 tester;
  tester.Test("cccc01101010nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011010101111ddddrr000111mmmm',
//       rule: 'SXTB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011010101111ddddrr000111mmmm,
//       rule: SXTB,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case6_TestCase6) {
  Unary2RegisterImmedShiftedOpTester_Case6 tester;
  tester.Test("cccc011010101111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011010111111dddd11110011mmmm',
//       rule: 'REV',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011010111111dddd11110011mmmm,
//       rule: REV,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case7_TestCase7) {
  Unary2RegisterImmedShiftedOpTester_Case7 tester;
  tester.Test("cccc011010111111dddd11110011mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101011nnnnddddrr000111mmmm',
//       rule: 'SXTAH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01101011nnnnddddrr000111mmmm,
//       rule: SXTAH,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case8_TestCase8) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8 tester;
  tester.Test("cccc01101011nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011010111111ddddrr000111mmmm',
//       rule: 'SXTH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011010111111ddddrr000111mmmm,
//       rule: SXTH,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case9_TestCase9) {
  Unary2RegisterImmedShiftedOpTester_Case9 tester;
  tester.Test("cccc011010111111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011010111111dddd11111011mmmm',
//       rule: 'REV16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011010111111dddd11111011mmmm,
//       rule: REV16,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case10_TestCase10) {
  Unary2RegisterImmedShiftedOpTester_Case10 tester;
  tester.Test("cccc011010111111dddd11111011mmmm");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101100nnnnddddrr000111mmmm',
//       rule: 'UXTAB16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01101100nnnnddddrr000111mmmm,
//       rule: UXTAB16,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case11_TestCase11) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11 tester;
  tester.Test("cccc01101100nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011011001111ddddrr000111mmmm',
//       rule: 'UXTB16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011011001111ddddrr000111mmmm,
//       rule: UXTB16,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case12_TestCase12) {
  Unary2RegisterImmedShiftedOpTester_Case12 tester;
  tester.Test("cccc011011001111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Unary2RegisterSatImmedShiftedOp',
//       baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101110iiiidddd11110011nnnn',
//       rule: 'USAT16',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       actual: Unary2RegisterSatImmedShiftedOp,
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       pattern: cccc01101110iiiidddd11110011nnnn,
//       rule: USAT16,
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case13_TestCase13) {
  Unary2RegisterSatImmedShiftedOpTester_Case13 tester;
  tester.Test("cccc01101110iiiidddd11110011nnnn");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101110nnnnddddrr000111mmmm',
//       rule: 'UXTAB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01101110nnnnddddrr000111mmmm,
//       rule: UXTAB,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case14_TestCase14) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14 tester;
  tester.Test("cccc01101110nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011011101111ddddrr000111mmmm',
//       rule: 'UXTB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011011101111ddddrr000111mmmm,
//       rule: UXTB,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case15_TestCase15) {
  Unary2RegisterImmedShiftedOpTester_Case15 tester;
  tester.Test("cccc011011101111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011011111111dddd11110011mmmm',
//       rule: 'RBIT',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011011111111dddd11110011mmmm,
//       rule: RBIT,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case16_TestCase16) {
  Unary2RegisterImmedShiftedOpTester_Case16 tester;
  tester.Test("cccc011011111111dddd11110011mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc01101111nnnnddddrr000111mmmm',
//       rule: 'UXTAH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:16)=1111 => DECODER_ERROR]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc01101111nnnnddddrr000111mmmm,
//       rule: UXTAH,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR,
//         Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case17_TestCase17) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17 tester;
  tester.Test("cccc01101111nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011011111111ddddrr000111mmmm',
//       rule: 'UXTH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011011111111ddddrr000111mmmm,
//       rule: UXTH,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case18_TestCase18) {
  Unary2RegisterImmedShiftedOpTester_Case18 tester;
  tester.Test("cccc011011111111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Unary2RegisterImmedShiftedOp',
//       baseline: 'Unary2RegisterImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc011011111111dddd11111011mmmm',
//       rule: 'REVSH',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterImmedShiftedOp,
//       baseline: Unary2RegisterImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc011011111111dddd11111011mmmm,
//       rule: REVSH,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case19_TestCase19) {
  Unary2RegisterImmedShiftedOpTester_Case19 tester;
  tester.Test("cccc011011111111dddd11111011mmmm");
}

// Neutral case:
// inst(22:20)=01x & inst(7:5)=xx0
//    = {actual: 'Unary2RegisterSatImmedShiftedOp',
//       baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc0110101iiiiiddddiiiiis01nnnn',
//       rule: 'SSAT',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       actual: Unary2RegisterSatImmedShiftedOp,
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       pattern: cccc0110101iiiiiddddiiiiis01nnnn,
//       rule: SSAT,
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case20_TestCase20) {
  Unary2RegisterSatImmedShiftedOpTester_Case20 tester;
  tester.Test("cccc0110101iiiiiddddiiiiis01nnnn");
}

// Neutral case:
// inst(22:20)=11x & inst(7:5)=xx0
//    = {actual: 'Unary2RegisterSatImmedShiftedOp',
//       baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc0110111iiiiiddddiiiiis01nnnn',
//       rule: 'USAT',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rn: Rn(3:0),
//       actual: Unary2RegisterSatImmedShiftedOp,
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rn(3:0)],
//       pattern: cccc0110111iiiiiddddiiiiis01nnnn,
//       rule: USAT,
//       safety: [Pc in {Rd, Rn} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case21_TestCase21) {
  Unary2RegisterSatImmedShiftedOpTester_Case21 tester;
  tester.Test("cccc0110111iiiiiddddiiiiis01nnnn");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
