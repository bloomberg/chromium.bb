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


// A(11:8)=0000
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSHR_1111001u1diiiiiidddd0000lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0000lqm1mmmm,
//       rule: VSHR,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITesterCase0
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_ITesterCase0(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_ITesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0001
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSRA_1111001u1diiiiiidddd0001lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0001lqm1mmmm,
//       rule: VSRA,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITesterCase1
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_ITesterCase1(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_ITesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0010
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0010lqm1mmmm,
//       rule: VRSHR,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITesterCase2
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_ITesterCase2(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_ITesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0011
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VRSRA_1111001u1diiiiiidddd0011lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0011lqm1mmmm,
//       rule: VRSRA,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITesterCase3
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_ITesterCase3(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_ITesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0100 & U(24)=1
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSRI_111100111diiiiiidddd0100lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100111diiiiiidddd0100lqm1mmmm,
//       rule: VSRI,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITesterCase4
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_ITesterCase4(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_ITesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000400) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0101 & U(24)=0
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSHL_immediate_111100101diiiiiidddd0101lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd0101lqm1mmmm,
//       rule: VSHL_immediate,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITesterCase5
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_ITesterCase5(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_ITesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000500) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=0101 & U(24)=1
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSLI_111100111diiiiiidddd0101lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100111diiiiiidddd0101lqm1mmmm,
//       rule: VSLI,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITesterCase6
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_ITesterCase6(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_ITesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000500) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1000 & U(24)=0 & B(6)=0 & L(7)=0
//    = {Vm: Vm(3:0),
//       actual: Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64R,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vm(3:0)],
//       generated_baseline: VSHRN_111100101diiiiiidddd100000m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd100000m1mmmm,
//       rule: VSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vm(0)=1 => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase7
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase7(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // B(6)=~0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1000 & U(24)=0 & B(6)=1 & L(7)=0
//    = {Vm: Vm(3:0),
//       actual: Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64R,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vm(3:0)],
//       generated_baseline: VRSHRN_111100101diiiiiidddd100001m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd100001m1mmmm,
//       rule: VRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vm(0)=1 => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase8
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase8(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // B(6)=~1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1000 & U(24)=1 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQRSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase9
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase9(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // B(6)=~0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1000 & U(24)=1 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRUN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase10
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase10(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // B(6)=~1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1001 & U(24)=0 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQSHRN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase11
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase11(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // B(6)=~0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1001 & U(24)=0 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase12
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase12(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // B(6)=~1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1001 & U(24)=1 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase13
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase13(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // B(6)=~0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1001 & U(24)=1 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase14
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase14(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // B(6)=~1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=1010 & B(6)=0 & L(7)=0
//    = {Vd: Vd(15:12),
//       actual: Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_E8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12)],
//       generated_baseline: VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd101000m1mmmm,
//       rule: VSHLL_A1_or_VMOVL,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_E8_16_32LTesterCase15
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_E8_16_32LTesterCase15(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_E8_16_32LTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;
  // B(6)=~0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=011x
//    = {L: L(7),
//       Q: Q(6),
//       U: U(24),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_ILS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), Vd(15:12), op(8), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd011plqm1mmmm,
//       rule: VQSHL_VQSHLU_immediate,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ILSTesterCase16
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_ILSTesterCase16(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_ILSTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~011x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// A(11:8)=111x & L(7)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_CVT,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd111p0qm1mmmm,
//       rule: VCVT_between_floating_point_and_fixed_point,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         imm6(21:16)=0xxxxx => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_CVTTesterCase17
    : public VectorBinary2RegisterShiftAmountTester {
 public:
  VectorBinary2RegisterShiftAmount_CVTTesterCase17(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterShiftAmountTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterShiftAmount_CVTTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~111x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000E00) return false;
  // L(7)=~0
  if ((inst.Bits() & 0x00000080)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterShiftAmountTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// A(11:8)=0000
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSHR_1111001u1diiiiiidddd0000lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0000lqm1mmmm,
//       rule: VSHR,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITester_Case0
    : public VectorBinary2RegisterShiftAmount_ITesterCase0 {
 public:
  VectorBinary2RegisterShiftAmount_ITester_Case0()
    : VectorBinary2RegisterShiftAmount_ITesterCase0(
      state_.VectorBinary2RegisterShiftAmount_I_VSHR_instance_)
  {}
};

// A(11:8)=0001
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSRA_1111001u1diiiiiidddd0001lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0001lqm1mmmm,
//       rule: VSRA,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITester_Case1
    : public VectorBinary2RegisterShiftAmount_ITesterCase1 {
 public:
  VectorBinary2RegisterShiftAmount_ITester_Case1()
    : VectorBinary2RegisterShiftAmount_ITesterCase1(
      state_.VectorBinary2RegisterShiftAmount_I_VSRA_instance_)
  {}
};

// A(11:8)=0010
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0010lqm1mmmm,
//       rule: VRSHR,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITester_Case2
    : public VectorBinary2RegisterShiftAmount_ITesterCase2 {
 public:
  VectorBinary2RegisterShiftAmount_ITester_Case2()
    : VectorBinary2RegisterShiftAmount_ITesterCase2(
      state_.VectorBinary2RegisterShiftAmount_I_VRSHR_instance_)
  {}
};

// A(11:8)=0011
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VRSRA_1111001u1diiiiiidddd0011lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0011lqm1mmmm,
//       rule: VRSRA,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITester_Case3
    : public VectorBinary2RegisterShiftAmount_ITesterCase3 {
 public:
  VectorBinary2RegisterShiftAmount_ITester_Case3()
    : VectorBinary2RegisterShiftAmount_ITesterCase3(
      state_.VectorBinary2RegisterShiftAmount_I_VRSRA_instance_)
  {}
};

// A(11:8)=0100 & U(24)=1
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSRI_111100111diiiiiidddd0100lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100111diiiiiidddd0100lqm1mmmm,
//       rule: VSRI,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITester_Case4
    : public VectorBinary2RegisterShiftAmount_ITesterCase4 {
 public:
  VectorBinary2RegisterShiftAmount_ITester_Case4()
    : VectorBinary2RegisterShiftAmount_ITesterCase4(
      state_.VectorBinary2RegisterShiftAmount_I_VSRI_instance_)
  {}
};

// A(11:8)=0101 & U(24)=0
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSHL_immediate_111100101diiiiiidddd0101lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd0101lqm1mmmm,
//       rule: VSHL_immediate,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITester_Case5
    : public VectorBinary2RegisterShiftAmount_ITesterCase5 {
 public:
  VectorBinary2RegisterShiftAmount_ITester_Case5()
    : VectorBinary2RegisterShiftAmount_ITesterCase5(
      state_.VectorBinary2RegisterShiftAmount_I_VSHL_immediate_instance_)
  {}
};

// A(11:8)=0101 & U(24)=1
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSLI_111100111diiiiiidddd0101lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100111diiiiiidddd0101lqm1mmmm,
//       rule: VSLI,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ITester_Case6
    : public VectorBinary2RegisterShiftAmount_ITesterCase6 {
 public:
  VectorBinary2RegisterShiftAmount_ITester_Case6()
    : VectorBinary2RegisterShiftAmount_ITesterCase6(
      state_.VectorBinary2RegisterShiftAmount_I_VSLI_instance_)
  {}
};

// A(11:8)=1000 & U(24)=0 & B(6)=0 & L(7)=0
//    = {Vm: Vm(3:0),
//       actual: Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64R,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vm(3:0)],
//       generated_baseline: VSHRN_111100101diiiiiidddd100000m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd100000m1mmmm,
//       rule: VSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vm(0)=1 => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RTester_Case7
    : public VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase7 {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RTester_Case7()
    : VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase7(
      state_.VectorBinary2RegisterShiftAmount_N16_32_64R_VSHRN_instance_)
  {}
};

// A(11:8)=1000 & U(24)=0 & B(6)=1 & L(7)=0
//    = {Vm: Vm(3:0),
//       actual: Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64R,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vm(3:0)],
//       generated_baseline: VRSHRN_111100101diiiiiidddd100001m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd100001m1mmmm,
//       rule: VRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vm(0)=1 => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RTester_Case8
    : public VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase8 {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RTester_Case8()
    : VectorBinary2RegisterShiftAmount_N16_32_64RTesterCase8(
      state_.VectorBinary2RegisterShiftAmount_N16_32_64R_VRSHRN_instance_)
  {}
};

// A(11:8)=1000 & U(24)=1 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQRSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case9
    : public VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase9 {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case9()
    : VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase9(
      state_.VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN_instance_)
  {}
};

// A(11:8)=1000 & U(24)=1 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRUN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case10
    : public VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase10 {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case10()
    : VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase10(
      state_.VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRUN_instance_)
  {}
};

// A(11:8)=1001 & U(24)=0 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQSHRN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case11
    : public VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase11 {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case11()
    : VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase11(
      state_.VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRN_instance_)
  {}
};

// A(11:8)=1001 & U(24)=0 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case12
    : public VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase12 {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case12()
    : VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase12(
      state_.VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN_instance_)
  {}
};

// A(11:8)=1001 & U(24)=1 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case13
    : public VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase13 {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case13()
    : VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase13(
      state_.VectorBinary2RegisterShiftAmount_N16_32_64RS_VQSHRUN_instance_)
  {}
};

// A(11:8)=1001 & U(24)=1 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case14
    : public VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase14 {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case14()
    : VectorBinary2RegisterShiftAmount_N16_32_64RSTesterCase14(
      state_.VectorBinary2RegisterShiftAmount_N16_32_64RS_VQRSHRN_instance_)
  {}
};

// A(11:8)=1010 & B(6)=0 & L(7)=0
//    = {Vd: Vd(15:12),
//       actual: Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_E8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12)],
//       generated_baseline: VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd101000m1mmmm,
//       rule: VSHLL_A1_or_VMOVL,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_E8_16_32LTester_Case15
    : public VectorBinary2RegisterShiftAmount_E8_16_32LTesterCase15 {
 public:
  VectorBinary2RegisterShiftAmount_E8_16_32LTester_Case15()
    : VectorBinary2RegisterShiftAmount_E8_16_32LTesterCase15(
      state_.VectorBinary2RegisterShiftAmount_E8_16_32L_VSHLL_A1_or_VMOVL_instance_)
  {}
};

// A(11:8)=011x
//    = {L: L(7),
//       Q: Q(6),
//       U: U(24),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_ILS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), Vd(15:12), op(8), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd011plqm1mmmm,
//       rule: VQSHL_VQSHLU_immediate,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_ILSTester_Case16
    : public VectorBinary2RegisterShiftAmount_ILSTesterCase16 {
 public:
  VectorBinary2RegisterShiftAmount_ILSTester_Case16()
    : VectorBinary2RegisterShiftAmount_ILSTesterCase16(
      state_.VectorBinary2RegisterShiftAmount_ILS_VQSHL_VQSHLU_immediate_instance_)
  {}
};

// A(11:8)=111x & L(7)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_CVT,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd111p0qm1mmmm,
//       rule: VCVT_between_floating_point_and_fixed_point,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         imm6(21:16)=0xxxxx => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
class VectorBinary2RegisterShiftAmount_CVTTester_Case17
    : public VectorBinary2RegisterShiftAmount_CVTTesterCase17 {
 public:
  VectorBinary2RegisterShiftAmount_CVTTester_Case17()
    : VectorBinary2RegisterShiftAmount_CVTTesterCase17(
      state_.VectorBinary2RegisterShiftAmount_CVT_VCVT_between_floating_point_and_fixed_point_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// A(11:8)=0000
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSHR_1111001u1diiiiiidddd0000lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0000lqm1mmmm,
//       rule: VSHR,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_ITester_Case0_TestCase0) {
  VectorBinary2RegisterShiftAmount_ITester_Case0 baseline_tester;
  NamedActual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1_VSHR actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd0000lqm1mmmm");
}

// A(11:8)=0001
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSRA_1111001u1diiiiiidddd0001lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0001lqm1mmmm,
//       rule: VSRA,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_ITester_Case1_TestCase1) {
  VectorBinary2RegisterShiftAmount_ITester_Case1 baseline_tester;
  NamedActual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1_VSRA actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd0001lqm1mmmm");
}

// A(11:8)=0010
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0010lqm1mmmm,
//       rule: VRSHR,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_ITester_Case2_TestCase2) {
  VectorBinary2RegisterShiftAmount_ITester_Case2 baseline_tester;
  NamedActual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1_VRSHR actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd0010lqm1mmmm");
}

// A(11:8)=0011
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VRSRA_1111001u1diiiiiidddd0011lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd0011lqm1mmmm,
//       rule: VRSRA,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_ITester_Case3_TestCase3) {
  VectorBinary2RegisterShiftAmount_ITester_Case3 baseline_tester;
  NamedActual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1_VRSRA actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd0011lqm1mmmm");
}

// A(11:8)=0100 & U(24)=1
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSRI_111100111diiiiiidddd0100lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100111diiiiiidddd0100lqm1mmmm,
//       rule: VSRI,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_ITester_Case4_TestCase4) {
  VectorBinary2RegisterShiftAmount_ITester_Case4 baseline_tester;
  NamedActual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1_VSRI actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111diiiiiidddd0100lqm1mmmm");
}

// A(11:8)=0101 & U(24)=0
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSHL_immediate_111100101diiiiiidddd0101lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd0101lqm1mmmm,
//       rule: VSHL_immediate,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_ITester_Case5_TestCase5) {
  VectorBinary2RegisterShiftAmount_ITester_Case5 baseline_tester;
  NamedActual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1_VSHL_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100101diiiiiidddd0101lqm1mmmm");
}

// A(11:8)=0101 & U(24)=1
//    = {L: L(7),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_I,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VSLI_111100111diiiiiidddd0101lqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100111diiiiiidddd0101lqm1mmmm,
//       rule: VSLI,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_ITester_Case6_TestCase6) {
  VectorBinary2RegisterShiftAmount_ITester_Case6 baseline_tester;
  NamedActual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1_VSLI actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100111diiiiiidddd0101lqm1mmmm");
}

// A(11:8)=1000 & U(24)=0 & B(6)=0 & L(7)=0
//    = {Vm: Vm(3:0),
//       actual: Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64R,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vm(3:0)],
//       generated_baseline: VSHRN_111100101diiiiiidddd100000m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd100000m1mmmm,
//       rule: VSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vm(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_N16_32_64RTester_Case7_TestCase7) {
  VectorBinary2RegisterShiftAmount_N16_32_64RTester_Case7 baseline_tester;
  NamedActual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1_VSHRN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100101diiiiiidddd100000m1mmmm");
}

// A(11:8)=1000 & U(24)=0 & B(6)=1 & L(7)=0
//    = {Vm: Vm(3:0),
//       actual: Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64R,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vm(3:0)],
//       generated_baseline: VRSHRN_111100101diiiiiidddd100001m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 111100101diiiiiidddd100001m1mmmm,
//       rule: VRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vm(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_N16_32_64RTester_Case8_TestCase8) {
  VectorBinary2RegisterShiftAmount_N16_32_64RTester_Case8 baseline_tester;
  NamedActual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1_VRSHRN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100101diiiiiidddd100001m1mmmm");
}

// A(11:8)=1000 & U(24)=1 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQRSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case9_TestCase9) {
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case9 baseline_tester;
  NamedActual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1_VQRSHRUN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd100p00m1mmmm");
}

// A(11:8)=1000 & U(24)=1 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRUN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case10_TestCase10) {
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case10 baseline_tester;
  NamedActual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1_VQRSHRUN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd100p01m1mmmm");
}

// A(11:8)=1001 & U(24)=0 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQSHRN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case11_TestCase11) {
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case11 baseline_tester;
  NamedActual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1_VQSHRN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd100p00m1mmmm");
}

// A(11:8)=1001 & U(24)=0 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case12_TestCase12) {
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case12 baseline_tester;
  NamedActual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1_VQRSHRN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd100p01m1mmmm");
}

// A(11:8)=1001 & U(24)=1 & B(6)=0 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//       rule: VQSHRUN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case13_TestCase13) {
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case13 baseline_tester;
  NamedActual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1_VQSHRUN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd100p00m1mmmm");
}

// A(11:8)=1001 & U(24)=1 & B(6)=1 & L(7)=0
//    = {U: U(24),
//       Vm: Vm(3:0),
//       actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), op(8), Vm(3:0)],
//       generated_baseline: VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//       rule: VQRSHRN,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         Vm(0)=1 => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case14_TestCase14) {
  VectorBinary2RegisterShiftAmount_N16_32_64RSTester_Case14 baseline_tester;
  NamedActual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1_VQRSHRN actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd100p01m1mmmm");
}

// A(11:8)=1010 & B(6)=0 & L(7)=0
//    = {Vd: Vd(15:12),
//       actual: Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_E8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12)],
//       generated_baseline: VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd101000m1mmmm,
//       rule: VSHLL_A1_or_VMOVL,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_E8_16_32LTester_Case15_TestCase15) {
  VectorBinary2RegisterShiftAmount_E8_16_32LTester_Case15 baseline_tester;
  NamedActual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1_VSHLL_A1_or_VMOVL actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd101000m1mmmm");
}

// A(11:8)=011x
//    = {L: L(7),
//       Q: Q(6),
//       U: U(24),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_ILS,
//       constraints: ,
//       defs: {},
//       fields: [U(24), imm6(21:16), Vd(15:12), op(8), L(7), Q(6), Vm(3:0)],
//       generated_baseline: VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_0,
//       imm6: imm6(21:16),
//       op: op(8),
//       pattern: 1111001u1diiiiiidddd011plqm1mmmm,
//       rule: VQSHL_VQSHLU_immediate,
//       safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         U(24)=0 &&
//            op(8)=0 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_ILSTester_Case16_TestCase16) {
  VectorBinary2RegisterShiftAmount_ILSTester_Case16 baseline_tester;
  NamedActual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1_VQSHL_VQSHLU_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd011plqm1mmmm");
}

// A(11:8)=111x & L(7)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1,
//       baseline: VectorBinary2RegisterShiftAmount_CVT,
//       constraints: ,
//       defs: {},
//       fields: [imm6(21:16), Vd(15:12), Q(6), Vm(3:0)],
//       generated_baseline: VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_0,
//       imm6: imm6(21:16),
//       pattern: 1111001u1diiiiiidddd111p0qm1mmmm,
//       rule: VCVT_between_floating_point_and_fixed_point,
//       safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//         imm6(21:16)=0xxxxx => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterShiftAmount_CVTTester_Case17_TestCase17) {
  VectorBinary2RegisterShiftAmount_CVTTester_Case17 baseline_tester;
  NamedActual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1_VCVT_between_floating_point_and_fixed_point actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001u1diiiiiidddd111p0qm1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
