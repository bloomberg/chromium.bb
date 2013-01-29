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


// opc1(23:20)=0x00
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VMLA_VMLS_floating_point_cccc11100d00nnnndddd101snom0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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
  // opc1(23:20)=~0x00
  if ((inst.Bits() & 0x00B00000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc1(23:20)=0x01
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VNMLA_VNMLS_cccc11100d01nnnndddd101snom0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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
  // opc1(23:20)=~0x01
  if ((inst.Bits() & 0x00B00000)  !=
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VMUL_floating_point_cccc11100d10nnnndddd101sn0m0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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

bool CondVfpOpTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VNMUL_cccc11100d10nnnndddd101sn1m0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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

bool CondVfpOpTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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

bool CondVfpOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VSUB_floating_point_cccc11100d11nnnndddd101sn1m0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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

bool CondVfpOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VDIV_cccc11101d00nnnndddd101sn0m0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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

bool CondVfpOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc1(23:20)=1x01
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VFNMA_VFNMS_cccc11101d01nnnndddd101snom0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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
  // opc1(23:20)=~1x01
  if ((inst.Bits() & 0x00B00000)  !=
          0x00900000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// opc1(23:20)=1x10
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VFMA_VFMS_cccc11101d10nnnndddd101snom0mmmm_case_0,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
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
  // opc1(23:20)=~1x10
  if ((inst.Bits() & 0x00B00000)  !=
          0x00A00000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool CondVfpOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(CondVfpOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  !=
          0xF0000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// opc1(23:20)=0x00
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VMLA_VMLS_floating_point_cccc11100d00nnnndddd101snom0mmmm_case_0,
//       rule: VMLA_VMLS_floating_point,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case0
    : public CondVfpOpTesterCase0 {
 public:
  CondVfpOpTester_Case0()
    : CondVfpOpTesterCase0(
      state_.CondVfpOp_VMLA_VMLS_floating_point_instance_)
  {}
};

// opc1(23:20)=0x01
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VNMLA_VNMLS_cccc11100d01nnnndddd101snom0mmmm_case_0,
//       rule: VNMLA_VNMLS,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case1
    : public CondVfpOpTesterCase1 {
 public:
  CondVfpOpTester_Case1()
    : CondVfpOpTesterCase1(
      state_.CondVfpOp_VNMLA_VNMLS_instance_)
  {}
};

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VMUL_floating_point_cccc11100d10nnnndddd101sn0m0mmmm_case_0,
//       rule: VMUL_floating_point,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case2
    : public CondVfpOpTesterCase2 {
 public:
  CondVfpOpTester_Case2()
    : CondVfpOpTesterCase2(
      state_.CondVfpOp_VMUL_floating_point_instance_)
  {}
};

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VNMUL_cccc11100d10nnnndddd101sn1m0mmmm_case_0,
//       rule: VNMUL,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case3
    : public CondVfpOpTesterCase3 {
 public:
  CondVfpOpTester_Case3()
    : CondVfpOpTesterCase3(
      state_.CondVfpOp_VNMUL_instance_)
  {}
};

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_0,
//       rule: VADD_floating_point,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case4
    : public CondVfpOpTesterCase4 {
 public:
  CondVfpOpTester_Case4()
    : CondVfpOpTesterCase4(
      state_.CondVfpOp_VADD_floating_point_instance_)
  {}
};

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VSUB_floating_point_cccc11100d11nnnndddd101sn1m0mmmm_case_0,
//       rule: VSUB_floating_point,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case5
    : public CondVfpOpTesterCase5 {
 public:
  CondVfpOpTester_Case5()
    : CondVfpOpTesterCase5(
      state_.CondVfpOp_VSUB_floating_point_instance_)
  {}
};

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VDIV_cccc11101d00nnnndddd101sn0m0mmmm_case_0,
//       rule: VDIV,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case6
    : public CondVfpOpTesterCase6 {
 public:
  CondVfpOpTester_Case6()
    : CondVfpOpTesterCase6(
      state_.CondVfpOp_VDIV_instance_)
  {}
};

// opc1(23:20)=1x01
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VFNMA_VFNMS_cccc11101d01nnnndddd101snom0mmmm_case_0,
//       rule: VFNMA_VFNMS,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case7
    : public CondVfpOpTesterCase7 {
 public:
  CondVfpOpTester_Case7()
    : CondVfpOpTesterCase7(
      state_.CondVfpOp_VFNMA_VFNMS_instance_)
  {}
};

// opc1(23:20)=1x10
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VFMA_VFMS_cccc11101d10nnnndddd101snom0mmmm_case_0,
//       rule: VFMA_VFMS,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
class CondVfpOpTester_Case8
    : public CondVfpOpTesterCase8 {
 public:
  CondVfpOpTester_Case8()
    : CondVfpOpTesterCase8(
      state_.CondVfpOp_VFMA_VFMS_instance_)
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
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VMLA_VMLS_floating_point_cccc11100d00nnnndddd101snom0mmmm_case_0,
//       pattern: cccc11100d00nnnndddd101snom0mmmm,
//       rule: VMLA_VMLS_floating_point,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case0_TestCase0) {
  CondVfpOpTester_Case0 tester;
  tester.Test("cccc11100d00nnnndddd101snom0mmmm");
}

// opc1(23:20)=0x01
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VNMLA_VNMLS_cccc11100d01nnnndddd101snom0mmmm_case_0,
//       pattern: cccc11100d01nnnndddd101snom0mmmm,
//       rule: VNMLA_VNMLS,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case1_TestCase1) {
  CondVfpOpTester_Case1 tester;
  tester.Test("cccc11100d01nnnndddd101snom0mmmm");
}

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VMUL_floating_point_cccc11100d10nnnndddd101sn0m0mmmm_case_0,
//       pattern: cccc11100d10nnnndddd101sn0m0mmmm,
//       rule: VMUL_floating_point,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case2_TestCase2) {
  CondVfpOpTester_Case2 tester;
  tester.Test("cccc11100d10nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VNMUL_cccc11100d10nnnndddd101sn1m0mmmm_case_0,
//       pattern: cccc11100d10nnnndddd101sn1m0mmmm,
//       rule: VNMUL,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case3_TestCase3) {
  CondVfpOpTester_Case3 tester;
  tester.Test("cccc11100d10nnnndddd101sn1m0mmmm");
}

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_0,
//       pattern: cccc11100d11nnnndddd101sn0m0mmmm,
//       rule: VADD_floating_point,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case4_TestCase4) {
  CondVfpOpTester_Case4 tester;
  tester.Test("cccc11100d11nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VSUB_floating_point_cccc11100d11nnnndddd101sn1m0mmmm_case_0,
//       pattern: cccc11100d11nnnndddd101sn1m0mmmm,
//       rule: VSUB_floating_point,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case5_TestCase5) {
  CondVfpOpTester_Case5 tester;
  tester.Test("cccc11100d11nnnndddd101sn1m0mmmm");
}

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VDIV_cccc11101d00nnnndddd101sn0m0mmmm_case_0,
//       pattern: cccc11101d00nnnndddd101sn0m0mmmm,
//       rule: VDIV,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case6_TestCase6) {
  CondVfpOpTester_Case6 tester;
  tester.Test("cccc11101d00nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=1x01
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VFNMA_VFNMS_cccc11101d01nnnndddd101snom0mmmm_case_0,
//       pattern: cccc11101d01nnnndddd101snom0mmmm,
//       rule: VFNMA_VFNMS,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case7_TestCase7) {
  CondVfpOpTester_Case7 tester;
  tester.Test("cccc11101d01nnnndddd101snom0mmmm");
}

// opc1(23:20)=1x10
//    = {actual: CondVfpOp,
//       baseline: CondVfpOp,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       generated_baseline: VFMA_VFMS_cccc11101d10nnnndddd101snom0mmmm_case_0,
//       pattern: cccc11101d10nnnndddd101snom0mmmm,
//       rule: VFMA_VFMS,
//       safety: [cond(31:28)=1111 => DECODER_ERROR],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case8_TestCase8) {
  CondVfpOpTester_Case8 tester;
  tester.Test("cccc11101d10nnnndddd101snom0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
