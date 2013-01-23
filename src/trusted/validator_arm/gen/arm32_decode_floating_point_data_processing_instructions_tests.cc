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


// opc1(23:20)=0x00
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase0
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase0(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~0x00
  if ((inst.Bits() & 0x00B00000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x01
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~0x01
  if ((inst.Bits() & 0x00B00000)  !=
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase2
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase2(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~0x10
  if ((inst.Bits() & 0x00B00000)  !=
          0x00200000) return false;
  // opc3(7:6)=~x0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase3
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase3(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~0x10
  if ((inst.Bits() & 0x00B00000)  !=
          0x00200000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase4
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase4(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~0x11
  if ((inst.Bits() & 0x00B00000)  !=
          0x00300000) return false;
  // opc3(7:6)=~x0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase5
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase5(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~0x11
  if ((inst.Bits() & 0x00B00000)  !=
          0x00300000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase6
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase6(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~1x00
  if ((inst.Bits() & 0x00B00000)  !=
          0x00800000) return false;
  // opc3(7:6)=~x0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=1x01
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase7
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase7(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~1x01
  if ((inst.Bits() & 0x00B00000)  !=
          0x00900000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=1x10
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase8
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase8(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc1(23:20)=~1x10
  if ((inst.Bits() & 0x00B00000)  !=
          0x00A00000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// opc1(23:20)=0x00
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vmla_vmls_Rule_423_A2_P636}
class CondVfpOpTester_Case0
    : public CondVfpOpTesterCase0 {
 public:
  CondVfpOpTester_Case0()
    : CondVfpOpTesterCase0(
      state_.CondVfpOp_Vmla_vmls_Rule_423_A2_P636_instance_)
  {}
};

// opc1(23:20)=0x01
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vnmla_vnmls_Rule_343_A1_P674}
class CondVfpOpTester_Case1
    : public CondVfpOpTesterCase1 {
 public:
  CondVfpOpTester_Case1()
    : CondVfpOpTesterCase1(
      state_.CondVfpOp_Vnmla_vnmls_Rule_343_A1_P674_instance_)
  {}
};

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vmul_Rule_338_A2_P664}
class CondVfpOpTester_Case2
    : public CondVfpOpTesterCase2 {
 public:
  CondVfpOpTester_Case2()
    : CondVfpOpTesterCase2(
      state_.CondVfpOp_Vmul_Rule_338_A2_P664_instance_)
  {}
};

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vnmul_Rule_343_A2_P674}
class CondVfpOpTester_Case3
    : public CondVfpOpTesterCase3 {
 public:
  CondVfpOpTester_Case3()
    : CondVfpOpTesterCase3(
      state_.CondVfpOp_Vnmul_Rule_343_A2_P674_instance_)
  {}
};

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vadd_Rule_271_A2_P536}
class CondVfpOpTester_Case4
    : public CondVfpOpTesterCase4 {
 public:
  CondVfpOpTester_Case4()
    : CondVfpOpTesterCase4(
      state_.CondVfpOp_Vadd_Rule_271_A2_P536_instance_)
  {}
};

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vsub_Rule_402_A2_P790}
class CondVfpOpTester_Case5
    : public CondVfpOpTesterCase5 {
 public:
  CondVfpOpTester_Case5()
    : CondVfpOpTesterCase5(
      state_.CondVfpOp_Vsub_Rule_402_A2_P790_instance_)
  {}
};

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vdiv_Rule_301_A1_P590}
class CondVfpOpTester_Case6
    : public CondVfpOpTesterCase6 {
 public:
  CondVfpOpTester_Case6()
    : CondVfpOpTesterCase6(
      state_.CondVfpOp_Vdiv_Rule_301_A1_P590_instance_)
  {}
};

// opc1(23:20)=1x01
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vfnma_vfnms_Rule_A1}
class CondVfpOpTester_Case7
    : public CondVfpOpTesterCase7 {
 public:
  CondVfpOpTester_Case7()
    : CondVfpOpTesterCase7(
      state_.CondVfpOp_Vfnma_vfnms_Rule_A1_instance_)
  {}
};

// opc1(23:20)=1x10
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       rule: Vfma_vfms_Rule_A1}
class CondVfpOpTester_Case8
    : public CondVfpOpTesterCase8 {
 public:
  CondVfpOpTester_Case8()
    : CondVfpOpTesterCase8(
      state_.CondVfpOp_Vfma_vfms_Rule_A1_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// opc1(23:20)=0x00
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11100d00nnnndddd101snom0mmmm,
//       rule: Vmla_vmls_Rule_423_A2_P636}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case0_TestCase0) {
  CondVfpOpTester_Case0 baseline_tester;
  NamedVfpOp_Vmla_vmls_Rule_423_A2_P636 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d00nnnndddd101snom0mmmm");
}

// opc1(23:20)=0x01
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11100d01nnnndddd101snom0mmmm,
//       rule: Vnmla_vnmls_Rule_343_A1_P674}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case1_TestCase1) {
  CondVfpOpTester_Case1 baseline_tester;
  NamedVfpOp_Vnmla_vnmls_Rule_343_A1_P674 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d01nnnndddd101snom0mmmm");
}

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11100d10nnnndddd101sn0m0mmmm,
//       rule: Vmul_Rule_338_A2_P664}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case2_TestCase2) {
  CondVfpOpTester_Case2 baseline_tester;
  NamedVfpOp_Vmul_Rule_338_A2_P664 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d10nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11100d10nnnndddd101sn1m0mmmm,
//       rule: Vnmul_Rule_343_A2_P674}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case3_TestCase3) {
  CondVfpOpTester_Case3 baseline_tester;
  NamedVfpOp_Vnmul_Rule_343_A2_P674 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d10nnnndddd101sn1m0mmmm");
}

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11100d11nnnndddd101sn0m0mmmm,
//       rule: Vadd_Rule_271_A2_P536}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case4_TestCase4) {
  CondVfpOpTester_Case4 baseline_tester;
  NamedVfpOp_Vadd_Rule_271_A2_P536 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d11nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11100d11nnnndddd101sn1m0mmmm,
//       rule: Vsub_Rule_402_A2_P790}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case5_TestCase5) {
  CondVfpOpTester_Case5 baseline_tester;
  NamedVfpOp_Vsub_Rule_402_A2_P790 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d11nnnndddd101sn1m0mmmm");
}

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d00nnnndddd101sn0m0mmmm,
//       rule: Vdiv_Rule_301_A1_P590}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case6_TestCase6) {
  CondVfpOpTester_Case6 baseline_tester;
  NamedVfpOp_Vdiv_Rule_301_A1_P590 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d00nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=1x01
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d01nnnndddd101snom0mmmm,
//       rule: Vfnma_vfnms_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case7_TestCase7) {
  CondVfpOpTester_Case7 baseline_tester;
  NamedVfpOp_Vfnma_vfnms_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d01nnnndddd101snom0mmmm");
}

// opc1(23:20)=1x10
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d10nnnndddd101snom0mmmm,
//       rule: Vfma_vfms_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case8_TestCase8) {
  CondVfpOpTester_Case8 baseline_tester;
  NamedVfpOp_Vfma_vfms_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d10nnnndddd101snom0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
