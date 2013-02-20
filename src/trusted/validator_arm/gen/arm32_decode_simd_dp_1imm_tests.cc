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


// op(5)=0 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MOVTesterCase0
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_MOVTesterCase0(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_MOVTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;
  // cmode(11:8)=~10x0
  if ((inst.Bits() & 0x00000D00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=0 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_BITTesterCase1
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_BITTesterCase1(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_BITTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;
  // cmode(11:8)=~10x1
  if ((inst.Bits() & 0x00000D00)  !=
          0x00000900) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=0 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MOVTesterCase2
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_MOVTesterCase2(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_MOVTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;
  // cmode(11:8)=~0xx0
  if ((inst.Bits() & 0x00000900)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=0 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_BITTesterCase3
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_BITTesterCase3(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_BITTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;
  // cmode(11:8)=~0xx1
  if ((inst.Bits() & 0x00000900)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=0 & cmode(11:8)=11xx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MOVTesterCase4
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_MOVTesterCase4(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_MOVTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;
  // cmode(11:8)=~11xx
  if ((inst.Bits() & 0x00000C00)  !=
          0x00000C00) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=1 & cmode(11:8)=1110
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MOVTesterCase5
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_MOVTesterCase5(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_MOVTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~1
  if ((inst.Bits() & 0x00000020)  !=
          0x00000020) return false;
  // cmode(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=1 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MVNTesterCase6
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_MVNTesterCase6(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_MVNTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~1
  if ((inst.Bits() & 0x00000020)  !=
          0x00000020) return false;
  // cmode(11:8)=~10x0
  if ((inst.Bits() & 0x00000D00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=1 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_BITTesterCase7
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_BITTesterCase7(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_BITTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~1
  if ((inst.Bits() & 0x00000020)  !=
          0x00000020) return false;
  // cmode(11:8)=~10x1
  if ((inst.Bits() & 0x00000D00)  !=
          0x00000900) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=1 & cmode(11:8)=110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MVNTesterCase8
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_MVNTesterCase8(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_MVNTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~1
  if ((inst.Bits() & 0x00000020)  !=
          0x00000020) return false;
  // cmode(11:8)=~110x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000C00) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=1 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MVNTesterCase9
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_MVNTesterCase9(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_MVNTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~1
  if ((inst.Bits() & 0x00000020)  !=
          0x00000020) return false;
  // cmode(11:8)=~0xx0
  if ((inst.Bits() & 0x00000900)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// op(5)=1 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_BITTesterCase10
    : public Vector1RegisterImmediateTester {
 public:
  Vector1RegisterImmediate_BITTesterCase10(const NamedClassDecoder& decoder)
    : Vector1RegisterImmediateTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Vector1RegisterImmediate_BITTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(5)=~1
  if ((inst.Bits() & 0x00000020)  !=
          0x00000020) return false;
  // cmode(11:8)=~0xx1
  if ((inst.Bits() & 0x00000900)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return Vector1RegisterImmediateTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op(5)=0 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MOVTester_Case0
    : public Vector1RegisterImmediate_MOVTesterCase0 {
 public:
  Vector1RegisterImmediate_MOVTester_Case0()
    : Vector1RegisterImmediate_MOVTesterCase0(
      state_.Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_)
  {}
};

// op(5)=0 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_BITTester_Case1
    : public Vector1RegisterImmediate_BITTesterCase1 {
 public:
  Vector1RegisterImmediate_BITTester_Case1()
    : Vector1RegisterImmediate_BITTesterCase1(
      state_.Vector1RegisterImmediate_BIT_VORR_immediate_instance_)
  {}
};

// op(5)=0 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MOVTester_Case2
    : public Vector1RegisterImmediate_MOVTesterCase2 {
 public:
  Vector1RegisterImmediate_MOVTester_Case2()
    : Vector1RegisterImmediate_MOVTesterCase2(
      state_.Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_)
  {}
};

// op(5)=0 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_BITTester_Case3
    : public Vector1RegisterImmediate_BITTesterCase3 {
 public:
  Vector1RegisterImmediate_BITTester_Case3()
    : Vector1RegisterImmediate_BITTesterCase3(
      state_.Vector1RegisterImmediate_BIT_VORR_immediate_instance_)
  {}
};

// op(5)=0 & cmode(11:8)=11xx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MOVTester_Case4
    : public Vector1RegisterImmediate_MOVTesterCase4 {
 public:
  Vector1RegisterImmediate_MOVTester_Case4()
    : Vector1RegisterImmediate_MOVTesterCase4(
      state_.Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_)
  {}
};

// op(5)=1 & cmode(11:8)=1110
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MOVTester_Case5
    : public Vector1RegisterImmediate_MOVTesterCase5 {
 public:
  Vector1RegisterImmediate_MOVTester_Case5()
    : Vector1RegisterImmediate_MOVTesterCase5(
      state_.Vector1RegisterImmediate_MOV_VMOV_immediate_A1_instance_)
  {}
};

// op(5)=1 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MVNTester_Case6
    : public Vector1RegisterImmediate_MVNTesterCase6 {
 public:
  Vector1RegisterImmediate_MVNTester_Case6()
    : Vector1RegisterImmediate_MVNTesterCase6(
      state_.Vector1RegisterImmediate_MVN_VMVN_immediate_instance_)
  {}
};

// op(5)=1 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_BITTester_Case7
    : public Vector1RegisterImmediate_BITTesterCase7 {
 public:
  Vector1RegisterImmediate_BITTester_Case7()
    : Vector1RegisterImmediate_BITTesterCase7(
      state_.Vector1RegisterImmediate_BIT_VBIC_immediate_instance_)
  {}
};

// op(5)=1 & cmode(11:8)=110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MVNTester_Case8
    : public Vector1RegisterImmediate_MVNTesterCase8 {
 public:
  Vector1RegisterImmediate_MVNTester_Case8()
    : Vector1RegisterImmediate_MVNTesterCase8(
      state_.Vector1RegisterImmediate_MVN_VMVN_immediate_instance_)
  {}
};

// op(5)=1 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_MVNTester_Case9
    : public Vector1RegisterImmediate_MVNTesterCase9 {
 public:
  Vector1RegisterImmediate_MVNTester_Case9()
    : Vector1RegisterImmediate_MVNTesterCase9(
      state_.Vector1RegisterImmediate_MVN_VMVN_immediate_instance_)
  {}
};

// op(5)=1 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
class Vector1RegisterImmediate_BITTester_Case10
    : public Vector1RegisterImmediate_BITTesterCase10 {
 public:
  Vector1RegisterImmediate_BITTester_Case10()
    : Vector1RegisterImmediate_BITTesterCase10(
      state_.Vector1RegisterImmediate_BIT_VBIC_immediate_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op(5)=0 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MOVTester_Case0_TestCase0) {
  Vector1RegisterImmediate_MOVTester_Case0 baseline_tester;
  NamedActual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1_VMOV_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001m1d000mmmddddcccc0qp1mmmm");
}

// op(5)=0 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_BITTester_Case1_TestCase1) {
  Vector1RegisterImmediate_BITTester_Case1 baseline_tester;
  NamedActual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1_VORR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001i1d000mmmddddcccc0q01mmmm");
}

// op(5)=0 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MOVTester_Case2_TestCase2) {
  Vector1RegisterImmediate_MOVTester_Case2 baseline_tester;
  NamedActual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1_VMOV_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001m1d000mmmddddcccc0qp1mmmm");
}

// op(5)=0 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//       rule: VORR_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_BITTester_Case3_TestCase3) {
  Vector1RegisterImmediate_BITTester_Case3 baseline_tester;
  NamedActual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1_VORR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001i1d000mmmddddcccc0q01mmmm");
}

// op(5)=0 & cmode(11:8)=11xx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MOVTester_Case4_TestCase4) {
  Vector1RegisterImmediate_MOVTester_Case4 baseline_tester;
  NamedActual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1_VMOV_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001m1d000mmmddddcccc0qp1mmmm");
}

// op(5)=1 & cmode(11:8)=1110
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MOV,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6), op(5)],
//       generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//       op: op(5),
//       pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//       rule: VMOV_immediate_A1,
//       safety: [op(5)=0 &&
//            cmode(0)=1 &&
//            cmode(3:2)=~11 => DECODER_ERROR,
//         op(5)=1 &&
//            cmode(11:8)=~1110 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MOVTester_Case5_TestCase5) {
  Vector1RegisterImmediate_MOVTester_Case5 baseline_tester;
  NamedActual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1_VMOV_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001m1d000mmmddddcccc0qp1mmmm");
}

// op(5)=1 & cmode(11:8)=10x0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MVNTester_Case6_TestCase6) {
  Vector1RegisterImmediate_MVNTester_Case6 baseline_tester;
  NamedActual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1_VMVN_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

// op(5)=1 & cmode(11:8)=10x1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_BITTester_Case7_TestCase7) {
  Vector1RegisterImmediate_BITTester_Case7 baseline_tester;
  NamedActual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1_VBIC_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

// op(5)=1 & cmode(11:8)=110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MVNTester_Case8_TestCase8) {
  Vector1RegisterImmediate_MVNTester_Case8 baseline_tester;
  NamedActual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1_VMVN_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

// op(5)=1 & cmode(11:8)=0xx0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_MVN,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VMVN_immediate,
//       safety: [(cmode(0)=1 &&
//            cmode(3:2)=~11) ||
//            cmode(3:1)=111 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_MVNTester_Case9_TestCase9) {
  Vector1RegisterImmediate_MVNTester_Case9 baseline_tester;
  NamedActual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1_VMVN_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

// op(5)=1 & cmode(11:8)=0xx1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//       baseline: Vector1RegisterImmediate_BIT,
//       cmode: cmode(11:8),
//       constraints: ,
//       defs: {},
//       fields: [Vd(15:12), cmode(11:8), Q(6)],
//       generated_baseline: VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//       pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//       rule: VBIC_immediate,
//       safety: [cmode(0)=0 ||
//            cmode(3:2)=11 => DECODER_ERROR,
//         Q(6)=1 &&
//            Vd(0)=1 => UNDEFINED],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Vector1RegisterImmediate_BITTester_Case10_TestCase10) {
  Vector1RegisterImmediate_BITTester_Case10 baseline_tester;
  NamedActual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1_VBIC_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111001i1d000mmmddddcccc0q11mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
