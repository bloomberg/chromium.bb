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


// A(11:8)=0000 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VHADD_1111001u0dssnnnndddd0000nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0000nqm0mmmm,
//       rule: VHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase0
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase0(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0000 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQADD_1111001u0dssnnnndddd0000nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0000nqm1mmmm,
//       rule: VQADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase1
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase1(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000000) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRHADD_1111001u0dssnnnndddd0001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0001nqm0mmmm,
//       rule: VRHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase2
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase2(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VAND_register_111100100d00nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d00nnnndddd0001nqm1mmmm,
//       rule: VAND_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase3
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase3(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~00
  if ((inst.Bits() & 0x00300000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIC_register_111100100d01nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d01nnnndddd0001nqm1mmmm,
//       rule: VBIC_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase4
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase4(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~01
  if ((inst.Bits() & 0x00300000)  !=
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VORR_register_or_VMOV_register_A1_111100100d10nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d10nnnndddd0001nqm1mmmm,
//       rule: VORR_register_or_VMOV_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase5
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase5(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~10
  if ((inst.Bits() & 0x00300000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VORN_register_111100100d11nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d11nnnndddd0001nqm1mmmm,
//       rule: VORN_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase6
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase6(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~11
  if ((inst.Bits() & 0x00300000)  !=
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VEOR_111100110d00nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d00nnnndddd0001nqm1mmmm,
//       rule: VEOR,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase7
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase7(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~00
  if ((inst.Bits() & 0x00300000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBSL_111100110d01nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d01nnnndddd0001nqm1mmmm,
//       rule: VBSL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase8
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase8(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~01
  if ((inst.Bits() & 0x00300000)  !=
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIT_111100110d10nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d10nnnndddd0001nqm1mmmm,
//       rule: VBIT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase9
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase9(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~10
  if ((inst.Bits() & 0x00300000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIF_111100110d11nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d11nnnndddd0001nqm1mmmm,
//       rule: VBIF,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase10
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase10(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~11
  if ((inst.Bits() & 0x00300000)  !=
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0010 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VHSUB_1111001u0dssnnnndddd0010nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0010nqm0mmmm,
//       rule: VHSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase11
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase11(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0010 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQSUB_1111001u0dssnnnndddd0010nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0010nqm1mmmm,
//       rule: VQSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase12
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase12(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0011 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_register_A1_1111001u0dssnnnndddd0011nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0011nqm0mmmm,
//       rule: VCGT_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase13
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase13(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0011 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_register_A1_1111001u0dssnnnndddd0011nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0011nqm1mmmm,
//       rule: VCGE_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase14
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase14(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0100 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSHL_register_1111001u0dssnnnndddd0100nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0100nqm0mmmm,
//       rule: VSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase15
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase15(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000400) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0100 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQSHL_register_1111001u0dssnnnndddd0100nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0100nqm1mmmm,
//       rule: VQSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase16
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase16(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000400) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0101 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSHL_1111001u0dssnnnndddd0101nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0101nqm0mmmm,
//       rule: VRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase17
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase17(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000500) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0101 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQRSHL_1111001u0dssnnnndddd0101nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0101nqm1mmmm,
//       rule: VQRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase18
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase18(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000500) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0110 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMAX_1111001u0dssnnnndddd0110nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0110nqm0mmmm,
//       rule: VMAX,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase19
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase19(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000600) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0110 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMIN_1111001u0dssnnnndddd0110nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0110nqm1mmmm,
//       rule: VMIN,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase20
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase20(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000600) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0111 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABD_1111001u0dssnnnndddd0111nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0111nqm0mmmm,
//       rule: VABD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase21
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase21(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000700) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0111 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0111nqm1mmmm,
//       rule: VABA,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase22
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase22(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase22
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000700) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1000 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1000nqm0mmmm,
//       rule: VADD_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase23
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase23(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase23
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1000 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSUB_integer_111100110dssnnnndddd1000nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1000nqm0mmmm,
//       rule: VSUB_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTesterCase24
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQTesterCase24(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQTesterCase24
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1000 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VTST_111100100dssnnnndddd1000nqm1mmmm_case_0,
//       pattern: 111100100dssnnnndddd1000nqm1mmmm,
//       rule: VTST,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase25
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase25(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase25
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1000 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_register_A1_111100110dssnnnndddd1000nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1000nqm1mmmm,
//       rule: VCEQ_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase26
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase26(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase26
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1001 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLA_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//       rule: VMLA_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase27
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase27(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase27
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1001 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLS_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//       rule: VMLS_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase28
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase28(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase28
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1001 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_integer_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//       rule: VMUL_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32TesterCase29
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32TesterCase29(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8_16_32TesterCase29
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1001 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//       rule: VMUL_polynomial_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=~00 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8PTesterCase30
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI8PTesterCase30(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI8PTesterCase30
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1010 & B(4)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMAX_1111001u0dssnnnndddd1010n0m0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1010n0m0mmmm,
//       rule: VPMAX,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDITesterCase31
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDITesterCase31(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDITesterCase31
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1010 & B(4)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMIN_1111001u0dssnnnndddd1010n0m1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1010n0m1mmmm,
//       rule: VPMIN,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDITesterCase32
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDITesterCase32(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDITesterCase32
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1011 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1011nqm0mmmm,
//       rule: VQDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI16_32TesterCase33
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI16_32TesterCase33(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI16_32TesterCase33
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1011 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQRDMULH_A1_111100110dssnnnndddd1011nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1011nqm0mmmm,
//       rule: VQRDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI16_32TesterCase34
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDQI16_32TesterCase34(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDQI16_32TesterCase34
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1011 & B(4)=1 & U(24)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_0,
//       pattern: 111100100dssnnnndddd1011n0m1mmmm,
//       rule: VPADD_integer,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDITesterCase35
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthDITesterCase35(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthDITesterCase35
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=0x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VFMA_A1_111100100d00nnnndddd1100nqm1mmmm_case_0,
//       pattern: 111100100d00nnnndddd1100nqm1mmmm,
//       rule: VFMA_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase36
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase36(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase36
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000C00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x00100000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=1x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VFMS_A1_111100100d10nnnndddd1100nqm1mmmm_case_0,
//       pattern: 111100100d10nnnndddd1100nqm1mmmm,
//       rule: VFMS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase37
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase37(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase37
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000C00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x00100000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VADD_floating_point_A1_111100100d0snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100100d0snnnndddd1101nqm0mmmm,
//       rule: VADD_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase38
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase38(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase38
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSUB_floating_point_A1_111100100d1snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100100d1snnnndddd1101nqm0mmmm,
//       rule: VSUB_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase39
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase39(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase39
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100110d0snnnndddd1101nqm0mmmm,
//       rule: VPADD_floating_point,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32PTesterCase40
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32PTesterCase40(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32PTesterCase40
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100110d1snnnndddd1101nqm0mmmm,
//       rule: VABD_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase41
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase41(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase41
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLA_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//       rule: VMLA_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase42
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase42(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase42
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLS_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//       rule: VMLS_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase43
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase43(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase43
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1101 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_floating_point_A1_111100110d0snnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100110d0snnnndddd1101nqm1mmmm,
//       rule: VMUL_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase44
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase44(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase44
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1110 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_register_A2_111100100d0snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100100d0snnnndddd1110nqm0mmmm,
//       rule: VCEQ_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase45
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase45(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase45
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_register_A2_111100110d0snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100110d0snnnndddd1110nqm0mmmm,
//       rule: VCGE_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase46
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase46(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase46
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_register_A2_111100110d1snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100110d1snnnndddd1110nqm0mmmm,
//       rule: VCGT_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase47
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase47(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase47
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VACGE_111100110dssnnnndddd1110nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1110nqm1mmmm,
//       rule: VACGE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase48
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase48(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase48
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VACGT_111100110dssnnnndddd1110nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1110nqm1mmmm,
//       rule: VACGT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase49
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase49(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase49
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMAX_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1111nqm0mmmm,
//       rule: VMAX_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase50
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase50(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase50
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMIN_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1111nqm0mmmm,
//       rule: VMIN_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase51
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase51(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase51
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMAX_111100110dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1111nqm0mmmm,
//       rule: VPMAX,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32PTesterCase52
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32PTesterCase52(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32PTesterCase52
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMIN_111100110dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1111nqm0mmmm,
//       rule: VPMIN,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32PTesterCase53
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32PTesterCase53(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32PTesterCase53
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRECPS_111100100d0snnnndddd1111nqm1mmmm_case_0,
//       pattern: 111100100d0snnnndddd1111nqm1mmmm,
//       rule: VRECPS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase54
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase54(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase54
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSQRTS_111100100d1snnnndddd1111nqm1mmmm_case_0,
//       pattern: 111100100d1snnnndddd1111nqm1mmmm,
//       rule: VRSQRTS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTesterCase55
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLength32_DQTesterCase55(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLength32_DQTesterCase55
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// A(11:8)=0000 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VHADD_1111001u0dssnnnndddd0000nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0000nqm0mmmm,
//       rule: VHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case0
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase0 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case0()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase0(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VHADD_instance_)
  {}
};

// A(11:8)=0000 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQADD_1111001u0dssnnnndddd0000nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0000nqm1mmmm,
//       rule: VQADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case1
    : public VectorBinary3RegisterSameLengthDQTesterCase1 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case1()
    : VectorBinary3RegisterSameLengthDQTesterCase1(
      state_.VectorBinary3RegisterSameLengthDQ_VQADD_instance_)
  {}
};

// A(11:8)=0001 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRHADD_1111001u0dssnnnndddd0001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0001nqm0mmmm,
//       rule: VRHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case2
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase2 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case2()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase2(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VRHADD_instance_)
  {}
};

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VAND_register_111100100d00nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d00nnnndddd0001nqm1mmmm,
//       rule: VAND_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case3
    : public VectorBinary3RegisterSameLengthDQTesterCase3 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case3()
    : VectorBinary3RegisterSameLengthDQTesterCase3(
      state_.VectorBinary3RegisterSameLengthDQ_VAND_register_instance_)
  {}
};

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIC_register_111100100d01nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d01nnnndddd0001nqm1mmmm,
//       rule: VBIC_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case4
    : public VectorBinary3RegisterSameLengthDQTesterCase4 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case4()
    : VectorBinary3RegisterSameLengthDQTesterCase4(
      state_.VectorBinary3RegisterSameLengthDQ_VBIC_register_instance_)
  {}
};

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VORR_register_or_VMOV_register_A1_111100100d10nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d10nnnndddd0001nqm1mmmm,
//       rule: VORR_register_or_VMOV_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case5
    : public VectorBinary3RegisterSameLengthDQTesterCase5 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case5()
    : VectorBinary3RegisterSameLengthDQTesterCase5(
      state_.VectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1_instance_)
  {}
};

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VORN_register_111100100d11nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d11nnnndddd0001nqm1mmmm,
//       rule: VORN_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case6
    : public VectorBinary3RegisterSameLengthDQTesterCase6 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case6()
    : VectorBinary3RegisterSameLengthDQTesterCase6(
      state_.VectorBinary3RegisterSameLengthDQ_VORN_register_instance_)
  {}
};

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VEOR_111100110d00nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d00nnnndddd0001nqm1mmmm,
//       rule: VEOR,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case7
    : public VectorBinary3RegisterSameLengthDQTesterCase7 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case7()
    : VectorBinary3RegisterSameLengthDQTesterCase7(
      state_.VectorBinary3RegisterSameLengthDQ_VEOR_instance_)
  {}
};

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBSL_111100110d01nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d01nnnndddd0001nqm1mmmm,
//       rule: VBSL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case8
    : public VectorBinary3RegisterSameLengthDQTesterCase8 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case8()
    : VectorBinary3RegisterSameLengthDQTesterCase8(
      state_.VectorBinary3RegisterSameLengthDQ_VBSL_instance_)
  {}
};

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIT_111100110d10nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d10nnnndddd0001nqm1mmmm,
//       rule: VBIT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case9
    : public VectorBinary3RegisterSameLengthDQTesterCase9 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case9()
    : VectorBinary3RegisterSameLengthDQTesterCase9(
      state_.VectorBinary3RegisterSameLengthDQ_VBIT_instance_)
  {}
};

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIF_111100110d11nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d11nnnndddd0001nqm1mmmm,
//       rule: VBIF,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case10
    : public VectorBinary3RegisterSameLengthDQTesterCase10 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case10()
    : VectorBinary3RegisterSameLengthDQTesterCase10(
      state_.VectorBinary3RegisterSameLengthDQ_VBIF_instance_)
  {}
};

// A(11:8)=0010 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VHSUB_1111001u0dssnnnndddd0010nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0010nqm0mmmm,
//       rule: VHSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case11
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase11 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case11()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase11(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VHSUB_instance_)
  {}
};

// A(11:8)=0010 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQSUB_1111001u0dssnnnndddd0010nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0010nqm1mmmm,
//       rule: VQSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case12
    : public VectorBinary3RegisterSameLengthDQTesterCase12 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case12()
    : VectorBinary3RegisterSameLengthDQTesterCase12(
      state_.VectorBinary3RegisterSameLengthDQ_VQSUB_instance_)
  {}
};

// A(11:8)=0011 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_register_A1_1111001u0dssnnnndddd0011nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0011nqm0mmmm,
//       rule: VCGT_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case13
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase13 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case13()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase13(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1_instance_)
  {}
};

// A(11:8)=0011 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_register_A1_1111001u0dssnnnndddd0011nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0011nqm1mmmm,
//       rule: VCGE_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case14
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase14 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case14()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase14(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1_instance_)
  {}
};

// A(11:8)=0100 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSHL_register_1111001u0dssnnnndddd0100nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0100nqm0mmmm,
//       rule: VSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case15
    : public VectorBinary3RegisterSameLengthDQTesterCase15 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case15()
    : VectorBinary3RegisterSameLengthDQTesterCase15(
      state_.VectorBinary3RegisterSameLengthDQ_VSHL_register_instance_)
  {}
};

// A(11:8)=0100 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQSHL_register_1111001u0dssnnnndddd0100nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0100nqm1mmmm,
//       rule: VQSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case16
    : public VectorBinary3RegisterSameLengthDQTesterCase16 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case16()
    : VectorBinary3RegisterSameLengthDQTesterCase16(
      state_.VectorBinary3RegisterSameLengthDQ_VQSHL_register_instance_)
  {}
};

// A(11:8)=0101 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSHL_1111001u0dssnnnndddd0101nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0101nqm0mmmm,
//       rule: VRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case17
    : public VectorBinary3RegisterSameLengthDQTesterCase17 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case17()
    : VectorBinary3RegisterSameLengthDQTesterCase17(
      state_.VectorBinary3RegisterSameLengthDQ_VRSHL_instance_)
  {}
};

// A(11:8)=0101 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQRSHL_1111001u0dssnnnndddd0101nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0101nqm1mmmm,
//       rule: VQRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case18
    : public VectorBinary3RegisterSameLengthDQTesterCase18 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case18()
    : VectorBinary3RegisterSameLengthDQTesterCase18(
      state_.VectorBinary3RegisterSameLengthDQ_VQRSHL_instance_)
  {}
};

// A(11:8)=0110 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMAX_1111001u0dssnnnndddd0110nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0110nqm0mmmm,
//       rule: VMAX,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case19
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase19 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case19()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase19(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMAX_instance_)
  {}
};

// A(11:8)=0110 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMIN_1111001u0dssnnnndddd0110nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0110nqm1mmmm,
//       rule: VMIN,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case20
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase20 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case20()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase20(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMIN_instance_)
  {}
};

// A(11:8)=0111 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABD_1111001u0dssnnnndddd0111nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0111nqm0mmmm,
//       rule: VABD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case21
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase21 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case21()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase21(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VABD_instance_)
  {}
};

// A(11:8)=0111 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0111nqm1mmmm,
//       rule: VABA,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case22
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase22 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case22()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase22(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VABA_instance_)
  {}
};

// A(11:8)=1000 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1000nqm0mmmm,
//       rule: VADD_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case23
    : public VectorBinary3RegisterSameLengthDQTesterCase23 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case23()
    : VectorBinary3RegisterSameLengthDQTesterCase23(
      state_.VectorBinary3RegisterSameLengthDQ_VADD_integer_instance_)
  {}
};

// A(11:8)=1000 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSUB_integer_111100110dssnnnndddd1000nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1000nqm0mmmm,
//       rule: VSUB_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary3RegisterSameLengthDQTester_Case24
    : public VectorBinary3RegisterSameLengthDQTesterCase24 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case24()
    : VectorBinary3RegisterSameLengthDQTesterCase24(
      state_.VectorBinary3RegisterSameLengthDQ_VSUB_integer_instance_)
  {}
};

// A(11:8)=1000 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VTST_111100100dssnnnndddd1000nqm1mmmm_case_0,
//       pattern: 111100100dssnnnndddd1000nqm1mmmm,
//       rule: VTST,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case25
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase25 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case25()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase25(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VTST_instance_)
  {}
};

// A(11:8)=1000 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_register_A1_111100110dssnnnndddd1000nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1000nqm1mmmm,
//       rule: VCEQ_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case26
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase26 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case26()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase26(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1_instance_)
  {}
};

// A(11:8)=1001 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLA_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//       rule: VMLA_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case27
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase27 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case27()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase27(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1_instance_)
  {}
};

// A(11:8)=1001 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLS_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//       rule: VMLS_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case28
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase28 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case28()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase28(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1_instance_)
  {}
};

// A(11:8)=1001 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_integer_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//       rule: VMUL_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case29
    : public VectorBinary3RegisterSameLengthDQI8_16_32TesterCase29 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case29()
    : VectorBinary3RegisterSameLengthDQI8_16_32TesterCase29(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1_instance_)
  {}
};

// A(11:8)=1001 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//       rule: VMUL_polynomial_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=~00 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI8PTester_Case30
    : public VectorBinary3RegisterSameLengthDQI8PTesterCase30 {
 public:
  VectorBinary3RegisterSameLengthDQI8PTester_Case30()
    : VectorBinary3RegisterSameLengthDQI8PTesterCase30(
      state_.VectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1_instance_)
  {}
};

// A(11:8)=1010 & B(4)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMAX_1111001u0dssnnnndddd1010n0m0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1010n0m0mmmm,
//       rule: VPMAX,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDITester_Case31
    : public VectorBinary3RegisterSameLengthDITesterCase31 {
 public:
  VectorBinary3RegisterSameLengthDITester_Case31()
    : VectorBinary3RegisterSameLengthDITesterCase31(
      state_.VectorBinary3RegisterSameLengthDI_VPMAX_instance_)
  {}
};

// A(11:8)=1010 & B(4)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMIN_1111001u0dssnnnndddd1010n0m1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1010n0m1mmmm,
//       rule: VPMIN,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDITester_Case32
    : public VectorBinary3RegisterSameLengthDITesterCase32 {
 public:
  VectorBinary3RegisterSameLengthDITester_Case32()
    : VectorBinary3RegisterSameLengthDITesterCase32(
      state_.VectorBinary3RegisterSameLengthDI_VPMIN_instance_)
  {}
};

// A(11:8)=1011 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1011nqm0mmmm,
//       rule: VQDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI16_32Tester_Case33
    : public VectorBinary3RegisterSameLengthDQI16_32TesterCase33 {
 public:
  VectorBinary3RegisterSameLengthDQI16_32Tester_Case33()
    : VectorBinary3RegisterSameLengthDQI16_32TesterCase33(
      state_.VectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1_instance_)
  {}
};

// A(11:8)=1011 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQRDMULH_A1_111100110dssnnnndddd1011nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1011nqm0mmmm,
//       rule: VQRDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDQI16_32Tester_Case34
    : public VectorBinary3RegisterSameLengthDQI16_32TesterCase34 {
 public:
  VectorBinary3RegisterSameLengthDQI16_32Tester_Case34()
    : VectorBinary3RegisterSameLengthDQI16_32TesterCase34(
      state_.VectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1_instance_)
  {}
};

// A(11:8)=1011 & B(4)=1 & U(24)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_0,
//       pattern: 111100100dssnnnndddd1011n0m1mmmm,
//       rule: VPADD_integer,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLengthDITester_Case35
    : public VectorBinary3RegisterSameLengthDITesterCase35 {
 public:
  VectorBinary3RegisterSameLengthDITester_Case35()
    : VectorBinary3RegisterSameLengthDITesterCase35(
      state_.VectorBinary3RegisterSameLengthDI_VPADD_integer_instance_)
  {}
};

// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=0x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VFMA_A1_111100100d00nnnndddd1100nqm1mmmm_case_0,
//       pattern: 111100100d00nnnndddd1100nqm1mmmm,
//       rule: VFMA_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case36
    : public VectorBinary3RegisterSameLength32_DQTesterCase36 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case36()
    : VectorBinary3RegisterSameLength32_DQTesterCase36(
      state_.VectorBinary3RegisterSameLength32_DQ_VFMA_A1_instance_)
  {}
};

// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=1x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VFMS_A1_111100100d10nnnndddd1100nqm1mmmm_case_0,
//       pattern: 111100100d10nnnndddd1100nqm1mmmm,
//       rule: VFMS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case37
    : public VectorBinary3RegisterSameLength32_DQTesterCase37 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case37()
    : VectorBinary3RegisterSameLength32_DQTesterCase37(
      state_.VectorBinary3RegisterSameLength32_DQ_VFMS_A1_instance_)
  {}
};

// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VADD_floating_point_A1_111100100d0snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100100d0snnnndddd1101nqm0mmmm,
//       rule: VADD_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case38
    : public VectorBinary3RegisterSameLength32_DQTesterCase38 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case38()
    : VectorBinary3RegisterSameLength32_DQTesterCase38(
      state_.VectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1_instance_)
  {}
};

// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSUB_floating_point_A1_111100100d1snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100100d1snnnndddd1101nqm0mmmm,
//       rule: VSUB_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case39
    : public VectorBinary3RegisterSameLength32_DQTesterCase39 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case39()
    : VectorBinary3RegisterSameLength32_DQTesterCase39(
      state_.VectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1_instance_)
  {}
};

// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100110d0snnnndddd1101nqm0mmmm,
//       rule: VPADD_floating_point,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32PTester_Case40
    : public VectorBinary3RegisterSameLength32PTesterCase40 {
 public:
  VectorBinary3RegisterSameLength32PTester_Case40()
    : VectorBinary3RegisterSameLength32PTesterCase40(
      state_.VectorBinary3RegisterSameLength32P_VPADD_floating_point_instance_)
  {}
};

// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100110d1snnnndddd1101nqm0mmmm,
//       rule: VABD_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case41
    : public VectorBinary3RegisterSameLength32_DQTesterCase41 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case41()
    : VectorBinary3RegisterSameLength32_DQTesterCase41(
      state_.VectorBinary3RegisterSameLength32_DQ_VABD_floating_point_instance_)
  {}
};

// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLA_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//       rule: VMLA_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case42
    : public VectorBinary3RegisterSameLength32_DQTesterCase42 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case42()
    : VectorBinary3RegisterSameLength32_DQTesterCase42(
      state_.VectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1_instance_)
  {}
};

// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLS_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//       rule: VMLS_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case43
    : public VectorBinary3RegisterSameLength32_DQTesterCase43 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case43()
    : VectorBinary3RegisterSameLength32_DQTesterCase43(
      state_.VectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1_instance_)
  {}
};

// A(11:8)=1101 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_floating_point_A1_111100110d0snnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100110d0snnnndddd1101nqm1mmmm,
//       rule: VMUL_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case44
    : public VectorBinary3RegisterSameLength32_DQTesterCase44 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case44()
    : VectorBinary3RegisterSameLength32_DQTesterCase44(
      state_.VectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1_instance_)
  {}
};

// A(11:8)=1110 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_register_A2_111100100d0snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100100d0snnnndddd1110nqm0mmmm,
//       rule: VCEQ_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case45
    : public VectorBinary3RegisterSameLength32_DQTesterCase45 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case45()
    : VectorBinary3RegisterSameLength32_DQTesterCase45(
      state_.VectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2_instance_)
  {}
};

// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_register_A2_111100110d0snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100110d0snnnndddd1110nqm0mmmm,
//       rule: VCGE_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case46
    : public VectorBinary3RegisterSameLength32_DQTesterCase46 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case46()
    : VectorBinary3RegisterSameLength32_DQTesterCase46(
      state_.VectorBinary3RegisterSameLength32_DQ_VCGE_register_A2_instance_)
  {}
};

// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_register_A2_111100110d1snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100110d1snnnndddd1110nqm0mmmm,
//       rule: VCGT_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case47
    : public VectorBinary3RegisterSameLength32_DQTesterCase47 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case47()
    : VectorBinary3RegisterSameLength32_DQTesterCase47(
      state_.VectorBinary3RegisterSameLength32_DQ_VCGT_register_A2_instance_)
  {}
};

// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VACGE_111100110dssnnnndddd1110nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1110nqm1mmmm,
//       rule: VACGE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case48
    : public VectorBinary3RegisterSameLength32_DQTesterCase48 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case48()
    : VectorBinary3RegisterSameLength32_DQTesterCase48(
      state_.VectorBinary3RegisterSameLength32_DQ_VACGE_instance_)
  {}
};

// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VACGT_111100110dssnnnndddd1110nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1110nqm1mmmm,
//       rule: VACGT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case49
    : public VectorBinary3RegisterSameLength32_DQTesterCase49 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case49()
    : VectorBinary3RegisterSameLength32_DQTesterCase49(
      state_.VectorBinary3RegisterSameLength32_DQ_VACGT_instance_)
  {}
};

// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMAX_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1111nqm0mmmm,
//       rule: VMAX_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case50
    : public VectorBinary3RegisterSameLength32_DQTesterCase50 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case50()
    : VectorBinary3RegisterSameLength32_DQTesterCase50(
      state_.VectorBinary3RegisterSameLength32_DQ_VMAX_floating_point_instance_)
  {}
};

// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMIN_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1111nqm0mmmm,
//       rule: VMIN_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case51
    : public VectorBinary3RegisterSameLength32_DQTesterCase51 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case51()
    : VectorBinary3RegisterSameLength32_DQTesterCase51(
      state_.VectorBinary3RegisterSameLength32_DQ_VMIN_floating_point_instance_)
  {}
};

// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMAX_111100110dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1111nqm0mmmm,
//       rule: VPMAX,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32PTester_Case52
    : public VectorBinary3RegisterSameLength32PTesterCase52 {
 public:
  VectorBinary3RegisterSameLength32PTester_Case52()
    : VectorBinary3RegisterSameLength32PTesterCase52(
      state_.VectorBinary3RegisterSameLength32P_VPMAX_instance_)
  {}
};

// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMIN_111100110dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1111nqm0mmmm,
//       rule: VPMIN,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32PTester_Case53
    : public VectorBinary3RegisterSameLength32PTesterCase53 {
 public:
  VectorBinary3RegisterSameLength32PTester_Case53()
    : VectorBinary3RegisterSameLength32PTesterCase53(
      state_.VectorBinary3RegisterSameLength32P_VPMIN_instance_)
  {}
};

// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRECPS_111100100d0snnnndddd1111nqm1mmmm_case_0,
//       pattern: 111100100d0snnnndddd1111nqm1mmmm,
//       rule: VRECPS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case54
    : public VectorBinary3RegisterSameLength32_DQTesterCase54 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case54()
    : VectorBinary3RegisterSameLength32_DQTesterCase54(
      state_.VectorBinary3RegisterSameLength32_DQ_VRECPS_instance_)
  {}
};

// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSQRTS_111100100d1snnnndddd1111nqm1mmmm_case_0,
//       pattern: 111100100d1snnnndddd1111nqm1mmmm,
//       rule: VRSQRTS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
class VectorBinary3RegisterSameLength32_DQTester_Case55
    : public VectorBinary3RegisterSameLength32_DQTesterCase55 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case55()
    : VectorBinary3RegisterSameLength32_DQTesterCase55(
      state_.VectorBinary3RegisterSameLength32_DQ_VRSQRTS_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// A(11:8)=0000 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VHADD_1111001u0dssnnnndddd0000nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0000nqm0mmmm,
//       rule: VHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case0_TestCase0) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case0 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VHADD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0000nqm0mmmm");
}

// A(11:8)=0000 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQADD_1111001u0dssnnnndddd0000nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0000nqm1mmmm,
//       rule: VQADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case1_TestCase1) {
  VectorBinary3RegisterSameLengthDQTester_Case1 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VQADD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0000nqm1mmmm");
}

// A(11:8)=0001 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRHADD_1111001u0dssnnnndddd0001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0001nqm0mmmm,
//       rule: VRHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case2_TestCase2) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case2 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VRHADD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0001nqm0mmmm");
}

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VAND_register_111100100d00nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d00nnnndddd0001nqm1mmmm,
//       rule: VAND_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case3_TestCase3) {
  VectorBinary3RegisterSameLengthDQTester_Case3 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VAND_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d00nnnndddd0001nqm1mmmm");
}

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIC_register_111100100d01nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d01nnnndddd0001nqm1mmmm,
//       rule: VBIC_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case4_TestCase4) {
  VectorBinary3RegisterSameLengthDQTester_Case4 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VBIC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d01nnnndddd0001nqm1mmmm");
}

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VORR_register_or_VMOV_register_A1_111100100d10nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d10nnnndddd0001nqm1mmmm,
//       rule: VORR_register_or_VMOV_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case5_TestCase5) {
  VectorBinary3RegisterSameLengthDQTester_Case5 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VORR_register_or_VMOV_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d10nnnndddd0001nqm1mmmm");
}

// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VORN_register_111100100d11nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100100d11nnnndddd0001nqm1mmmm,
//       rule: VORN_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case6_TestCase6) {
  VectorBinary3RegisterSameLengthDQTester_Case6 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VORN_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d11nnnndddd0001nqm1mmmm");
}

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VEOR_111100110d00nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d00nnnndddd0001nqm1mmmm,
//       rule: VEOR,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case7_TestCase7) {
  VectorBinary3RegisterSameLengthDQTester_Case7 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VEOR actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d00nnnndddd0001nqm1mmmm");
}

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBSL_111100110d01nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d01nnnndddd0001nqm1mmmm,
//       rule: VBSL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case8_TestCase8) {
  VectorBinary3RegisterSameLengthDQTester_Case8 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VBSL actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d01nnnndddd0001nqm1mmmm");
}

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIT_111100110d10nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d10nnnndddd0001nqm1mmmm,
//       rule: VBIT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case9_TestCase9) {
  VectorBinary3RegisterSameLengthDQTester_Case9 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VBIT actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d10nnnndddd0001nqm1mmmm");
}

// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VBIF_111100110d11nnnndddd0001nqm1mmmm_case_0,
//       pattern: 111100110d11nnnndddd0001nqm1mmmm,
//       rule: VBIF,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case10_TestCase10) {
  VectorBinary3RegisterSameLengthDQTester_Case10 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VBIF actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d11nnnndddd0001nqm1mmmm");
}

// A(11:8)=0010 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VHSUB_1111001u0dssnnnndddd0010nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0010nqm0mmmm,
//       rule: VHSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case11_TestCase11) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case11 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VHSUB actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0010nqm0mmmm");
}

// A(11:8)=0010 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQSUB_1111001u0dssnnnndddd0010nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0010nqm1mmmm,
//       rule: VQSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case12_TestCase12) {
  VectorBinary3RegisterSameLengthDQTester_Case12 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VQSUB actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0010nqm1mmmm");
}

// A(11:8)=0011 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_register_A1_1111001u0dssnnnndddd0011nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0011nqm0mmmm,
//       rule: VCGT_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case13_TestCase13) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case13 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VCGT_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0011nqm0mmmm");
}

