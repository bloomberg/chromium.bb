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


// op1(22:21)=00
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualOp,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0,
//       safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpTesterCase0
    : public Binary4RegisterDualOpTester {
 public:
  Binary4RegisterDualOpTesterCase0(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:21)=~00
  if ((inst.Bits() & 0x00600000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualOpTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == (((inst.Bits() & 0x0000F000) >> 12))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:21)=01 & op(5)=0
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualOp,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0,
//       safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpTesterCase1
    : public Binary4RegisterDualOpTester {
 public:
  Binary4RegisterDualOpTesterCase1(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op(5)=~0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualOpTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == (((inst.Bits() & 0x0000F000) >> 12))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary3RegisterOpAltA,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm}}
class Binary3RegisterOpAltATesterCase2
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltATesterCase2(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op(5)=~1
  if ((inst.Bits() & 0x00000020)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltATesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltATester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:21)=10
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualResult,
//       baseline: Binary4RegisterDualResult,
//       constraints: ,
//       defs: {RdLo, RdHi},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0,
//       safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdLo, RdHi, Rn, Rm}}
class Binary4RegisterDualResultTesterCase3
    : public Binary4RegisterDualResultTester {
 public:
  Binary4RegisterDualResultTesterCase3(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:21)=~10
  if ((inst.Bits() & 0x00600000)  !=
          0x00400000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualResultTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualResultTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // safety: RdHi  ==
  //          RdLo => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (((inst.Bits() & 0x0000F000) >> 12))));

  // defs: {RdLo, RdHi};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary3RegisterOpAltA,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm}}
class Binary3RegisterOpAltATesterCase4
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltATesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltATesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltATester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op1(22:21)=00
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualOp,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0,
//       rule: SMLABB_SMLABT_SMLATB_SMLATT,
//       safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpTester_Case0
    : public Binary4RegisterDualOpTesterCase0 {
 public:
  Binary4RegisterDualOpTester_Case0()
    : Binary4RegisterDualOpTesterCase0(
      state_.Binary4RegisterDualOp_SMLABB_SMLABT_SMLATB_SMLATT_instance_)
  {}
};

// op1(22:21)=01 & op(5)=0
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualOp,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0,
//       rule: SMLAWB_SMLAWT,
//       safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpTester_Case1
    : public Binary4RegisterDualOpTesterCase1 {
 public:
  Binary4RegisterDualOpTester_Case1()
    : Binary4RegisterDualOpTesterCase1(
      state_.Binary4RegisterDualOp_SMLAWB_SMLAWT_instance_)
  {}
};

// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary3RegisterOpAltA,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0,
//       rule: SMULWB_SMULWT,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm}}
class Binary3RegisterOpAltATester_Case2
    : public Binary3RegisterOpAltATesterCase2 {
 public:
  Binary3RegisterOpAltATester_Case2()
    : Binary3RegisterOpAltATesterCase2(
      state_.Binary3RegisterOpAltA_SMULWB_SMULWT_instance_)
  {}
};

// op1(22:21)=10
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualResult,
//       baseline: Binary4RegisterDualResult,
//       constraints: ,
//       defs: {RdLo, RdHi},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0,
//       rule: SMLALBB_SMLALBT_SMLALTB_SMLALTT,
//       safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdLo, RdHi, Rn, Rm}}
class Binary4RegisterDualResultTester_Case3
    : public Binary4RegisterDualResultTesterCase3 {
 public:
  Binary4RegisterDualResultTester_Case3()
    : Binary4RegisterDualResultTesterCase3(
      state_.Binary4RegisterDualResult_SMLALBB_SMLALBT_SMLALTB_SMLALTT_instance_)
  {}
};

// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary3RegisterOpAltA,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0,
//       rule: SMULBB_SMULBT_SMULTB_SMULTT,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm}}
class Binary3RegisterOpAltATester_Case4
    : public Binary3RegisterOpAltATesterCase4 {
 public:
  Binary3RegisterOpAltATester_Case4()
    : Binary3RegisterOpAltATesterCase4(
      state_.Binary3RegisterOpAltA_SMULBB_SMULBT_SMULTB_SMULTT_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op1(22:21)=00
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualOp,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLABB_SMLABT_SMLATB_SMLATT_cccc00010000ddddaaaammmm1xx0nnnn_case_0,
//       pattern: cccc00010000ddddaaaammmm1xx0nnnn,
//       rule: SMLABB_SMLABT_SMLATB_SMLATT,
//       safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case0_TestCase0) {
  Binary4RegisterDualOpTester_Case0 tester;
  tester.Test("cccc00010000ddddaaaammmm1xx0nnnn");
}

// op1(22:21)=01 & op(5)=0
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualOp,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLAWB_SMLAWT_cccc00010010ddddaaaammmm1x00nnnn_case_0,
//       pattern: cccc00010010ddddaaaammmm1x00nnnn,
//       rule: SMLAWB_SMLAWT,
//       safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case1_TestCase1) {
  Binary4RegisterDualOpTester_Case1 tester;
  tester.Test("cccc00010010ddddaaaammmm1x00nnnn");
}

// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary3RegisterOpAltA,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0,
//       pattern: cccc00010010dddd0000mmmm1x10nnnn,
//       rule: SMULWB_SMULWT,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case2_TestCase2) {
  Binary3RegisterOpAltATester_Case2 tester;
  tester.Test("cccc00010010dddd0000mmmm1x10nnnn");
}

// op1(22:21)=10
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary4RegisterDualResult,
//       baseline: Binary4RegisterDualResult,
//       constraints: ,
//       defs: {RdLo, RdHi},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_0,
//       pattern: cccc00010100hhhhllllmmmm1xx0nnnn,
//       rule: SMLALBB_SMLALBT_SMLALTB_SMLALTT,
//       safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdLo, RdHi, Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case3_TestCase3) {
  Binary4RegisterDualResultTester_Case3 tester;
  tester.Test("cccc00010100hhhhllllmmmm1xx0nnnn");
}

// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Binary3RegisterOpAltA,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0,
//       pattern: cccc00010110dddd0000mmmm1xx0nnnn,
//       rule: SMULBB_SMULBT_SMULTB_SMULTT,
//       safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case4_TestCase4) {
  Binary3RegisterOpAltATester_Case4 tester;
  tester.Test("cccc00010110dddd0000mmmm1xx0nnnn");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
