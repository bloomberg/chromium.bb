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


// U(24)=0 & A(23:19)=1x11x & C(7:4)=xxx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VEXT_111100101d11nnnnddddiiiinqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterImmOp,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), imm4(11:8), Q(6), Vm(3:0)],
//       generated_baseline: VEXT_111100101d11nnnnddddiiiinqm0mmmm_case_0,
//       imm4: imm4(11:8),
//       pattern: 111100101d11nnnnddddiiiinqm0mmmm,
//       rule: VEXT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         Q(6)=0 &&
//            imm4(3)=1 => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterImmOpTesterCase0
    : public VectorBinary3RegisterImmOpTester {
 public:
  VectorBinary3RegisterImmOpTesterCase0(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterImmOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterImmOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // A(23:19)=~1x11x
  if ((inst.Bits() & 0x00B00000)  !=
          0x00B00000) return false;
  // C(7:4)=~xxx0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterImmOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterImmOpTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterImmOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: Q(6)=0 &&
  //       imm4(3)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x00000F00) >> 8) & 0x00000008)  ==
          0x00000008)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// U(24)=1 & A(23:19)=1x11x & B(11:8)=1100 & C(7:4)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VDUP_scalar_111100111d11iiiidddd11000qm0mmmm_case_1,
//       baseline: VectorUnary2RegisterDup,
//       constraints: ,
//       defs: {},
//       fields: [imm4(19:16), Vd(15:12), Q(6)],
//       generated_baseline: VDUP_scalar_111100111d11iiiidddd11000qm0mmmm_case_0,
//       imm4: imm4(19:16),
//       pattern: 111100111d11iiiidddd11000qm0mmmm,
//       rule: VDUP_scalar,
//       safety: [imm4(19:16)=x000 => UNDEFINED,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class VectorUnary2RegisterDupTesterCase1
    : public VectorUnary2RegisterDupTester {
 public:
  VectorUnary2RegisterDupTesterCase1(const NamedClassDecoder& decoder)
    : VectorUnary2RegisterDupTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorUnary2RegisterDupTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // A(23:19)=~1x11x
  if ((inst.Bits() & 0x00B00000)  !=
          0x00B00000) return false;
  // B(11:8)=~1100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000C00) return false;
  // C(7:4)=~0xx0
  if ((inst.Bits() & 0x00000090)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorUnary2RegisterDupTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorUnary2RegisterDupTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorUnary2RegisterDupTester::
               ApplySanityChecks(inst, decoder));

  // safety: imm4(19:16)=x000 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00070000)  !=
          0x00000000);

  // safety: Q(6)=1 &&
  //       Vd(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// U(24)=1 & A(23:19)=1x11x & B(11:8)=10xx & C(7:4)=xxx0
//    = {N: N(7),
//       Vn: Vn(19:16),
//       actual: Actual_VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_1,
//       baseline: VectorBinary3RegisterLookupOp,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), len(9:8), N(7)],
//       generated_baseline: VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_0,
//       len: len(9:8),
//       length: len + 1,
//       n: N:Vn,
//       pattern: 111100111d11nnnndddd10ccnpm0mmmm,
//       rule: VTBL_VTBX,
//       safety: [n + length  >
//               32 => UNPREDICTABLE],
//       uses: {}}
class VectorBinary3RegisterLookupOpTesterCase2
    : public VectorBinary3RegisterLookupOpTester {
 public:
  VectorBinary3RegisterLookupOpTesterCase2(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterLookupOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterLookupOpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // A(23:19)=~1x11x
  if ((inst.Bits() & 0x00B00000)  !=
          0x00B00000) return false;
  // B(11:8)=~10xx
  if ((inst.Bits() & 0x00000C00)  !=
          0x00000800) return false;
  // C(7:4)=~xxx0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterLookupOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterLookupOpTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterLookupOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: n + length  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE((((((((inst.Bits() & 0x00000080) >> 7)) << 4) | ((inst.Bits() & 0x000F0000) >> 16)) + ((inst.Bits() & 0x00000300) >> 8) + 1) <= (32)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// U(24)=0 & A(23:19)=1x11x & C(7:4)=xxx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VEXT_111100101d11nnnnddddiiiinqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterImmOp,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), imm4(11:8), Q(6), Vm(3:0)],
//       generated_baseline: VEXT_111100101d11nnnnddddiiiinqm0mmmm_case_0,
//       imm4: imm4(11:8),
//       pattern: 111100101d11nnnnddddiiiinqm0mmmm,
//       rule: VEXT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         Q(6)=0 &&
//            imm4(3)=1 => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterImmOpTester_Case0
    : public VectorBinary3RegisterImmOpTesterCase0 {
 public:
  VectorBinary3RegisterImmOpTester_Case0()
    : VectorBinary3RegisterImmOpTesterCase0(
      state_.VectorBinary3RegisterImmOp_VEXT_instance_)
  {}
};

// U(24)=1 & A(23:19)=1x11x & B(11:8)=1100 & C(7:4)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VDUP_scalar_111100111d11iiiidddd11000qm0mmmm_case_1,
//       baseline: VectorUnary2RegisterDup,
//       constraints: ,
//       defs: {},
//       fields: [imm4(19:16), Vd(15:12), Q(6)],
//       generated_baseline: VDUP_scalar_111100111d11iiiidddd11000qm0mmmm_case_0,
//       imm4: imm4(19:16),
//       pattern: 111100111d11iiiidddd11000qm0mmmm,
//       rule: VDUP_scalar,
//       safety: [imm4(19:16)=x000 => UNDEFINED,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class VectorUnary2RegisterDupTester_Case1
    : public VectorUnary2RegisterDupTesterCase1 {
 public:
  VectorUnary2RegisterDupTester_Case1()
    : VectorUnary2RegisterDupTesterCase1(
      state_.VectorUnary2RegisterDup_VDUP_scalar_instance_)
  {}
};

// U(24)=1 & A(23:19)=1x11x & B(11:8)=10xx & C(7:4)=xxx0
//    = {N: N(7),
//       Vn: Vn(19:16),
//       actual: Actual_VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_1,
//       baseline: VectorBinary3RegisterLookupOp,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), len(9:8), N(7)],
//       generated_baseline: VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_0,
//       len: len(9:8),
//       length: len + 1,
//       n: N:Vn,
//       pattern: 111100111d11nnnndddd10ccnpm0mmmm,
//       rule: VTBL_VTBX,
//       safety: [n + length  >
//               32 => UNPREDICTABLE],
//       uses: {}}
class VectorBinary3RegisterLookupOpTester_Case2
    : public VectorBinary3RegisterLookupOpTesterCase2 {
 public:
  VectorBinary3RegisterLookupOpTester_Case2()
    : VectorBinary3RegisterLookupOpTesterCase2(
      state_.VectorBinary3RegisterLookupOp_VTBL_VTBX_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// U(24)=0 & A(23:19)=1x11x & C(7:4)=xxx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VEXT_111100101d11nnnnddddiiiinqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterImmOp,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), imm4(11:8), Q(6), Vm(3:0)],
//       generated_baseline: VEXT_111100101d11nnnnddddiiiinqm0mmmm_case_0,
//       imm4: imm4(11:8),
//       pattern: 111100101d11nnnnddddiiiinqm0mmmm,
//       rule: VEXT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         Q(6)=0 &&
//            imm4(3)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterImmOpTester_Case0_TestCase0) {
  VectorBinary3RegisterImmOpTester_Case0 baseline_tester;
  NamedActual_VEXT_111100101d11nnnnddddiiiinqm0mmmm_case_1_VEXT actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100101d11nnnnddddiiiinqm0mmmm");
}

// U(24)=1 & A(23:19)=1x11x & B(11:8)=1100 & C(7:4)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VDUP_scalar_111100111d11iiiidddd11000qm0mmmm_case_1,
//       baseline: VectorUnary2RegisterDup,
//       constraints: ,
//       defs: {},
//       fields: [imm4(19:16), Vd(15:12), Q(6)],
//       generated_baseline: VDUP_scalar_111100111d11iiiidddd11000qm0mmmm_case_0,
//       imm4: imm4(19:16),
//       pattern: 111100111d11iiiidddd11000qm0mmmm,
//       rule: VDUP_scalar,
//       safety: [imm4(19:16)=x000 => UNDEFINED,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorUnary2RegisterDupTester_Case1_TestCase1) {
  VectorUnary2RegisterDupTester_Case1 baseline_tester;
  NamedActual_VDUP_scalar_111100111d11iiiidddd11000qm0mmmm_case_1_VDUP_scalar actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11iiiidddd11000qm0mmmm");
}

// U(24)=1 & A(23:19)=1x11x & B(11:8)=10xx & C(7:4)=xxx0
//    = {N: N(7),
//       Vn: Vn(19:16),
//       actual: Actual_VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_1,
//       baseline: VectorBinary3RegisterLookupOp,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), len(9:8), N(7)],
//       generated_baseline: VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_0,
//       len: len(9:8),
//       length: len + 1,
//       n: N:Vn,
//       pattern: 111100111d11nnnndddd10ccnpm0mmmm,
//       rule: VTBL_VTBX,
//       safety: [n + length  >
//               32 => UNPREDICTABLE],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterLookupOpTester_Case2_TestCase2) {
  VectorBinary3RegisterLookupOpTester_Case2 baseline_tester;
  NamedActual_VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_1_VTBL_VTBX actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11nnnndddd10ccnpm0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