// A(11:8)=0011 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_register_A1_1111001u0dssnnnndddd0011nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0011nqm1mmmm,
//       rule: VCGE_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case14_TestCase14) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case14 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VCGE_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0011nqm1mmmm");
}

// A(11:8)=0100 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSHL_register_1111001u0dssnnnndddd0100nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0100nqm0mmmm,
//       rule: VSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case15_TestCase15) {
  VectorBinary3RegisterSameLengthDQTester_Case15 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VSHL_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0100nqm0mmmm");
}

// A(11:8)=0100 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQSHL_register_1111001u0dssnnnndddd0100nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0100nqm1mmmm,
//       rule: VQSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case16_TestCase16) {
  VectorBinary3RegisterSameLengthDQTester_Case16 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VQSHL_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0100nqm1mmmm");
}

// A(11:8)=0101 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSHL_1111001u0dssnnnndddd0101nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0101nqm0mmmm,
//       rule: VRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case17_TestCase17) {
  VectorBinary3RegisterSameLengthDQTester_Case17 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VRSHL actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0101nqm0mmmm");
}

// A(11:8)=0101 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQRSHL_1111001u0dssnnnndddd0101nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0101nqm1mmmm,
//       rule: VQRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case18_TestCase18) {
  VectorBinary3RegisterSameLengthDQTester_Case18 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VQRSHL actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0101nqm1mmmm");
}

