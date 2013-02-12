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


// opc2(19:16)=0000 & opc3(7:6)=01
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VMOV_register_cccc11101d110000dddd101s01m0mmmm_case_0,
//       pattern: cccc11101d110000dddd101s01m0mmmm,
//       rule: VMOV_register,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase0
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase0(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // opc3(7:6)=~01
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000040) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=0000 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VABS_cccc11101d110000dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110000dddd101s11m0mmmm,
//       rule: VABS,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // opc3(7:6)=~11
  if ((inst.Bits() & 0x000000C0)  !=
          0x000000C0) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=0001 & opc3(7:6)=01
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VNEG_cccc11101d110001dddd101s01m0mmmm_case_0,
//       pattern: cccc11101d110001dddd101s01m0mmmm,
//       rule: VNEG,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase2
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase2(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0001
  if ((inst.Bits() & 0x000F0000)  !=
          0x00010000) return false;
  // opc3(7:6)=~01
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000040) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=0001 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VSQRT_cccc11101d110001dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110001dddd101s11m0mmmm,
//       rule: VSQRT,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase3
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase3(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0001
  if ((inst.Bits() & 0x000F0000)  !=
          0x00010000) return false;
  // opc3(7:6)=~11
  if ((inst.Bits() & 0x000000C0)  !=
          0x000000C0) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=0100 & opc3(7:6)=x1
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCMP_VCMPE_cccc11101d110100dddd101se1m0mmmm_case_0,
//       pattern: cccc11101d110100dddd101se1m0mmmm,
//       rule: VCMP_VCMPE,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase4
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase4(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0100
  if ((inst.Bits() & 0x000F0000)  !=
          0x00040000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCMP_VCMPE_cccc11101d110101dddd101se1000000_case_0,
//       pattern: cccc11101d110101dddd101se1000000,
//       rule: VCMP_VCMPE,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase5
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase5(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0101
  if ((inst.Bits() & 0x000F0000)  !=
          0x00050000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
  if ((inst.Bits() & 0x0000002F)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=0111 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCVT_between_double_precision_and_single_precision_cccc11101d110111dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110111dddd101s11m0mmmm,
//       rule: VCVT_between_double_precision_and_single_precision,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase6
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase6(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0111
  if ((inst.Bits() & 0x000F0000)  !=
          0x00070000) return false;
  // opc3(7:6)=~11
  if ((inst.Bits() & 0x000000C0)  !=
          0x000000C0) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=1000 & opc3(7:6)=x1
//    = {actual: Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       fields: [opc2(18:16)],
//       generated_baseline: VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_0,
//       opc2: opc2(18:16),
//       pattern: cccc11101d111ooodddd101sp1m0mmmm,
//       rule: VCVT_VCVTR_between_floating_point_and_integer_Floating_point,
//       safety: [opc2(18:16)=~000 &&
//            opc2(18:16)=~10x => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTesterCase7
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase7(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~1000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00080000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: opc2(18:16)=~000 &&
  //       opc2(18:16)=~10x => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x00070000)  !=
          0x00000000) &&
       ((inst.Bits() & 0x00060000)  !=
          0x00040000)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCVTB_VCVTT_cccc11101d11001odddd1010t1m0mmmm_case_0,
//       pattern: cccc11101d11001odddd1010t1m0mmmm,
//       rule: VCVTB_VCVTT,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase8
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase8(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~001x
  if ((inst.Bits() & 0x000E0000)  !=
          0x00020000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=101x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1,
//       baseline: VcvtPtAndFixedPoint_FloatingPoint,
//       constraints: ,
//       defs: {},
//       fields: [sx(7), i(5), imm4(3:0)],
//       frac_bits: size - imm4:i,
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_0,
//       i: i(5),
//       imm4: imm4(3:0),
//       pattern: cccc11101d111o1udddd101fx1i0iiii,
//       rule: VCVT_between_floating_point_and_fixed_point_Floating_point,
//       safety: [frac_bits  <
//               0 => UNPREDICTABLE],
//       size: 16
//            if sx(7)=0
//            else 32,
//       sx: sx(7),
//       uses: {}}
class VcvtPtAndFixedPoint_FloatingPointTesterCase9
    : public CondVfpOpTester {
 public:
  VcvtPtAndFixedPoint_FloatingPointTesterCase9(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VcvtPtAndFixedPoint_FloatingPointTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~101x
  if ((inst.Bits() & 0x000E0000)  !=
          0x000A0000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool VcvtPtAndFixedPoint_FloatingPointTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: frac_bits  <
  //          0 => UNPREDICTABLE
  EXPECT_TRUE(((static_cast<int32_t>(((inst.Bits() & 0x00000080)  ==
          0x00000000
       ? 16
       : 32)) - static_cast<int32_t>(((((inst.Bits() & 0x0000000F)) << 1) | ((inst.Bits() & 0x00000020) >> 5)))) >= (0)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=110x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       fields: [opc2(18:16)],
//       generated_baseline: VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_0,
//       opc2: opc2(18:16),
//       pattern: cccc11101d111ooodddd101sp1m0mmmm,
//       rule: VCVT_VCVTR_between_floating_point_and_integer_Floating_point,
//       safety: [opc2(18:16)=~000 &&
//            opc2(18:16)=~10x => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTesterCase10
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase10(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~110x
  if ((inst.Bits() & 0x000E0000)  !=
          0x000C0000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: opc2(18:16)=~000 &&
  //       opc2(18:16)=~10x => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x00070000)  !=
          0x00000000) &&
       ((inst.Bits() & 0x00060000)  !=
          0x00040000)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc2(19:16)=111x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1,
//       baseline: VcvtPtAndFixedPoint_FloatingPoint,
//       constraints: ,
//       defs: {},
//       fields: [sx(7), i(5), imm4(3:0)],
//       frac_bits: size - imm4:i,
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_0,
//       i: i(5),
//       imm4: imm4(3:0),
//       pattern: cccc11101d111o1udddd101fx1i0iiii,
//       rule: VCVT_between_floating_point_and_fixed_point_Floating_point,
//       safety: [frac_bits  <
//               0 => UNPREDICTABLE],
//       size: 16
//            if sx(7)=0
//            else 32,
//       sx: sx(7),
//       uses: {}}
class VcvtPtAndFixedPoint_FloatingPointTesterCase11
    : public CondVfpOpTester {
 public:
  VcvtPtAndFixedPoint_FloatingPointTesterCase11(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VcvtPtAndFixedPoint_FloatingPointTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~111x
  if ((inst.Bits() & 0x000E0000)  !=
          0x000E0000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool VcvtPtAndFixedPoint_FloatingPointTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: frac_bits  <
  //          0 => UNPREDICTABLE
  EXPECT_TRUE(((static_cast<int32_t>(((inst.Bits() & 0x00000080)  ==
          0x00000000
       ? 16
       : 32)) - static_cast<int32_t>(((((inst.Bits() & 0x0000000F)) << 1) | ((inst.Bits() & 0x00000020) >> 5)))) >= (0)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VMOV_immediate_cccc11101d11iiiidddd101s0000iiii_case_0,
//       pattern: cccc11101d11iiiidddd101s0000iiii,
//       rule: VMOV_immediate,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTesterCase12
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase12(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc3(7:6)=~x0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
  if ((inst.Bits() & 0x000000A0)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// opc2(19:16)=0000 & opc3(7:6)=01
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VMOV_register_cccc11101d110000dddd101s01m0mmmm_case_0,
//       pattern: cccc11101d110000dddd101s01m0mmmm,
//       rule: VMOV_register,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case0
    : public CondVfpOpTesterCase0 {
 public:
  CondVfpOpTester_Case0()
    : CondVfpOpTesterCase0(
      state_.CondVfpOp_VMOV_register_instance_)
  {}
};

// opc2(19:16)=0000 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VABS_cccc11101d110000dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110000dddd101s11m0mmmm,
//       rule: VABS,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case1
    : public CondVfpOpTesterCase1 {
 public:
  CondVfpOpTester_Case1()
    : CondVfpOpTesterCase1(
      state_.CondVfpOp_VABS_instance_)
  {}
};

// opc2(19:16)=0001 & opc3(7:6)=01
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VNEG_cccc11101d110001dddd101s01m0mmmm_case_0,
//       pattern: cccc11101d110001dddd101s01m0mmmm,
//       rule: VNEG,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case2
    : public CondVfpOpTesterCase2 {
 public:
  CondVfpOpTester_Case2()
    : CondVfpOpTesterCase2(
      state_.CondVfpOp_VNEG_instance_)
  {}
};

// opc2(19:16)=0001 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VSQRT_cccc11101d110001dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110001dddd101s11m0mmmm,
//       rule: VSQRT,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case3
    : public CondVfpOpTesterCase3 {
 public:
  CondVfpOpTester_Case3()
    : CondVfpOpTesterCase3(
      state_.CondVfpOp_VSQRT_instance_)
  {}
};

// opc2(19:16)=0100 & opc3(7:6)=x1
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCMP_VCMPE_cccc11101d110100dddd101se1m0mmmm_case_0,
//       pattern: cccc11101d110100dddd101se1m0mmmm,
//       rule: VCMP_VCMPE,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case4
    : public CondVfpOpTesterCase4 {
 public:
  CondVfpOpTester_Case4()
    : CondVfpOpTesterCase4(
      state_.CondVfpOp_VCMP_VCMPE_instance_)
  {}
};

// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCMP_VCMPE_cccc11101d110101dddd101se1000000_case_0,
//       pattern: cccc11101d110101dddd101se1000000,
//       rule: VCMP_VCMPE,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case5
    : public CondVfpOpTesterCase5 {
 public:
  CondVfpOpTester_Case5()
    : CondVfpOpTesterCase5(
      state_.CondVfpOp_VCMP_VCMPE_instance_)
  {}
};

// opc2(19:16)=0111 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCVT_between_double_precision_and_single_precision_cccc11101d110111dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110111dddd101s11m0mmmm,
//       rule: VCVT_between_double_precision_and_single_precision,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case6
    : public CondVfpOpTesterCase6 {
 public:
  CondVfpOpTester_Case6()
    : CondVfpOpTesterCase6(
      state_.CondVfpOp_VCVT_between_double_precision_and_single_precision_instance_)
  {}
};

// opc2(19:16)=1000 & opc3(7:6)=x1
//    = {actual: Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       fields: [opc2(18:16)],
//       generated_baseline: VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_0,
//       opc2: opc2(18:16),
//       pattern: cccc11101d111ooodddd101sp1m0mmmm,
//       rule: VCVT_VCVTR_between_floating_point_and_integer_Floating_point,
//       safety: [opc2(18:16)=~000 &&
//            opc2(18:16)=~10x => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case7
    : public CondVfpOpTesterCase7 {
 public:
  CondVfpOpTester_Case7()
    : CondVfpOpTesterCase7(
      state_.CondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_instance_)
  {}
};

// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCVTB_VCVTT_cccc11101d11001odddd1010t1m0mmmm_case_0,
//       pattern: cccc11101d11001odddd1010t1m0mmmm,
//       rule: VCVTB_VCVTT,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case8
    : public CondVfpOpTesterCase8 {
 public:
  CondVfpOpTester_Case8()
    : CondVfpOpTesterCase8(
      state_.CondVfpOp_VCVTB_VCVTT_instance_)
  {}
};

// opc2(19:16)=101x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1,
//       baseline: VcvtPtAndFixedPoint_FloatingPoint,
//       constraints: ,
//       defs: {},
//       fields: [sx(7), i(5), imm4(3:0)],
//       frac_bits: size - imm4:i,
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_0,
//       i: i(5),
//       imm4: imm4(3:0),
//       pattern: cccc11101d111o1udddd101fx1i0iiii,
//       rule: VCVT_between_floating_point_and_fixed_point_Floating_point,
//       safety: [frac_bits  <
//               0 => UNPREDICTABLE],
//       size: 16
//            if sx(7)=0
//            else 32,
//       sx: sx(7),
//       uses: {}}
class VcvtPtAndFixedPoint_FloatingPointTester_Case9
    : public VcvtPtAndFixedPoint_FloatingPointTesterCase9 {
 public:
  VcvtPtAndFixedPoint_FloatingPointTester_Case9()
    : VcvtPtAndFixedPoint_FloatingPointTesterCase9(
      state_.VcvtPtAndFixedPoint_FloatingPoint_VCVT_between_floating_point_and_fixed_point_Floating_point_instance_)
  {}
};

// opc2(19:16)=110x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       fields: [opc2(18:16)],
//       generated_baseline: VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_0,
//       opc2: opc2(18:16),
//       pattern: cccc11101d111ooodddd101sp1m0mmmm,
//       rule: VCVT_VCVTR_between_floating_point_and_integer_Floating_point,
//       safety: [opc2(18:16)=~000 &&
//            opc2(18:16)=~10x => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case10
    : public CondVfpOpTesterCase10 {
 public:
  CondVfpOpTester_Case10()
    : CondVfpOpTesterCase10(
      state_.CondVfpOp_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_instance_)
  {}
};

// opc2(19:16)=111x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1,
//       baseline: VcvtPtAndFixedPoint_FloatingPoint,
//       constraints: ,
//       defs: {},
//       fields: [sx(7), i(5), imm4(3:0)],
//       frac_bits: size - imm4:i,
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_0,
//       i: i(5),
//       imm4: imm4(3:0),
//       pattern: cccc11101d111o1udddd101fx1i0iiii,
//       rule: VCVT_between_floating_point_and_fixed_point_Floating_point,
//       safety: [frac_bits  <
//               0 => UNPREDICTABLE],
//       size: 16
//            if sx(7)=0
//            else 32,
//       sx: sx(7),
//       uses: {}}
class VcvtPtAndFixedPoint_FloatingPointTester_Case11
    : public VcvtPtAndFixedPoint_FloatingPointTesterCase11 {
 public:
  VcvtPtAndFixedPoint_FloatingPointTester_Case11()
    : VcvtPtAndFixedPoint_FloatingPointTesterCase11(
      state_.VcvtPtAndFixedPoint_FloatingPoint_VCVT_between_floating_point_and_fixed_point_Floating_point_instance_)
  {}
};

// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VMOV_immediate_cccc11101d11iiiidddd101s0000iiii_case_0,
//       pattern: cccc11101d11iiiidddd101s0000iiii,
//       rule: VMOV_immediate,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
class CondVfpOpTester_Case12
    : public CondVfpOpTesterCase12 {
 public:
  CondVfpOpTester_Case12()
    : CondVfpOpTesterCase12(
      state_.CondVfpOp_VMOV_immediate_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// opc2(19:16)=0000 & opc3(7:6)=01
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VMOV_register_cccc11101d110000dddd101s01m0mmmm_case_0,
//       pattern: cccc11101d110000dddd101s01m0mmmm,
//       rule: VMOV_register,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case0_TestCase0) {
  CondVfpOpTester_Case0 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s01m0mmmm");
}

// opc2(19:16)=0000 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VABS_cccc11101d110000dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110000dddd101s11m0mmmm,
//       rule: VABS,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case1_TestCase1) {
  CondVfpOpTester_Case1 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VABS actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s11m0mmmm");
}

// opc2(19:16)=0001 & opc3(7:6)=01
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VNEG_cccc11101d110001dddd101s01m0mmmm_case_0,
//       pattern: cccc11101d110001dddd101s01m0mmmm,
//       rule: VNEG,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case2_TestCase2) {
  CondVfpOpTester_Case2 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VNEG actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s01m0mmmm");
}

// opc2(19:16)=0001 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VSQRT_cccc11101d110001dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110001dddd101s11m0mmmm,
//       rule: VSQRT,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case3_TestCase3) {
  CondVfpOpTester_Case3 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VSQRT actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s11m0mmmm");
}

// opc2(19:16)=0100 & opc3(7:6)=x1
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCMP_VCMPE_cccc11101d110100dddd101se1m0mmmm_case_0,
//       pattern: cccc11101d110100dddd101se1m0mmmm,
//       rule: VCMP_VCMPE,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case4_TestCase4) {
  CondVfpOpTester_Case4 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCMP_VCMPE actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110100dddd101se1m0mmmm");
}

// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCMP_VCMPE_cccc11101d110101dddd101se1000000_case_0,
//       pattern: cccc11101d110101dddd101se1000000,
//       rule: VCMP_VCMPE,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case5_TestCase5) {
  CondVfpOpTester_Case5 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCMP_VCMPE actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110101dddd101se1000000");
}

// opc2(19:16)=0111 & opc3(7:6)=11
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCVT_between_double_precision_and_single_precision_cccc11101d110111dddd101s11m0mmmm_case_0,
//       pattern: cccc11101d110111dddd101s11m0mmmm,
//       rule: VCVT_between_double_precision_and_single_precision,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case6_TestCase6) {
  CondVfpOpTester_Case6 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVT_between_double_precision_and_single_precision actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110111dddd101s11m0mmmm");
}

// opc2(19:16)=1000 & opc3(7:6)=x1
//    = {actual: Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       fields: [opc2(18:16)],
//       generated_baseline: VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_0,
//       opc2: opc2(18:16),
//       pattern: cccc11101d111ooodddd101sp1m0mmmm,
//       rule: VCVT_VCVTR_between_floating_point_and_integer_Floating_point,
//       safety: [opc2(18:16)=~000 &&
//            opc2(18:16)=~10x => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case7_TestCase7) {
  CondVfpOpTester_Case7 baseline_tester;
  NamedActual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1_VCVT_VCVTR_between_floating_point_and_integer_Floating_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d111ooodddd101sp1m0mmmm");
}

// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VCVTB_VCVTT_cccc11101d11001odddd1010t1m0mmmm_case_0,
//       pattern: cccc11101d11001odddd1010t1m0mmmm,
//       rule: VCVTB_VCVTT,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case8_TestCase8) {
  CondVfpOpTester_Case8 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VCVTB_VCVTT actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11001odddd1010t1m0mmmm");
}

// opc2(19:16)=101x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1,
//       baseline: VcvtPtAndFixedPoint_FloatingPoint,
//       constraints: ,
//       defs: {},
//       fields: [sx(7), i(5), imm4(3:0)],
//       frac_bits: size - imm4:i,
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_0,
//       i: i(5),
//       imm4: imm4(3:0),
//       pattern: cccc11101d111o1udddd101fx1i0iiii,
//       rule: VCVT_between_floating_point_and_fixed_point_Floating_point,
//       safety: [frac_bits  <
//               0 => UNPREDICTABLE],
//       size: 16
//            if sx(7)=0
//            else 32,
//       sx: sx(7),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VcvtPtAndFixedPoint_FloatingPointTester_Case9_TestCase9) {
  VcvtPtAndFixedPoint_FloatingPointTester_Case9 baseline_tester;
  NamedActual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1_VCVT_between_floating_point_and_fixed_point_Floating_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d111o1udddd101fx1i0iiii");
}

// opc2(19:16)=110x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       fields: [opc2(18:16)],
//       generated_baseline: VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_0,
//       opc2: opc2(18:16),
//       pattern: cccc11101d111ooodddd101sp1m0mmmm,
//       rule: VCVT_VCVTR_between_floating_point_and_integer_Floating_point,
//       safety: [opc2(18:16)=~000 &&
//            opc2(18:16)=~10x => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case10_TestCase10) {
  CondVfpOpTester_Case10 baseline_tester;
  NamedActual_VCVT_VCVTR_between_floating_point_and_integer_Floating_point_cccc11101d111ooodddd101sp1m0mmmm_case_1_VCVT_VCVTR_between_floating_point_and_integer_Floating_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d111ooodddd101sp1m0mmmm");
}

// opc2(19:16)=111x & opc3(7:6)=x1
//    = {actual: Actual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1,
//       baseline: VcvtPtAndFixedPoint_FloatingPoint,
//       constraints: ,
//       defs: {},
//       fields: [sx(7), i(5), imm4(3:0)],
//       frac_bits: size - imm4:i,
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_0,
//       i: i(5),
//       imm4: imm4(3:0),
//       pattern: cccc11101d111o1udddd101fx1i0iiii,
//       rule: VCVT_between_floating_point_and_fixed_point_Floating_point,
//       safety: [frac_bits  <
//               0 => UNPREDICTABLE],
//       size: 16
//            if sx(7)=0
//            else 32,
//       sx: sx(7),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VcvtPtAndFixedPoint_FloatingPointTester_Case11_TestCase11) {
  VcvtPtAndFixedPoint_FloatingPointTester_Case11 baseline_tester;
  NamedActual_VCVT_between_floating_point_and_fixed_point_Floating_point_cccc11101d111o1udddd101fx1i0iiii_case_1_VCVT_between_floating_point_and_fixed_point_Floating_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d111o1udddd101fx1i0iiii");
}

// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//       baseline: CondVfpOp,
//       constraints: ,
//       defs: {},
//       generated_baseline: VMOV_immediate_cccc11101d11iiiidddd101s0000iiii_case_0,
//       pattern: cccc11101d11iiiidddd101s0000iiii,
//       rule: VMOV_immediate,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case12_TestCase12) {
  CondVfpOpTester_Case12 baseline_tester;
  NamedActual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1_VMOV_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11iiiidddd101s0000iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
