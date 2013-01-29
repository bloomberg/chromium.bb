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


// op1(27:20)=11000100
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCRR2_111111000100ssssttttiiiiiiiiiiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~11000100
  if ((inst.Bits() & 0x0FF00000)  !=
          0x0C400000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=11000101
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRRC2_111111000101ssssttttiiiiiiiiiiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~11000101
  if ((inst.Bits() & 0x0FF00000)  !=
          0x0C500000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: RFE_1111100pu0w1nnnn0000101000000000_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase2
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~100xx0x1
  if ((inst.Bits() & 0x0E500000)  !=
          0x08100000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000101000000000
  if ((inst.Bits() & 0x0000FFFF)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SRS_1111100pu1w0110100000101000iiiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase3
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~100xx1x0
  if ((inst.Bits() & 0x0E500000)  !=
          0x08400000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx110100000101000xxxxx
  if ((inst.Bits() & 0x000FFFE0)  !=
          0x000D0500) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=1110xxx0 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCR2_11111110iii0iiiittttiiiiiii1iiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase4
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase4(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~1110xxx0
  if ((inst.Bits() & 0x0F100000)  !=
          0x0E000000) return false;
  // op(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=1110xxx1 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRC2_11111110iii1iiiittttiiiiiii1iiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase5
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase5(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~1110xxx1
  if ((inst.Bits() & 0x0F100000)  !=
          0x0E100000) return false;
  // op(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: STC2_1111110pudw0nnnniiiiiiiiiiiiiiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase6
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase6(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~110xxxx0
  if ((inst.Bits() & 0x0E100000)  !=
          0x0C000000) return false;
  // op1_repeated(27:20)=11000x00
  if ((inst.Bits() & 0x0FB00000)  ==
          0x0C000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC2_immediate_1111110pudw1nnnniiiiiiiiiiiiiiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase7
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase7(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~110xxxx1
  if ((inst.Bits() & 0x0E100000)  !=
          0x0C100000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // op1_repeated(27:20)=11000x01
  if ((inst.Bits() & 0x0FB00000)  ==
          0x0C100000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC2_literal_1111110pudw11111iiiiiiiiiiiiiiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase8
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase8(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~110xxxx1
  if ((inst.Bits() & 0x0E100000)  !=
          0x0C100000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // op1_repeated(27:20)=11000x01
  if ((inst.Bits() & 0x0FB00000)  ==
          0x0C100000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=1110xxxx & op(4)=0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CDP2_11111110iiiiiiiiiiiiiiiiiii0iiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase9
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~1110xxxx
  if ((inst.Bits() & 0x0F000000)  !=
          0x0E000000) return false;
  // op(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(27:20)=101xxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase10
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(27:20)=~101xxxxx
  if ((inst.Bits() & 0x0E000000)  !=
          0x0A000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op1(27:20)=11000100
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCRR2_111111000100ssssttttiiiiiiiiiiii_case_0,
//       rule: MCRR2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case0
    : public UnsafeCondDecoderTesterCase0 {
 public:
  ForbiddenTester_Case0()
    : UnsafeCondDecoderTesterCase0(
      state_.Forbidden_MCRR2_instance_)
  {}
};

// op1(27:20)=11000101
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRRC2_111111000101ssssttttiiiiiiiiiiii_case_0,
//       rule: MRRC2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.Forbidden_MRRC2_instance_)
  {}
};

// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: RFE_1111100pu0w1nnnn0000101000000000_case_0,
//       rule: RFE,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.Forbidden_RFE_instance_)
  {}
};

// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SRS_1111100pu1w0110100000101000iiiii_case_0,
//       rule: SRS,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.Forbidden_SRS_instance_)
  {}
};

// op1(27:20)=1110xxx0 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCR2_11111110iii0iiiittttiiiiiii1iiii_case_0,
//       rule: MCR2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case4
    : public UnsafeCondDecoderTesterCase4 {
 public:
  ForbiddenTester_Case4()
    : UnsafeCondDecoderTesterCase4(
      state_.Forbidden_MCR2_instance_)
  {}
};

// op1(27:20)=1110xxx1 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRC2_11111110iii1iiiittttiiiiiii1iiii_case_0,
//       rule: MRC2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case5
    : public UnsafeCondDecoderTesterCase5 {
 public:
  ForbiddenTester_Case5()
    : UnsafeCondDecoderTesterCase5(
      state_.Forbidden_MRC2_instance_)
  {}
};

// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: STC2_1111110pudw0nnnniiiiiiiiiiiiiiii_case_0,
//       rule: STC2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case6
    : public UnsafeCondDecoderTesterCase6 {
 public:
  ForbiddenTester_Case6()
    : UnsafeCondDecoderTesterCase6(
      state_.Forbidden_STC2_instance_)
  {}
};

// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC2_immediate_1111110pudw1nnnniiiiiiiiiiiiiiii_case_0,
//       rule: LDC2_immediate,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case7
    : public UnsafeCondDecoderTesterCase7 {
 public:
  ForbiddenTester_Case7()
    : UnsafeCondDecoderTesterCase7(
      state_.Forbidden_LDC2_immediate_instance_)
  {}
};

// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC2_literal_1111110pudw11111iiiiiiiiiiiiiiii_case_0,
//       rule: LDC2_literal,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case8
    : public UnsafeCondDecoderTesterCase8 {
 public:
  ForbiddenTester_Case8()
    : UnsafeCondDecoderTesterCase8(
      state_.Forbidden_LDC2_literal_instance_)
  {}
};

// op1(27:20)=1110xxxx & op(4)=0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CDP2_11111110iiiiiiiiiiiiiiiiiii0iiii_case_0,
//       rule: CDP2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.Forbidden_CDP2_instance_)
  {}
};

// op1(27:20)=101xxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       rule: BLX_immediate,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case10
    : public UnsafeCondDecoderTesterCase10 {
 public:
  ForbiddenTester_Case10()
    : UnsafeCondDecoderTesterCase10(
      state_.Forbidden_BLX_immediate_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op1(27:20)=11000100
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCRR2_111111000100ssssttttiiiiiiiiiiii_case_0,
//       pattern: 111111000100ssssttttiiiiiiiiiiii,
//       rule: MCRR2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case0_TestCase0) {
  ForbiddenTester_Case0 tester;
  tester.Test("111111000100ssssttttiiiiiiiiiiii");
}

// op1(27:20)=11000101
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRRC2_111111000101ssssttttiiiiiiiiiiii_case_0,
//       pattern: 111111000101ssssttttiiiiiiiiiiii,
//       rule: MRRC2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case1_TestCase1) {
  ForbiddenTester_Case1 tester;
  tester.Test("111111000101ssssttttiiiiiiiiiiii");
}

// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: RFE_1111100pu0w1nnnn0000101000000000_case_0,
//       pattern: 1111100pu0w1nnnn0000101000000000,
//       rule: RFE,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case2_TestCase2) {
  ForbiddenTester_Case2 tester;
  tester.Test("1111100pu0w1nnnn0000101000000000");
}

// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SRS_1111100pu1w0110100000101000iiiii_case_0,
//       pattern: 1111100pu1w0110100000101000iiiii,
//       rule: SRS,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case3_TestCase3) {
  ForbiddenTester_Case3 tester;
  tester.Test("1111100pu1w0110100000101000iiiii");
}

// op1(27:20)=1110xxx0 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCR2_11111110iii0iiiittttiiiiiii1iiii_case_0,
//       pattern: 11111110iii0iiiittttiiiiiii1iiii,
//       rule: MCR2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case4_TestCase4) {
  ForbiddenTester_Case4 tester;
  tester.Test("11111110iii0iiiittttiiiiiii1iiii");
}

// op1(27:20)=1110xxx1 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRC2_11111110iii1iiiittttiiiiiii1iiii_case_0,
//       pattern: 11111110iii1iiiittttiiiiiii1iiii,
//       rule: MRC2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case5_TestCase5) {
  ForbiddenTester_Case5 tester;
  tester.Test("11111110iii1iiiittttiiiiiii1iiii");
}

// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: STC2_1111110pudw0nnnniiiiiiiiiiiiiiii_case_0,
//       pattern: 1111110pudw0nnnniiiiiiiiiiiiiiii,
//       rule: STC2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case6_TestCase6) {
  ForbiddenTester_Case6 tester;
  tester.Test("1111110pudw0nnnniiiiiiiiiiiiiiii");
}

// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC2_immediate_1111110pudw1nnnniiiiiiiiiiiiiiii_case_0,
//       pattern: 1111110pudw1nnnniiiiiiiiiiiiiiii,
//       rule: LDC2_immediate,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case7_TestCase7) {
  ForbiddenTester_Case7 tester;
  tester.Test("1111110pudw1nnnniiiiiiiiiiiiiiii");
}

// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC2_literal_1111110pudw11111iiiiiiiiiiiiiiii_case_0,
//       pattern: 1111110pudw11111iiiiiiiiiiiiiiii,
//       rule: LDC2_literal,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case8_TestCase8) {
  ForbiddenTester_Case8 tester;
  tester.Test("1111110pudw11111iiiiiiiiiiiiiiii");
}

// op1(27:20)=1110xxxx & op(4)=0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CDP2_11111110iiiiiiiiiiiiiiiiiii0iiii_case_0,
//       pattern: 11111110iiiiiiiiiiiiiiiiiii0iiii,
//       rule: CDP2,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case9_TestCase9) {
  ForbiddenTester_Case9 tester;
  tester.Test("11111110iiiiiiiiiiiiiiiiiii0iiii");
}

// op1(27:20)=101xxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       pattern: 1111101hiiiiiiiiiiiiiiiiiiiiiiii,
//       rule: BLX_immediate,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case10_TestCase10) {
  ForbiddenTester_Case10 tester;
  tester.Test("1111101hiiiiiiiiiiiiiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