// A(11:8)=0110 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMAX_1111001u0dssnnnndddd0110nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0110nqm0mmmm,
//       rule: VMAX,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case19_TestCase19) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case19 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VMAX actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0110nqm0mmmm");
}

// A(11:8)=0110 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMIN_1111001u0dssnnnndddd0110nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0110nqm1mmmm,
//       rule: VMIN,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case20_TestCase20) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case20 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VMIN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0110nqm1mmmm");
}

// A(11:8)=0111 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABD_1111001u0dssnnnndddd0111nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0111nqm0mmmm,
//       rule: VABD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case21_TestCase21) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case21 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VABD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0111nqm0mmmm");
}

// A(11:8)=0111 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd0111nqm1mmmm,
//       rule: VABA,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case22_TestCase22) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case22 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VABA actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd0111nqm1mmmm");
}

// A(11:8)=1000 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1000nqm0mmmm,
//       rule: VADD_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case23_TestCase23) {
  VectorBinary3RegisterSameLengthDQTester_Case23 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VADD_integer actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100dssnnnndddd1000nqm0mmmm");
}

// A(11:8)=1000 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSUB_integer_111100110dssnnnndddd1000nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1000nqm0mmmm,
//       rule: VSUB_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case24_TestCase24) {
  VectorBinary3RegisterSameLengthDQTester_Case24 baseline_tester;
  NamedActual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1_VSUB_integer actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110dssnnnndddd1000nqm0mmmm");
}

