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


// A(17:16)=00 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV64_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV64,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_RGTesterCase0
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_RGTesterCase0(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_RGTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~0000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV32_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV32,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_RGTesterCase1
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_RGTesterCase1(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_RGTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~0001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000080) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV16_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV16,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_RGTesterCase2
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_RGTesterCase2(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_RGTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~0010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLS_111100111d11ss00dddd01000qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01000qm0mmmm,
//       rule: VCLS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase3
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase3(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLZ_111100111d11ss00dddd01001qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01001qm0mmmm,
//       rule: VCLZ,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase4
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase4(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000480) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCNT_111100111d11ss00dddd01010qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01010qm0mmmm,
//       rule: VCNT,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8TesterCase5
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8TesterCase5(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8TesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000500) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMVN_register_111100111d11ss00dddd01011qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01011qm0mmmm,
//       rule: VMVN_register,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8TesterCase6
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8TesterCase6(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8TesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1011x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000580) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQABS_111100111d11ss00dddd01110qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01110qm0mmmm,
//       rule: VQABS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase7
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase7(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1110x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000700) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQNEG_111100111d11ss00dddd01111qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01111qm0mmmm,
//       rule: VQNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase8
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase8(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1111x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000780) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=010xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VPADDL_111100111d11ss00dddd0010p1m0mmmm_case_0,
//       pattern: 111100111d11ss00dddd0010p1m0mmmm,
//       rule: VPADDL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase9
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase9(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~010xx
  if ((inst.Bits() & 0x00000700)  !=
          0x00000200) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=00 & B(10:6)=110xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VPADAL_111100111d11ss00dddd0110p1m0mmmm_case_0,
//       pattern: 111100111d11ss00dddd0110p1m0mmmm,
//       rule: VPADAL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase10
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase10(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~110xx
  if ((inst.Bits() & 0x00000700)  !=
          0x00000600) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f000qm0mmmm,
//       rule: VCGT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase11
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase11(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f001qm0mmmm,
//       rule: VCGE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase12
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase12(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000080) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f010qm0mmmm,
//       rule: VCEQ_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase13
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase13(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=0011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f011qm0mmmm,
//       rule: VCLE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase14
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase14(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0011x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000180) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=0100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f100qm0mmmm,
//       rule: VCLT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase15
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase15(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0100x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=0110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f110qm0mmmm,
//       rule: VABS_A1,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase16
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase16(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0110x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=0111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VNEG_111100111d11ss01dddd0f111qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f111qm0mmmm,
//       rule: VNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TesterCase17
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TesterCase17(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0111x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000380) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f000qm0mmmm,
//       rule: VCGT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase18
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase18(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f001qm0mmmm,
//       rule: VCGE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase19
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase19(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000480) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f010qm0mmmm,
//       rule: VCEQ_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase20
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase20(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000500) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f011qm0mmmm,
//       rule: VCLE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase21
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase21(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1011x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000580) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=1100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f100qm0mmmm,
//       rule: VCLT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase22
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase22(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase22
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1100x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f110qm0mmmm,
//       rule: VABS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase23
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase23(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase23
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1110x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000700) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=01 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VNEG_111100111d11ss01dddd0f111qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f111qm0mmmm,
//       rule: VNEG,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase24
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase24(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase24
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1111x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000780) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=01000
//    = {Vm: Vm(3:0),
//       actual: Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vm(3:0)],
//       generated_baseline: VMOVN_111100111d11ss10dddd001000m0mmmm_case_0,
//       pattern: 111100111d11ss10dddd001000m0mmmm,
//       rule: VMOVN,
//       safety: [size(19:18)=11 => UNDEFINED, Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V16_32_64NTesterCase25
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V16_32_64NTesterCase25(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V16_32_64NTesterCase25
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~01000
  if ((inst.Bits() & 0x000007C0)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=01001
//    = {Vm: Vm(3:0),
//       actual: Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       generated_baseline: VQMOVUN_111100111d11ss10dddd0010ppm0mmmm_case_0,
//       op: op(7:6),
//       pattern: 111100111d11ss10dddd0010ppm0mmmm,
//       rule: VQMOVUN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_I16_32_64NTesterCase26
    : public Vector2RegisterMiscellaneous_I16_32_64NTester {
 public:
  Vector2RegisterMiscellaneous_I16_32_64NTesterCase26(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneous_I16_32_64NTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_I16_32_64NTesterCase26
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~01001
  if ((inst.Bits() & 0x000007C0)  !=
          0x00000240) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneous_I16_32_64NTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=01100
//    = {Vd: Vd(15:12),
//       actual: Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12)],
//       generated_baseline: VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_0,
//       pattern: 111100111d11ss10dddd001100m0mmmm,
//       rule: VSHLL_A2,
//       safety: [size(19:18)=11 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_I8_16_32LTesterCase27
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_I8_16_32LTesterCase27(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_I8_16_32LTesterCase27
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~01100
  if ((inst.Bits() & 0x000007C0)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=11x00
//    = {Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_CVT_H2S,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8), Vm(3:0)],
//       generated_baseline: CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_0,
//       half_to_single: op(8)=1,
//       op: op(8),
//       pattern: 111100111d11ss10dddd011p00m0mmmm,
//       rule: CVT_between_half_precision_and_single_precision,
//       safety: [size(19:18)=~01 => UNDEFINED,
//         half_to_single &&
//            Vd(0)=1 => UNDEFINED,
//         not half_to_single &&
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_CVT_H2STesterCase28
    : public Vector2RegisterMiscellaneous_CVT_H2STester {
 public:
  Vector2RegisterMiscellaneous_CVT_H2STesterCase28(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneous_CVT_H2STester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_CVT_H2STesterCase28
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~11x00
  if ((inst.Bits() & 0x000006C0)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneous_CVT_H2STester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=0000x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8S,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VSWP_111100111d11ss10dddd00000qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00000qm0mmmm,
//       rule: VSWP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8STesterCase29
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8STesterCase29(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8STesterCase29
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=0001x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32T,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VTRN_111100111d11ss10dddd00001qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00001qm0mmmm,
//       rule: VTRN,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TTesterCase30
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TTesterCase30(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32TTesterCase30
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000080) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=0010x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VUZP_111100111d11ss10dddd00010qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00010qm0mmmm,
//       rule: VUZP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32ITesterCase31
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32ITesterCase31(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32ITesterCase31
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=0011x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VZIP_111100111d11ss10dddd00011qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00011qm0mmmm,
//       rule: VZIP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32ITesterCase32
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_V8_16_32ITesterCase32(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_V8_16_32ITesterCase32
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0011x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000180) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=10 & B(10:6)=0101x
//    = {Vm: Vm(3:0),
//       actual: Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       generated_baseline: VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_0,
//       op: op(7:6),
//       pattern: 111100111d11ss10dddd0010ppm0mmmm,
//       rule: VQMOVN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_I16_32_64NTesterCase33
    : public Vector2RegisterMiscellaneous_I16_32_64NTester {
 public:
  Vector2RegisterMiscellaneous_I16_32_64NTesterCase33(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneous_I16_32_64NTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_I16_32_64NTesterCase33
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0101x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000280) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneous_I16_32_64NTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=11 & B(10:6)=10x0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRECPE_111100111d11ss11dddd010f0qm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd010f0qm0mmmm,
//       rule: VRECPE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase34
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase34(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase34
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~11
  if ((inst.Bits() & 0x00030000)  !=
          0x00030000) return false;
  // B(10:6)=~10x0x
  if ((inst.Bits() & 0x00000680)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=11 & B(10:6)=10x1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSQRTE_111100111d11ss11dddd010f1qm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd010f1qm0mmmm,
//       rule: VRSQRTE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32TesterCase35
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_F32TesterCase35(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_F32TesterCase35
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~11
  if ((inst.Bits() & 0x00030000)  !=
          0x00030000) return false;
  // B(10:6)=~10x1x
  if ((inst.Bits() & 0x00000680)  !=
          0x00000480) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// A(17:16)=11 & B(10:6)=11xxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_CVT_F2I,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCVT_111100111d11ss11dddd011ppqm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd011ppqm0mmmm,
//       rule: VCVT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_CVT_F2ITesterCase36
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneous_CVT_F2ITesterCase36(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_CVT_F2ITesterCase36
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~11
  if ((inst.Bits() & 0x00030000)  !=
          0x00030000) return false;
  // B(10:6)=~11xxx
  if ((inst.Bits() & 0x00000600)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// A(17:16)=00 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV64_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV64,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_RGTester_Case0
    : public Vector2RegisterMiscellaneous_RGTesterCase0 {
 public:
  Vector2RegisterMiscellaneous_RGTester_Case0()
    : Vector2RegisterMiscellaneous_RGTesterCase0(
      state_.Vector2RegisterMiscellaneous_RG_VREV64_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV32_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV32,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_RGTester_Case1
    : public Vector2RegisterMiscellaneous_RGTesterCase1 {
 public:
  Vector2RegisterMiscellaneous_RGTester_Case1()
    : Vector2RegisterMiscellaneous_RGTesterCase1(
      state_.Vector2RegisterMiscellaneous_RG_VREV32_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV16_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV16,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_RGTester_Case2
    : public Vector2RegisterMiscellaneous_RGTesterCase2 {
 public:
  Vector2RegisterMiscellaneous_RGTester_Case2()
    : Vector2RegisterMiscellaneous_RGTesterCase2(
      state_.Vector2RegisterMiscellaneous_RG_VREV16_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLS_111100111d11ss00dddd01000qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01000qm0mmmm,
//       rule: VCLS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case3
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase3 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case3()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase3(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCLS_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLZ_111100111d11ss00dddd01001qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01001qm0mmmm,
//       rule: VCLZ,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case4
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase4 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case4()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase4(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCLZ_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCNT_111100111d11ss00dddd01010qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01010qm0mmmm,
//       rule: VCNT,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8Tester_Case5
    : public Vector2RegisterMiscellaneous_V8TesterCase5 {
 public:
  Vector2RegisterMiscellaneous_V8Tester_Case5()
    : Vector2RegisterMiscellaneous_V8TesterCase5(
      state_.Vector2RegisterMiscellaneous_V8_VCNT_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMVN_register_111100111d11ss00dddd01011qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01011qm0mmmm,
//       rule: VMVN_register,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8Tester_Case6
    : public Vector2RegisterMiscellaneous_V8TesterCase6 {
 public:
  Vector2RegisterMiscellaneous_V8Tester_Case6()
    : Vector2RegisterMiscellaneous_V8TesterCase6(
      state_.Vector2RegisterMiscellaneous_V8_VMVN_register_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQABS_111100111d11ss00dddd01110qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01110qm0mmmm,
//       rule: VQABS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case7
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase7 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case7()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase7(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VQABS_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQNEG_111100111d11ss00dddd01111qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01111qm0mmmm,
//       rule: VQNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case8
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase8 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case8()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase8(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VQNEG_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=010xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VPADDL_111100111d11ss00dddd0010p1m0mmmm_case_0,
//       pattern: 111100111d11ss00dddd0010p1m0mmmm,
//       rule: VPADDL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case9
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase9 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case9()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase9(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VPADDL_instance_)
  {}
};

// A(17:16)=00 & B(10:6)=110xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VPADAL_111100111d11ss00dddd0110p1m0mmmm_case_0,
//       pattern: 111100111d11ss00dddd0110p1m0mmmm,
//       rule: VPADAL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case10
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase10 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case10()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase10(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VPADAL_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f000qm0mmmm,
//       rule: VCGT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case11
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase11 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case11()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase11(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f001qm0mmmm,
//       rule: VCGE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case12
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase12 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case12()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase12(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f010qm0mmmm,
//       rule: VCEQ_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case13
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase13 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case13()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase13(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=0011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f011qm0mmmm,
//       rule: VCLE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case14
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase14 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case14()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase14(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=0100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f100qm0mmmm,
//       rule: VCLT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case15
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase15 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case15()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase15(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=0110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f110qm0mmmm,
//       rule: VABS_A1,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case16
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase16 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case16()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase16(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VABS_A1_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=0111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VNEG_111100111d11ss01dddd0f111qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f111qm0mmmm,
//       rule: VNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case17
    : public Vector2RegisterMiscellaneous_V8_16_32TesterCase17 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case17()
    : Vector2RegisterMiscellaneous_V8_16_32TesterCase17(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VNEG_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f000qm0mmmm,
//       rule: VCGT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case18
    : public Vector2RegisterMiscellaneous_F32TesterCase18 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case18()
    : Vector2RegisterMiscellaneous_F32TesterCase18(
      state_.Vector2RegisterMiscellaneous_F32_VCGT_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f001qm0mmmm,
//       rule: VCGE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case19
    : public Vector2RegisterMiscellaneous_F32TesterCase19 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case19()
    : Vector2RegisterMiscellaneous_F32TesterCase19(
      state_.Vector2RegisterMiscellaneous_F32_VCGE_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f010qm0mmmm,
//       rule: VCEQ_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case20
    : public Vector2RegisterMiscellaneous_F32TesterCase20 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case20()
    : Vector2RegisterMiscellaneous_F32TesterCase20(
      state_.Vector2RegisterMiscellaneous_F32_VCEQ_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f011qm0mmmm,
//       rule: VCLE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case21
    : public Vector2RegisterMiscellaneous_F32TesterCase21 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case21()
    : Vector2RegisterMiscellaneous_F32TesterCase21(
      state_.Vector2RegisterMiscellaneous_F32_VCLE_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=1100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f100qm0mmmm,
//       rule: VCLT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case22
    : public Vector2RegisterMiscellaneous_F32TesterCase22 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case22()
    : Vector2RegisterMiscellaneous_F32TesterCase22(
      state_.Vector2RegisterMiscellaneous_F32_VCLT_immediate_0_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f110qm0mmmm,
//       rule: VABS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case23
    : public Vector2RegisterMiscellaneous_F32TesterCase23 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case23()
    : Vector2RegisterMiscellaneous_F32TesterCase23(
      state_.Vector2RegisterMiscellaneous_F32_VABS_A1_instance_)
  {}
};

// A(17:16)=01 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VNEG_111100111d11ss01dddd0f111qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f111qm0mmmm,
//       rule: VNEG,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case24
    : public Vector2RegisterMiscellaneous_F32TesterCase24 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case24()
    : Vector2RegisterMiscellaneous_F32TesterCase24(
      state_.Vector2RegisterMiscellaneous_F32_VNEG_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=01000
//    = {Vm: Vm(3:0),
//       actual: Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vm(3:0)],
//       generated_baseline: VMOVN_111100111d11ss10dddd001000m0mmmm_case_0,
//       pattern: 111100111d11ss10dddd001000m0mmmm,
//       rule: VMOVN,
//       safety: [size(19:18)=11 => UNDEFINED, Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V16_32_64NTester_Case25
    : public Vector2RegisterMiscellaneous_V16_32_64NTesterCase25 {
 public:
  Vector2RegisterMiscellaneous_V16_32_64NTester_Case25()
    : Vector2RegisterMiscellaneous_V16_32_64NTesterCase25(
      state_.Vector2RegisterMiscellaneous_V16_32_64N_VMOVN_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=01001
//    = {Vm: Vm(3:0),
//       actual: Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       generated_baseline: VQMOVUN_111100111d11ss10dddd0010ppm0mmmm_case_0,
//       op: op(7:6),
//       pattern: 111100111d11ss10dddd0010ppm0mmmm,
//       rule: VQMOVUN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_I16_32_64NTester_Case26
    : public Vector2RegisterMiscellaneous_I16_32_64NTesterCase26 {
 public:
  Vector2RegisterMiscellaneous_I16_32_64NTester_Case26()
    : Vector2RegisterMiscellaneous_I16_32_64NTesterCase26(
      state_.Vector2RegisterMiscellaneous_I16_32_64N_VQMOVUN_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=01100
//    = {Vd: Vd(15:12),
//       actual: Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12)],
//       generated_baseline: VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_0,
//       pattern: 111100111d11ss10dddd001100m0mmmm,
//       rule: VSHLL_A2,
//       safety: [size(19:18)=11 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_I8_16_32LTester_Case27
    : public Vector2RegisterMiscellaneous_I8_16_32LTesterCase27 {
 public:
  Vector2RegisterMiscellaneous_I8_16_32LTester_Case27()
    : Vector2RegisterMiscellaneous_I8_16_32LTesterCase27(
      state_.Vector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=11x00
//    = {Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_CVT_H2S,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8), Vm(3:0)],
//       generated_baseline: CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_0,
//       half_to_single: op(8)=1,
//       op: op(8),
//       pattern: 111100111d11ss10dddd011p00m0mmmm,
//       rule: CVT_between_half_precision_and_single_precision,
//       safety: [size(19:18)=~01 => UNDEFINED,
//         half_to_single &&
//            Vd(0)=1 => UNDEFINED,
//         not half_to_single &&
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_CVT_H2STester_Case28
    : public Vector2RegisterMiscellaneous_CVT_H2STesterCase28 {
 public:
  Vector2RegisterMiscellaneous_CVT_H2STester_Case28()
    : Vector2RegisterMiscellaneous_CVT_H2STesterCase28(
      state_.Vector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=0000x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8S,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VSWP_111100111d11ss10dddd00000qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00000qm0mmmm,
//       rule: VSWP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8STester_Case29
    : public Vector2RegisterMiscellaneous_V8STesterCase29 {
 public:
  Vector2RegisterMiscellaneous_V8STester_Case29()
    : Vector2RegisterMiscellaneous_V8STesterCase29(
      state_.Vector2RegisterMiscellaneous_V8S_VSWP_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=0001x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32T,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VTRN_111100111d11ss10dddd00001qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00001qm0mmmm,
//       rule: VTRN,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32TTester_Case30
    : public Vector2RegisterMiscellaneous_V8_16_32TTesterCase30 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TTester_Case30()
    : Vector2RegisterMiscellaneous_V8_16_32TTesterCase30(
      state_.Vector2RegisterMiscellaneous_V8_16_32T_VTRN_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=0010x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VUZP_111100111d11ss10dddd00010qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00010qm0mmmm,
//       rule: VUZP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32ITester_Case31
    : public Vector2RegisterMiscellaneous_V8_16_32ITesterCase31 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32ITester_Case31()
    : Vector2RegisterMiscellaneous_V8_16_32ITesterCase31(
      state_.Vector2RegisterMiscellaneous_V8_16_32I_VUZP_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=0011x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VZIP_111100111d11ss10dddd00011qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00011qm0mmmm,
//       rule: VZIP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_V8_16_32ITester_Case32
    : public Vector2RegisterMiscellaneous_V8_16_32ITesterCase32 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32ITester_Case32()
    : Vector2RegisterMiscellaneous_V8_16_32ITesterCase32(
      state_.Vector2RegisterMiscellaneous_V8_16_32I_VZIP_instance_)
  {}
};

// A(17:16)=10 & B(10:6)=0101x
//    = {Vm: Vm(3:0),
//       actual: Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       generated_baseline: VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_0,
//       op: op(7:6),
//       pattern: 111100111d11ss10dddd0010ppm0mmmm,
//       rule: VQMOVN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_I16_32_64NTester_Case33
    : public Vector2RegisterMiscellaneous_I16_32_64NTesterCase33 {
 public:
  Vector2RegisterMiscellaneous_I16_32_64NTester_Case33()
    : Vector2RegisterMiscellaneous_I16_32_64NTesterCase33(
      state_.Vector2RegisterMiscellaneous_I16_32_64N_VQMOVN_instance_)
  {}
};

// A(17:16)=11 & B(10:6)=10x0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRECPE_111100111d11ss11dddd010f0qm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd010f0qm0mmmm,
//       rule: VRECPE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case34
    : public Vector2RegisterMiscellaneous_F32TesterCase34 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case34()
    : Vector2RegisterMiscellaneous_F32TesterCase34(
      state_.Vector2RegisterMiscellaneous_F32_VRECPE_instance_)
  {}
};

// A(17:16)=11 & B(10:6)=10x1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSQRTE_111100111d11ss11dddd010f1qm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd010f1qm0mmmm,
//       rule: VRSQRTE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_F32Tester_Case35
    : public Vector2RegisterMiscellaneous_F32TesterCase35 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case35()
    : Vector2RegisterMiscellaneous_F32TesterCase35(
      state_.Vector2RegisterMiscellaneous_F32_VRSQRTE_instance_)
  {}
};

// A(17:16)=11 & B(10:6)=11xxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_CVT_F2I,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCVT_111100111d11ss11dddd011ppqm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd011ppqm0mmmm,
//       rule: VCVT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
class Vector2RegisterMiscellaneous_CVT_F2ITester_Case36
    : public Vector2RegisterMiscellaneous_CVT_F2ITesterCase36 {
 public:
  Vector2RegisterMiscellaneous_CVT_F2ITester_Case36()
    : Vector2RegisterMiscellaneous_CVT_F2ITesterCase36(
      state_.Vector2RegisterMiscellaneous_CVT_F2I_VCVT_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// A(17:16)=00 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV64_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV64,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_RGTester_Case0_TestCase0) {
  Vector2RegisterMiscellaneous_RGTester_Case0 baseline_tester;
  NamedActual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1_VREV64 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd000ppqm0mmmm");
}

// A(17:16)=00 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV32_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV32,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_RGTester_Case1_TestCase1) {
  Vector2RegisterMiscellaneous_RGTester_Case1 baseline_tester;
  NamedActual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1_VREV32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd000ppqm0mmmm");
}

// A(17:16)=00 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       generated_baseline: VREV16_111100111d11ss00dddd000ppqm0mmmm_case_0,
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV16,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_RGTester_Case2_TestCase2) {
  Vector2RegisterMiscellaneous_RGTester_Case2 baseline_tester;
  NamedActual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1_VREV16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd000ppqm0mmmm");
}

// A(17:16)=00 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLS_111100111d11ss00dddd01000qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01000qm0mmmm,
//       rule: VCLS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case3_TestCase3) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case3 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VCLS actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd01000qm0mmmm");
}

// A(17:16)=00 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLZ_111100111d11ss00dddd01001qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01001qm0mmmm,
//       rule: VCLZ,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case4_TestCase4) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case4 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VCLZ actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd01001qm0mmmm");
}

// A(17:16)=00 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCNT_111100111d11ss00dddd01010qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01010qm0mmmm,
//       rule: VCNT,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8Tester_Case5_TestCase5) {
  Vector2RegisterMiscellaneous_V8Tester_Case5 baseline_tester;
  NamedActual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1_VCNT actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd01010qm0mmmm");
}

// A(17:16)=00 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMVN_register_111100111d11ss00dddd01011qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01011qm0mmmm,
//       rule: VMVN_register,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8Tester_Case6_TestCase6) {
  Vector2RegisterMiscellaneous_V8Tester_Case6 baseline_tester;
  NamedActual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1_VMVN_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd01011qm0mmmm");
}

// A(17:16)=00 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQABS_111100111d11ss00dddd01110qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01110qm0mmmm,
//       rule: VQABS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case7_TestCase7) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case7 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VQABS actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd01110qm0mmmm");
}

// A(17:16)=00 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQNEG_111100111d11ss00dddd01111qm0mmmm_case_0,
//       pattern: 111100111d11ss00dddd01111qm0mmmm,
//       rule: VQNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case8_TestCase8) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case8 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VQNEG actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd01111qm0mmmm");
}

// A(17:16)=00 & B(10:6)=010xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VPADDL_111100111d11ss00dddd0010p1m0mmmm_case_0,
//       pattern: 111100111d11ss00dddd0010p1m0mmmm,
//       rule: VPADDL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case9_TestCase9) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case9 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VPADDL actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd0010p1m0mmmm");
}

// A(17:16)=00 & B(10:6)=110xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VPADAL_111100111d11ss00dddd0110p1m0mmmm_case_0,
//       pattern: 111100111d11ss00dddd0110p1m0mmmm,
//       rule: VPADAL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case10_TestCase10) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case10 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VPADAL actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss00dddd0110p1m0mmmm");
}

// A(17:16)=01 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f000qm0mmmm,
//       rule: VCGT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case11_TestCase11) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case11 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VCGT_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f000qm0mmmm");
}

// A(17:16)=01 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f001qm0mmmm,
//       rule: VCGE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case12_TestCase12) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case12 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VCGE_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f001qm0mmmm");
}

// A(17:16)=01 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f010qm0mmmm,
//       rule: VCEQ_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case13_TestCase13) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case13 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VCEQ_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f010qm0mmmm");
}

// A(17:16)=01 & B(10:6)=0011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f011qm0mmmm,
//       rule: VCLE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case14_TestCase14) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case14 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VCLE_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f011qm0mmmm");
}

// A(17:16)=01 & B(10:6)=0100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f100qm0mmmm,
//       rule: VCLT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case15_TestCase15) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case15 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VCLT_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f100qm0mmmm");
}

// A(17:16)=01 & B(10:6)=0110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f110qm0mmmm,
//       rule: VABS_A1,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case16_TestCase16) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case16 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VABS_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f110qm0mmmm");
}

// A(17:16)=01 & B(10:6)=0111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VNEG_111100111d11ss01dddd0f111qm0mmmm_case_0,
//       pattern: 111100111d11ss01dddd0f111qm0mmmm,
//       rule: VNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case17_TestCase17) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case17 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1_VNEG actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f111qm0mmmm");
}

// A(17:16)=01 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f000qm0mmmm,
//       rule: VCGT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case18_TestCase18) {
  Vector2RegisterMiscellaneous_F32Tester_Case18 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VCGT_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f000qm0mmmm");
}

// A(17:16)=01 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f001qm0mmmm,
//       rule: VCGE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case19_TestCase19) {
  Vector2RegisterMiscellaneous_F32Tester_Case19 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VCGE_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f001qm0mmmm");
}

// A(17:16)=01 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f010qm0mmmm,
//       rule: VCEQ_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case20_TestCase20) {
  Vector2RegisterMiscellaneous_F32Tester_Case20 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VCEQ_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f010qm0mmmm");
}

// A(17:16)=01 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f011qm0mmmm,
//       rule: VCLE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case21_TestCase21) {
  Vector2RegisterMiscellaneous_F32Tester_Case21 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VCLE_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f011qm0mmmm");
}

// A(17:16)=01 & B(10:6)=1100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f100qm0mmmm,
//       rule: VCLT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case22_TestCase22) {
  Vector2RegisterMiscellaneous_F32Tester_Case22 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VCLT_immediate_0 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f100qm0mmmm");
}

// A(17:16)=01 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f110qm0mmmm,
//       rule: VABS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case23_TestCase23) {
  Vector2RegisterMiscellaneous_F32Tester_Case23 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VABS_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f110qm0mmmm");
}

// A(17:16)=01 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VNEG_111100111d11ss01dddd0f111qm0mmmm_case_1,
//       pattern: 111100111d11ss01dddd0f111qm0mmmm,
//       rule: VNEG,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case24_TestCase24) {
  Vector2RegisterMiscellaneous_F32Tester_Case24 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VNEG actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss01dddd0f111qm0mmmm");
}

// A(17:16)=10 & B(10:6)=01000
//    = {Vm: Vm(3:0),
//       actual: Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vm(3:0)],
//       generated_baseline: VMOVN_111100111d11ss10dddd001000m0mmmm_case_0,
//       pattern: 111100111d11ss10dddd001000m0mmmm,
//       rule: VMOVN,
//       safety: [size(19:18)=11 => UNDEFINED, Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V16_32_64NTester_Case25_TestCase25) {
  Vector2RegisterMiscellaneous_V16_32_64NTester_Case25 baseline_tester;
  NamedActual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1_VMOVN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd001000m0mmmm");
}

// A(17:16)=10 & B(10:6)=01001
//    = {Vm: Vm(3:0),
//       actual: Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       generated_baseline: VQMOVUN_111100111d11ss10dddd0010ppm0mmmm_case_0,
//       op: op(7:6),
//       pattern: 111100111d11ss10dddd0010ppm0mmmm,
//       rule: VQMOVUN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_I16_32_64NTester_Case26_TestCase26) {
  Vector2RegisterMiscellaneous_I16_32_64NTester_Case26 baseline_tester;
  NamedActual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1_VQMOVUN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd0010ppm0mmmm");
}

// A(17:16)=10 & B(10:6)=01100
//    = {Vd: Vd(15:12),
//       actual: Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12)],
//       generated_baseline: VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_0,
//       pattern: 111100111d11ss10dddd001100m0mmmm,
//       rule: VSHLL_A2,
//       safety: [size(19:18)=11 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_I8_16_32LTester_Case27_TestCase27) {
  Vector2RegisterMiscellaneous_I8_16_32LTester_Case27 baseline_tester;
  NamedActual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1_VSHLL_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd001100m0mmmm");
}

// A(17:16)=10 & B(10:6)=11x00
//    = {Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_CVT_H2S,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8), Vm(3:0)],
//       generated_baseline: CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_0,
//       half_to_single: op(8)=1,
//       op: op(8),
//       pattern: 111100111d11ss10dddd011p00m0mmmm,
//       rule: CVT_between_half_precision_and_single_precision,
//       safety: [size(19:18)=~01 => UNDEFINED,
//         half_to_single &&
//            Vd(0)=1 => UNDEFINED,
//         not half_to_single &&
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_CVT_H2STester_Case28_TestCase28) {
  Vector2RegisterMiscellaneous_CVT_H2STester_Case28 baseline_tester;
  NamedActual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1_CVT_between_half_precision_and_single_precision actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd011p00m0mmmm");
}

// A(17:16)=10 & B(10:6)=0000x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8S,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VSWP_111100111d11ss10dddd00000qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00000qm0mmmm,
//       rule: VSWP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8STester_Case29_TestCase29) {
  Vector2RegisterMiscellaneous_V8STester_Case29 baseline_tester;
  NamedActual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1_VSWP actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd00000qm0mmmm");
}

// A(17:16)=10 & B(10:6)=0001x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32T,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VTRN_111100111d11ss10dddd00001qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00001qm0mmmm,
//       rule: VTRN,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32TTester_Case30_TestCase30) {
  Vector2RegisterMiscellaneous_V8_16_32TTester_Case30 baseline_tester;
  NamedActual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1_VTRN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd00001qm0mmmm");
}

// A(17:16)=10 & B(10:6)=0010x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VUZP_111100111d11ss10dddd00010qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00010qm0mmmm,
//       rule: VUZP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32ITester_Case31_TestCase31) {
  Vector2RegisterMiscellaneous_V8_16_32ITester_Case31 baseline_tester;
  NamedActual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1_VUZP actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd00010qm0mmmm");
}

// A(17:16)=10 & B(10:6)=0011x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       generated_baseline: VZIP_111100111d11ss10dddd00011qm0mmmm_case_0,
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00011qm0mmmm,
//       rule: VZIP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32ITester_Case32_TestCase32) {
  Vector2RegisterMiscellaneous_V8_16_32ITester_Case32 baseline_tester;
  NamedActual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1_VZIP actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd00011qm0mmmm");
}

// A(17:16)=10 & B(10:6)=0101x
//    = {Vm: Vm(3:0),
//       actual: Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1,
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       generated_baseline: VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_0,
//       op: op(7:6),
//       pattern: 111100111d11ss10dddd0010ppm0mmmm,
//       rule: VQMOVN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_I16_32_64NTester_Case33_TestCase33) {
  Vector2RegisterMiscellaneous_I16_32_64NTester_Case33 baseline_tester;
  NamedActual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1_VQMOVN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss10dddd0010ppm0mmmm");
}

// A(17:16)=11 & B(10:6)=10x0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRECPE_111100111d11ss11dddd010f0qm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd010f0qm0mmmm,
//       rule: VRECPE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case34_TestCase34) {
  Vector2RegisterMiscellaneous_F32Tester_Case34 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VRECPE actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss11dddd010f0qm0mmmm");
}

// A(17:16)=11 & B(10:6)=10x1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSQRTE_111100111d11ss11dddd010f1qm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd010f1qm0mmmm,
//       rule: VRSQRTE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case35_TestCase35) {
  Vector2RegisterMiscellaneous_F32Tester_Case35 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VRSQRTE actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss11dddd010f1qm0mmmm");
}

// A(17:16)=11 & B(10:6)=11xxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//       baseline: Vector2RegisterMiscellaneous_CVT_F2I,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCVT_111100111d11ss11dddd011ppqm0mmmm_case_0,
//       pattern: 111100111d11ss11dddd011ppqm0mmmm,
//       rule: VCVT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_CVT_F2ITester_Case36_TestCase36) {
  Vector2RegisterMiscellaneous_CVT_F2ITester_Case36 baseline_tester;
  NamedActual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2_VCVT actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111d11ss11dddd011ppqm0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
