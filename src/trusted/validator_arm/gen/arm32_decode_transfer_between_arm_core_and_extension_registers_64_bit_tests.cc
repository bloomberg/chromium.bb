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
#include "native_client/src/trusted/validator_arm/baseline_vs_baseline.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"
#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"
#include "native_client/src/trusted/validator_arm/arm_helpers.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_bases.h"

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


// C(8)=0 & op(7:4)=00x1
//    = {M: M(5),
//       Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt2(19:16),
//       Vm: Vm(3:0),
//       actual: Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1,
//       baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       defs: {Rt, Rt2}
//            if to_arm_registers
//            else {},
//       fields: [op(20), Rt2(19:16), Rt(15:12), M(5), Vm(3:0)],
//       generated_baseline: VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_0,
//       m: Vm:M,
//       op: op(20),
//       pattern: cccc1100010otttttttt101000m1mmmm,
//       rule: VMOV_between_two_ARM_core_registers_and_two_single_precision_registers,
//       safety: [Pc in {t, t2} ||
//            m  ==
//               31 => UNPREDICTABLE,
//         to_arm_registers &&
//            t  ==
//               t2 => UNPREDICTABLE],
//       t: Rt,
//       t2: Rt2,
//       to_arm_registers: op(20)=1,
//       uses: {}
//            if to_arm_registers
//            else {Rt, Rt2}}
class MoveDoubleVfpRegisterOpTesterCase0
    : public MoveDoubleVfpRegisterOpTester {
 public:
  MoveDoubleVfpRegisterOpTesterCase0(const NamedClassDecoder& decoder)
    : MoveDoubleVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool MoveDoubleVfpRegisterOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // C(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;
  // op(7:4)=~00x1
  if ((inst.Bits() & 0x000000D0)  !=
          0x00000010) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return MoveDoubleVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool MoveDoubleVfpRegisterOpTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveDoubleVfpRegisterOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {t, t2} ||
  //       m  ==
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!(((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16))))) ||
       (((((((inst.Bits() & 0x0000000F)) << 1) | ((inst.Bits() & 0x00000020) >> 5))) == (31)))));

  // safety: to_arm_registers &&
  //       t  ==
  //          t2 => UNPREDICTABLE
  EXPECT_TRUE(!(((inst.Bits() & 0x00100000)  ==
          0x00100000) &&
       (((((inst.Bits() & 0x0000F000) >> 12)) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // defs: {Rt, Rt2}
  //       if to_arm_registers
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// C(8)=1 & op(7:4)=00x1
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt2(19:16),
//       actual: Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1,
//       baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       defs: {Rt, Rt2}
//            if to_arm_registers
//            else {},
//       fields: [op(20), Rt2(19:16), Rt(15:12)],
//       generated_baseline: VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_0,
//       op: op(20),
//       pattern: cccc1100010otttttttt101100m1mmmm,
//       rule: VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register,
//       safety: [Pc in {t, t2} => UNPREDICTABLE,
//         to_arm_registers &&
//            t  ==
//               t2 => UNPREDICTABLE],
//       t: Rt,
//       t2: Rt2,
//       to_arm_registers: op(20)=1,
//       uses: {}
//            if to_arm_registers
//            else {Rt, Rt2}}
class MoveDoubleVfpRegisterOpTesterCase1
    : public MoveDoubleVfpRegisterOpTester {
 public:
  MoveDoubleVfpRegisterOpTesterCase1(const NamedClassDecoder& decoder)
    : MoveDoubleVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool MoveDoubleVfpRegisterOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // C(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;
  // op(7:4)=~00x1
  if ((inst.Bits() & 0x000000D0)  !=
          0x00000010) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return MoveDoubleVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool MoveDoubleVfpRegisterOpTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveDoubleVfpRegisterOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {t, t2} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: to_arm_registers &&
  //       t  ==
  //          t2 => UNPREDICTABLE
  EXPECT_TRUE(!(((inst.Bits() & 0x00100000)  ==
          0x00100000) &&
       (((((inst.Bits() & 0x0000F000) >> 12)) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // defs: {Rt, Rt2}
  //       if to_arm_registers
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// C(8)=0 & op(7:4)=00x1
//    = {M: M(5),
//       Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt2(19:16),
//       Vm: Vm(3:0),
//       actual: Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1,
//       baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       defs: {Rt, Rt2}
//            if to_arm_registers
//            else {},
//       fields: [op(20), Rt2(19:16), Rt(15:12), M(5), Vm(3:0)],
//       generated_baseline: VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_0,
//       m: Vm:M,
//       op: op(20),
//       pattern: cccc1100010otttttttt101000m1mmmm,
//       rule: VMOV_between_two_ARM_core_registers_and_two_single_precision_registers,
//       safety: [Pc in {t, t2} ||
//            m  ==
//               31 => UNPREDICTABLE,
//         to_arm_registers &&
//            t  ==
//               t2 => UNPREDICTABLE],
//       t: Rt,
//       t2: Rt2,
//       to_arm_registers: op(20)=1,
//       uses: {}
//            if to_arm_registers
//            else {Rt, Rt2}}
class MoveDoubleVfpRegisterOpTester_Case0
    : public MoveDoubleVfpRegisterOpTesterCase0 {
 public:
  MoveDoubleVfpRegisterOpTester_Case0()
    : MoveDoubleVfpRegisterOpTesterCase0(
      state_.MoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_instance_)
  {}
};

// C(8)=1 & op(7:4)=00x1
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt2(19:16),
//       actual: Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1,
//       baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       defs: {Rt, Rt2}
//            if to_arm_registers
//            else {},
//       fields: [op(20), Rt2(19:16), Rt(15:12)],
//       generated_baseline: VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_0,
//       op: op(20),
//       pattern: cccc1100010otttttttt101100m1mmmm,
//       rule: VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register,
//       safety: [Pc in {t, t2} => UNPREDICTABLE,
//         to_arm_registers &&
//            t  ==
//               t2 => UNPREDICTABLE],
//       t: Rt,
//       t2: Rt2,
//       to_arm_registers: op(20)=1,
//       uses: {}
//            if to_arm_registers
//            else {Rt, Rt2}}
class MoveDoubleVfpRegisterOpTester_Case1
    : public MoveDoubleVfpRegisterOpTesterCase1 {
 public:
  MoveDoubleVfpRegisterOpTester_Case1()
    : MoveDoubleVfpRegisterOpTesterCase1(
      state_.MoveDoubleVfpRegisterOp_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// C(8)=0 & op(7:4)=00x1
//    = {M: M(5),
//       Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt2(19:16),
//       Vm: Vm(3:0),
//       actual: Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1,
//       baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       defs: {Rt, Rt2}
//            if to_arm_registers
//            else {},
//       fields: [op(20), Rt2(19:16), Rt(15:12), M(5), Vm(3:0)],
//       generated_baseline: VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_0,
//       m: Vm:M,
//       op: op(20),
//       pattern: cccc1100010otttttttt101000m1mmmm,
//       rule: VMOV_between_two_ARM_core_registers_and_two_single_precision_registers,
//       safety: [Pc in {t, t2} ||
//            m  ==
//               31 => UNPREDICTABLE,
//         to_arm_registers &&
//            t  ==
//               t2 => UNPREDICTABLE],
//       t: Rt,
//       t2: Rt2,
//       to_arm_registers: op(20)=1,
//       uses: {}
//            if to_arm_registers
//            else {Rt, Rt2}}
TEST_F(Arm32DecoderStateTests,
       MoveDoubleVfpRegisterOpTester_Case0_TestCase0) {
  MoveDoubleVfpRegisterOpTester_Case0 baseline_tester;
  NamedActual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1100010otttttttt101000m1mmmm");
}

// C(8)=1 & op(7:4)=00x1
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt2(19:16),
//       actual: Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1,
//       baseline: MoveDoubleVfpRegisterOp,
//       constraints: ,
//       defs: {Rt, Rt2}
//            if to_arm_registers
//            else {},
//       fields: [op(20), Rt2(19:16), Rt(15:12)],
//       generated_baseline: VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_0,
//       op: op(20),
//       pattern: cccc1100010otttttttt101100m1mmmm,
//       rule: VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register,
//       safety: [Pc in {t, t2} => UNPREDICTABLE,
//         to_arm_registers &&
//            t  ==
//               t2 => UNPREDICTABLE],
//       t: Rt,
//       t2: Rt2,
//       to_arm_registers: op(20)=1,
//       uses: {}
//            if to_arm_registers
//            else {Rt, Rt2}}
TEST_F(Arm32DecoderStateTests,
       MoveDoubleVfpRegisterOpTester_Case1_TestCase1) {
  MoveDoubleVfpRegisterOpTester_Case1 baseline_tester;
  NamedActual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1100010otttttttt101100m1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