// A(11:8)=1000 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VTST_111100100dssnnnndddd1000nqm1mmmm_case_0,
//       pattern: 111100100dssnnnndddd1000nqm1mmmm,
//       rule: VTST,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case25_TestCase25) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case25 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VTST actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100dssnnnndddd1000nqm1mmmm");
}

// A(11:8)=1000 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_register_A1_111100110dssnnnndddd1000nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1000nqm1mmmm,
//       rule: VCEQ_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case26_TestCase26) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case26 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VCEQ_register_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110dssnnnndddd1000nqm1mmmm");
}

// A(11:8)=1001 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLA_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//       rule: VMLA_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case27_TestCase27) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case27 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VMLA_integer_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd1001nqm0mmmm");
}

// A(11:8)=1001 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLS_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//       rule: VMLS_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case28_TestCase28) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case28 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VMLS_integer_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd1001nqm0mmmm");
}

// A(11:8)=1001 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_integer_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//       rule: VMUL_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case29_TestCase29) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case29 baseline_tester;
  NamedActual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1_VMUL_integer_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd1001nqm1mmmm");
}

// A(11:8)=1001 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI8P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//       rule: VMUL_polynomial_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(21:20)=~00 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8PTester_Case30_TestCase30) {
  VectorBinary3RegisterSameLengthDQI8PTester_Case30 baseline_tester;
  NamedActual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1_VMUL_polynomial_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd1001nqm1mmmm");
}

