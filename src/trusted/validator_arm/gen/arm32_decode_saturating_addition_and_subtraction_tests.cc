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
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
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
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
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
  // op(22:21)=~00
  if ((inst.Bits() & 0x00600000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
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
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
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
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
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
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase1
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
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
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
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
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
  // op(22:21)=~10
  if ((inst.Bits() & 0x00600000)  !=
          0x00400000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000000) return false;

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
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
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
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
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
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
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

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QADD',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QADD,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case0
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase0 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase0(
      state_.Binary3RegisterOpAltBNoCondUpdates_QADD_instance_)
  {}
};

// Neutral case:
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QSUB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QSUB,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case1
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase1 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase1(
      state_.Binary3RegisterOpAltBNoCondUpdates_QSUB_instance_)
  {}
};

// Neutral case:
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QDADD',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QDADD,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase2 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase2(
      state_.Binary3RegisterOpAltBNoCondUpdates_QDADD_instance_)
  {}
};

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'QDSUB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       rule: QDSUB,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case3
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase3 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase3(
      state_.Binary3RegisterOpAltBNoCondUpdates_QDSUB_instance_)
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
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc00010000nnnndddd00000101mmmm',
//       rule: 'QADD',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc00010000nnnndddd00000101mmmm,
//       rule: QADD,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case0_TestCase0) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0 tester;
  tester.Test("cccc00010000nnnndddd00000101mmmm");
}

// Neutral case:
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc00010010nnnndddd00000101mmmm',
//       rule: 'QSUB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc00010010nnnndddd00000101mmmm,
//       rule: QSUB,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case1_TestCase1) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1 tester;
  tester.Test("cccc00010010nnnndddd00000101mmmm");
}

// Neutral case:
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc00010100nnnndddd00000101mmmm',
//       rule: 'QDADD',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc00010100nnnndddd00000101mmmm,
//       rule: QDADD,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case2_TestCase2) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2 tester;
  tester.Test("cccc00010100nnnndddd00000101mmmm");
}

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Binary3RegisterOpAltBNoCondUpdates',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc00010110nnnndddd00000101mmmm',
//       rule: 'QDSUB',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(19:16) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representaive case:
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: Binary3RegisterOpAltBNoCondUpdates,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc00010110nnnndddd00000101mmmm,
//       rule: QDSUB,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case3_TestCase3) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3 tester;
  tester.Test("cccc00010110nnnndddd00000101mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
