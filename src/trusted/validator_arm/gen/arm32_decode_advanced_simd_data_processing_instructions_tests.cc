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
//    = {actual: VectorBinary3RegisterImmOp,
//       baseline: VectorBinary3RegisterImmOp,
//       constraints: }
class VectorBinary3RegisterImmOpTesterCase0
    : public VectorBinary3RegisterImmOpTester {
 public:
  VectorBinary3RegisterImmOpTesterCase0(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterImmOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
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

// U(24)=1 & A(23:19)=1x11x & B(11:8)=1100 & C(7:4)=0xx0
//    = {actual: VectorUnary2RegisterDup,
//       baseline: VectorUnary2RegisterDup,
//       constraints: }
class VectorUnary2RegisterDupTesterCase1
    : public VectorUnary2RegisterDupTester {
 public:
  VectorUnary2RegisterDupTesterCase1(const NamedClassDecoder& decoder)
    : VectorUnary2RegisterDupTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
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

// U(24)=1 & A(23:19)=1x11x & B(11:8)=10xx & C(7:4)=xxx0
//    = {actual: VectorBinary3RegisterLookupOp,
//       baseline: VectorBinary3RegisterLookupOp,
//       constraints: }
class VectorBinary3RegisterLookupOpTesterCase2
    : public VectorBinary3RegisterLookupOpTester {
 public:
  VectorBinary3RegisterLookupOpTesterCase2(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterLookupOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
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

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// U(24)=0 & A(23:19)=1x11x & C(7:4)=xxx0
//    = {actual: VectorBinary3RegisterImmOp,
//       baseline: VectorBinary3RegisterImmOp,
//       constraints: ,
//       rule: Vext_Rule_305_A1_P598}
class VectorBinary3RegisterImmOpTester_Case0
    : public VectorBinary3RegisterImmOpTesterCase0 {
 public:
  VectorBinary3RegisterImmOpTester_Case0()
    : VectorBinary3RegisterImmOpTesterCase0(
      state_.VectorBinary3RegisterImmOp_Vext_Rule_305_A1_P598_instance_)
  {}
};

// U(24)=1 & A(23:19)=1x11x & B(11:8)=1100 & C(7:4)=0xx0
//    = {actual: VectorUnary2RegisterDup,
//       baseline: VectorUnary2RegisterDup,
//       constraints: ,
//       rule: Vdup_Rule_302_A1_P592}
class VectorUnary2RegisterDupTester_Case1
    : public VectorUnary2RegisterDupTesterCase1 {
 public:
  VectorUnary2RegisterDupTester_Case1()
    : VectorUnary2RegisterDupTesterCase1(
      state_.VectorUnary2RegisterDup_Vdup_Rule_302_A1_P592_instance_)
  {}
};

// U(24)=1 & A(23:19)=1x11x & B(11:8)=10xx & C(7:4)=xxx0
//    = {actual: VectorBinary3RegisterLookupOp,
//       baseline: VectorBinary3RegisterLookupOp,
//       constraints: ,
//       rule: Vtbl_Vtbx_Rule_406_A1_P798}
class VectorBinary3RegisterLookupOpTester_Case2
    : public VectorBinary3RegisterLookupOpTesterCase2 {
 public:
  VectorBinary3RegisterLookupOpTester_Case2()
    : VectorBinary3RegisterLookupOpTesterCase2(
      state_.VectorBinary3RegisterLookupOp_Vtbl_Vtbx_Rule_406_A1_P798_instance_)
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
//    = {actual: VectorBinary3RegisterImmOp,
//       baseline: VectorBinary3RegisterImmOp,
//       constraints: ,
//       pattern: 111100101d11nnnnddddiiiinqm0mmmm,
//       rule: Vext_Rule_305_A1_P598}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterImmOpTester_Case0_TestCase0) {
  VectorBinary3RegisterImmOpTester_Case0 tester;
  tester.Test("111100101d11nnnnddddiiiinqm0mmmm");
}

// U(24)=1 & A(23:19)=1x11x & B(11:8)=1100 & C(7:4)=0xx0
//    = {actual: VectorUnary2RegisterDup,
//       baseline: VectorUnary2RegisterDup,
//       constraints: ,
//       pattern: 111100111d11iiiidddd11000qm0mmmm,
//       rule: Vdup_Rule_302_A1_P592}
TEST_F(Arm32DecoderStateTests,
       VectorUnary2RegisterDupTester_Case1_TestCase1) {
  VectorUnary2RegisterDupTester_Case1 tester;
  tester.Test("111100111d11iiiidddd11000qm0mmmm");
}

// U(24)=1 & A(23:19)=1x11x & B(11:8)=10xx & C(7:4)=xxx0
//    = {actual: VectorBinary3RegisterLookupOp,
//       baseline: VectorBinary3RegisterLookupOp,
//       constraints: ,
//       pattern: 111100111d11nnnndddd10ccnpm0mmmm,
//       rule: Vtbl_Vtbx_Rule_406_A1_P798}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterLookupOpTester_Case2_TestCase2) {
  VectorBinary3RegisterLookupOpTester_Case2 tester;
  tester.Test("111100111d11nnnndddd10ccnpm0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