// A(11:8)=1010 & B(4)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMAX_1111001u0dssnnnndddd1010n0m0mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1010n0m0mmmm,
//       rule: VPMAX,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDITester_Case31_TestCase31) {
  VectorBinary3RegisterSameLengthDITester_Case31 baseline_tester;
  NamedActual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1_VPMAX actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd1010n0m0mmmm");
}

// A(11:8)=1010 & B(4)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMIN_1111001u0dssnnnndddd1010n0m1mmmm_case_0,
//       pattern: 1111001u0dssnnnndddd1010n0m1mmmm,
//       rule: VPMIN,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDITester_Case32_TestCase32) {
  VectorBinary3RegisterSameLengthDITester_Case32 baseline_tester;
  NamedActual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1_VPMIN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u0dssnnnndddd1010n0m1mmmm");
}

// A(11:8)=1011 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1011nqm0mmmm,
//       rule: VQDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI16_32Tester_Case33_TestCase33) {
  VectorBinary3RegisterSameLengthDQI16_32Tester_Case33 baseline_tester;
  NamedActual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1_VQDMULH_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100dssnnnndddd1011nqm0mmmm");
}

// A(11:8)=1011 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VQRDMULH_A1_111100110dssnnnndddd1011nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1011nqm0mmmm,
//       rule: VQRDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI16_32Tester_Case34_TestCase34) {
  VectorBinary3RegisterSameLengthDQI16_32Tester_Case34 baseline_tester;
  NamedActual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1_VQRDMULH_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110dssnnnndddd1011nqm0mmmm");
}

