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
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=1011
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~0xx1x
  if ((inst.Bits() & 0x01200000)  !=
          0x00200000) return false;
  // op2(7:4)=~1011
  if ((inst.Bits() & 0x000000F0)  !=
          0x000000B0) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=11x1
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
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
  // op(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~0xx1x
  if ((inst.Bits() & 0x01200000)  !=
          0x00200000) return false;
  // op2(7:4)=~11x1
  if ((inst.Bits() & 0x000000D0)  !=
          0x000000D0) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=1 & inst(24:20)=10000
//    = {baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [inst(15:12)=1111 => UNPREDICTABLE]}
//
// Representaive case:
// op(25)=1 & op1(24:20)=10000
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterImmediateOpTesterCase2
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase2(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~10000
  if ((inst.Bits() & 0x01F00000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rd(15:12)=1111 => UNPREDICTABLE
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
// inst(25)=1 & inst(24:20)=10100
//    = {baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       safety: [inst(15:12)=1111 => UNPREDICTABLE]}
//
// Representaive case:
// op(25)=1 & op1(24:20)=10100
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterImmediateOpTesterCase3
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase3(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~10100
  if ((inst.Bits() & 0x01F00000)  !=
          0x01400000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rd(15:12)=1111 => UNPREDICTABLE
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
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=1011
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'extra_load_store_instructions_unpriviledged'}
//
// Representative case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: extra_load_store_instructions_unpriviledged}
class ForbiddenCondDecoderTester_Case0
    : public UnsafeCondDecoderTesterCase0 {
 public:
  ForbiddenCondDecoderTester_Case0()
    : UnsafeCondDecoderTesterCase0(
      state_.ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=11x1
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'extra_load_store_instructions_unpriviledged'}
//
// Representative case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: extra_load_store_instructions_unpriviledged}
class ForbiddenCondDecoderTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenCondDecoderTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=10000
//    = {baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'MOVW',
//       safety: [inst(15:12)=1111 => UNPREDICTABLE]}
//
// Representative case:
// op(25)=1 & op1(24:20)=10000
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: MOVW,
//       safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterImmediateOpDynCodeReplaceTester_Case2
    : public Unary1RegisterImmediateOpTesterCase2 {
 public:
  Unary1RegisterImmediateOpDynCodeReplaceTester_Case2()
    : Unary1RegisterImmediateOpTesterCase2(
      state_.Unary1RegisterImmediateOpDynCodeReplace_MOVW_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=10100
//    = {baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       rule: 'MOVT',
//       safety: [inst(15:12)=1111 => UNPREDICTABLE]}
//
// Representative case:
// op(25)=1 & op1(24:20)=10100
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       rule: MOVT,
//       safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterImmediateOpDynCodeReplaceTester_Case3
    : public Unary1RegisterImmediateOpTesterCase3 {
 public:
  Unary1RegisterImmediateOpDynCodeReplaceTester_Case3()
    : Unary1RegisterImmediateOpTesterCase3(
      state_.Unary1RegisterImmediateOpDynCodeReplace_MOVT_instance_)
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
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=1011
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0000xx1xxxxxxxxxxxxx1xx1xxxx',
//       rule: 'extra_load_store_instructions_unpriviledged'}
//
// Representaive case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0000xx1xxxxxxxxxxxxx1xx1xxxx,
//       rule: extra_load_store_instructions_unpriviledged}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case0_TestCase0) {
  ForbiddenCondDecoderTester_Case0 tester;
  tester.Test("cccc0000xx1xxxxxxxxxxxxx1xx1xxxx");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=11x1
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0000xx1xxxxxxxxxxxxx1xx1xxxx',
//       rule: 'extra_load_store_instructions_unpriviledged'}
//
// Representaive case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0000xx1xxxxxxxxxxxxx1xx1xxxx,
//       rule: extra_load_store_instructions_unpriviledged}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case1_TestCase1) {
  ForbiddenCondDecoderTester_Case1 tester;
  tester.Test("cccc0000xx1xxxxxxxxxxxxx1xx1xxxx");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=10000
//    = {actual: 'Unary1RegisterImmediateOpDynCodeReplace',
//       baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc00110000iiiiddddiiiiiiiiiiii',
//       rule: 'MOVW',
//       safety: [inst(15:12)=1111 => UNPREDICTABLE]}
//
// Representaive case:
// op(25)=1 & op1(24:20)=10000
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary1RegisterImmediateOpDynCodeReplace,
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc00110000iiiiddddiiiiiiiiiiii,
//       rule: MOVW,
//       safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpDynCodeReplaceTester_Case2_TestCase2) {
  Unary1RegisterImmediateOpDynCodeReplaceTester_Case2 tester;
  tester.Test("cccc00110000iiiiddddiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=10100
//    = {actual: 'Unary1RegisterImmediateOpDynCodeReplace',
//       baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12), 16
//            if inst(20)
//            else 32},
//       pattern: 'cccc00110100iiiiddddiiiiiiiiiiii',
//       rule: 'MOVT',
//       safety: [inst(15:12)=1111 => UNPREDICTABLE]}
//
// Representaive case:
// op(25)=1 & op1(24:20)=10100
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary1RegisterImmediateOpDynCodeReplace,
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc00110100iiiiddddiiiiiiiiiiii,
//       rule: MOVT,
//       safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpDynCodeReplaceTester_Case3_TestCase3) {
  Unary1RegisterImmediateOpDynCodeReplaceTester_Case3 tester;
  tester.Test("cccc00110100iiiiddddiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