// A(11:8)=1011 & B(4)=1 & U(24)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_0,
//       pattern: 111100100dssnnnndddd1011n0m1mmmm,
//       rule: VPADD_integer,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDITester_Case35_TestCase35) {
  VectorBinary3RegisterSameLengthDITester_Case35 baseline_tester;
  NamedActual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1_VPADD_integer actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100dssnnnndddd1011n0m1mmmm");
}

// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=0x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VFMA_A1_111100100d00nnnndddd1100nqm1mmmm_case_0,
//       pattern: 111100100d00nnnndddd1100nqm1mmmm,
//       rule: VFMA_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case36_TestCase36) {
  VectorBinary3RegisterSameLength32_DQTester_Case36 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VFMA_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d00nnnndddd1100nqm1mmmm");
}

// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=1x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VFMS_A1_111100100d10nnnndddd1100nqm1mmmm_case_0,
//       pattern: 111100100d10nnnndddd1100nqm1mmmm,
//       rule: VFMS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case37_TestCase37) {
  VectorBinary3RegisterSameLength32_DQTester_Case37 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VFMS_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d10nnnndddd1100nqm1mmmm");
}

// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VADD_floating_point_A1_111100100d0snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100100d0snnnndddd1101nqm0mmmm,
//       rule: VADD_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case38_TestCase38) {
  VectorBinary3RegisterSameLength32_DQTester_Case38 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VADD_floating_point_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d0snnnndddd1101nqm0mmmm");
}

// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VSUB_floating_point_A1_111100100d1snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100100d1snnnndddd1101nqm0mmmm,
//       rule: VSUB_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case39_TestCase39) {
  VectorBinary3RegisterSameLength32_DQTester_Case39 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VSUB_floating_point_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d1snnnndddd1101nqm0mmmm");
}

// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100110d0snnnndddd1101nqm0mmmm,
//       rule: VPADD_floating_point,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32PTester_Case40_TestCase40) {
  VectorBinary3RegisterSameLength32PTester_Case40 baseline_tester;
  NamedActual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1_VPADD_floating_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d0snnnndddd1101nqm0mmmm");
}

// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_0,
//       pattern: 111100110d1snnnndddd1101nqm0mmmm,
//       rule: VABD_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case41_TestCase41) {
  VectorBinary3RegisterSameLength32_DQTester_Case41 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VABD_floating_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d1snnnndddd1101nqm0mmmm");
}

// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLA_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//       rule: VMLA_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case42_TestCase42) {
  VectorBinary3RegisterSameLength32_DQTester_Case42 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VMLA_floating_point_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100dpsnnnndddd1101nqm1mmmm");
}

// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMLS_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//       rule: VMLS_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case43_TestCase43) {
  VectorBinary3RegisterSameLength32_DQTester_Case43 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VMLS_floating_point_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100dpsnnnndddd1101nqm1mmmm");
}

// A(11:8)=1101 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMUL_floating_point_A1_111100110d0snnnndddd1101nqm1mmmm_case_0,
//       pattern: 111100110d0snnnndddd1101nqm1mmmm,
//       rule: VMUL_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case44_TestCase44) {
  VectorBinary3RegisterSameLength32_DQTester_Case44 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VMUL_floating_point_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d0snnnndddd1101nqm1mmmm");
}

// A(11:8)=1110 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCEQ_register_A2_111100100d0snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100100d0snnnndddd1110nqm0mmmm,
//       rule: VCEQ_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case45_TestCase45) {
  VectorBinary3RegisterSameLength32_DQTester_Case45 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VCEQ_register_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d0snnnndddd1110nqm0mmmm");
}

// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGE_register_A2_111100110d0snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100110d0snnnndddd1110nqm0mmmm,
//       rule: VCGE_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case46_TestCase46) {
  VectorBinary3RegisterSameLength32_DQTester_Case46 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VCGE_register_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d0snnnndddd1110nqm0mmmm");
}

// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCGT_register_A2_111100110d1snnnndddd1110nqm0mmmm_case_0,
//       pattern: 111100110d1snnnndddd1110nqm0mmmm,
//       rule: VCGT_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case47_TestCase47) {
  VectorBinary3RegisterSameLength32_DQTester_Case47 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VCGT_register_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110d1snnnndddd1110nqm0mmmm");
}

// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VACGE_111100110dssnnnndddd1110nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1110nqm1mmmm,
//       rule: VACGE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case48_TestCase48) {
  VectorBinary3RegisterSameLength32_DQTester_Case48 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VACGE actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110dssnnnndddd1110nqm1mmmm");
}

// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VACGT_111100110dssnnnndddd1110nqm1mmmm_case_0,
//       pattern: 111100110dssnnnndddd1110nqm1mmmm,
//       rule: VACGT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case49_TestCase49) {
  VectorBinary3RegisterSameLength32_DQTester_Case49 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VACGT actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110dssnnnndddd1110nqm1mmmm");
}

// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMAX_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1111nqm0mmmm,
//       rule: VMAX_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case50_TestCase50) {
  VectorBinary3RegisterSameLength32_DQTester_Case50 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VMAX_floating_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100dssnnnndddd1111nqm0mmmm");
}

// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VMIN_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100100dssnnnndddd1111nqm0mmmm,
//       rule: VMIN_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case51_TestCase51) {
  VectorBinary3RegisterSameLength32_DQTester_Case51 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VMIN_floating_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100dssnnnndddd1111nqm0mmmm");
}

// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMAX_111100110dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1111nqm0mmmm,
//       rule: VPMAX,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32PTester_Case52_TestCase52) {
  VectorBinary3RegisterSameLength32PTester_Case52 baseline_tester;
  NamedActual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1_VPMAX actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110dssnnnndddd1111nqm0mmmm");
}

// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       generated_baseline: VPMIN_111100110dssnnnndddd1111nqm0mmmm_case_0,
//       pattern: 111100110dssnnnndddd1111nqm0mmmm,
//       rule: VPMIN,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32PTester_Case53_TestCase53) {
  VectorBinary3RegisterSameLength32PTester_Case53 baseline_tester;
  NamedActual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1_VPMIN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100110dssnnnndddd1111nqm0mmmm");
}

// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRECPS_111100100d0snnnndddd1111nqm1mmmm_case_0,
//       pattern: 111100100d0snnnndddd1111nqm1mmmm,
//       rule: VRECPS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case54_TestCase54) {
  VectorBinary3RegisterSameLength32_DQTester_Case54 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VRECPS actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d0snnnndddd1111nqm1mmmm");
}

// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VRSQRTS_111100100d1snnnndddd1111nqm1mmmm_case_0,
//       pattern: 111100100d1snnnndddd1111nqm1mmmm,
//       rule: VRSQRTS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20),
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case55_TestCase55) {
  VectorBinary3RegisterSameLength32_DQTester_Case55 baseline_tester;
  NamedActual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1_VRSQRTS actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100100d1snnnndddd1111nqm1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
