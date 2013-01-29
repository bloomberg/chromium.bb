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


// coproc(11:8)=~101x & op1(25:20)=000100
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCRR_cccc11000100ttttttttccccoooommmm_case_0,
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
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~000100
  if ((inst.Bits() & 0x03F00000)  !=
          0x00400000) return false;

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

// coproc(11:8)=~101x & op1(25:20)=000101
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRRC_cccc11000101ttttttttccccoooommmm_case_0,
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
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~000101
  if ((inst.Bits() & 0x03F00000)  !=
          0x00500000) return false;

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

// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCR_cccc1110ooo0nnnnttttccccooo1mmmm_case_0,
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
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~10xxx0
  if ((inst.Bits() & 0x03100000)  !=
          0x02000000) return false;
  // op(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

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

// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRC_cccc1110ooo1nnnnttttccccooo1mmmm_case_0,
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
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~10xxx1
  if ((inst.Bits() & 0x03100000)  !=
          0x02100000) return false;
  // op(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

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

// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: STC_cccc110pudw0nnnnddddcccciiiiiiii_case_0,
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
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~0xxxx0
  if ((inst.Bits() & 0x02100000)  !=
          0x00000000) return false;
  // op1_repeated(25:20)=000x00
  if ((inst.Bits() & 0x03B00000)  ==
          0x00000000) return false;

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

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC_immediate_cccc110pudw1nnnnddddcccciiiiiiii_case_0,
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
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~0xxxx1
  if ((inst.Bits() & 0x02100000)  !=
          0x00100000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // op1_repeated(25:20)=000x01
  if ((inst.Bits() & 0x03B00000)  ==
          0x00100000) return false;

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

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC_literal_cccc110pudw11111ddddcccciiiiiiii_case_0,
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
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~0xxxx1
  if ((inst.Bits() & 0x02100000)  !=
          0x00100000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // op1_repeated(25:20)=000x01
  if ((inst.Bits() & 0x03B00000)  ==
          0x00100000) return false;

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

// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CDP_cccc1110oooonnnnddddccccooo0mmmm_case_0,
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
  // coproc(11:8)=101x
  if ((inst.Bits() & 0x00000E00)  ==
          0x00000A00) return false;
  // op1(25:20)=~10xxxx
  if ((inst.Bits() & 0x03000000)  !=
          0x02000000) return false;
  // op(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

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

// op1(25:20)=00000x
//    = {actual: Undefined,
//       baseline: Undefined,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_cccc1100000xnnnnxxxxccccxxxoxxxx_case_0,
//       safety: [true => UNDEFINED],
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
  // op1(25:20)=~00000x
  if ((inst.Bits() & 0x03E00000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNDEFINED
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(25:20)=11xxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SVC_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_case_0,
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
  // op1(25:20)=~11xxxx
  if ((inst.Bits() & 0x03000000)  !=
          0x03000000) return false;

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

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// coproc(11:8)=~101x & op1(25:20)=000100
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCRR_cccc11000100ttttttttccccoooommmm_case_0,
//       rule: MCRR,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case0
    : public UnsafeCondDecoderTesterCase0 {
 public:
  ForbiddenTester_Case0()
    : UnsafeCondDecoderTesterCase0(
      state_.Forbidden_MCRR_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=000101
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRRC_cccc11000101ttttttttccccoooommmm_case_0,
//       rule: MRRC,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.Forbidden_MRRC_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCR_cccc1110ooo0nnnnttttccccooo1mmmm_case_0,
//       rule: MCR,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.Forbidden_MCR_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRC_cccc1110ooo1nnnnttttccccooo1mmmm_case_0,
//       rule: MRC,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.Forbidden_MRC_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: STC_cccc110pudw0nnnnddddcccciiiiiiii_case_0,
//       rule: STC,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case4
    : public UnsafeCondDecoderTesterCase4 {
 public:
  ForbiddenTester_Case4()
    : UnsafeCondDecoderTesterCase4(
      state_.Forbidden_STC_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC_immediate_cccc110pudw1nnnnddddcccciiiiiiii_case_0,
//       rule: LDC_immediate,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case5
    : public UnsafeCondDecoderTesterCase5 {
 public:
  ForbiddenTester_Case5()
    : UnsafeCondDecoderTesterCase5(
      state_.Forbidden_LDC_immediate_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC_literal_cccc110pudw11111ddddcccciiiiiiii_case_0,
//       rule: LDC_literal,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case6
    : public UnsafeCondDecoderTesterCase6 {
 public:
  ForbiddenTester_Case6()
    : UnsafeCondDecoderTesterCase6(
      state_.Forbidden_LDC_literal_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CDP_cccc1110oooonnnnddddccccooo0mmmm_case_0,
//       rule: CDP,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case7
    : public UnsafeCondDecoderTesterCase7 {
 public:
  ForbiddenTester_Case7()
    : UnsafeCondDecoderTesterCase7(
      state_.Forbidden_CDP_instance_)
  {}
};

// op1(25:20)=00000x
//    = {actual: Undefined,
//       baseline: Undefined,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_cccc1100000xnnnnxxxxccccxxxoxxxx_case_0,
//       safety: [true => UNDEFINED],
//       true: true,
//       uses: {}}
class UndefinedTester_Case8
    : public UnsafeCondDecoderTesterCase8 {
 public:
  UndefinedTester_Case8()
    : UnsafeCondDecoderTesterCase8(
      state_.Undefined_None_instance_)
  {}
};

// op1(25:20)=11xxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SVC_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       rule: SVC,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.Forbidden_SVC_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// coproc(11:8)=~101x & op1(25:20)=000100
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCRR_cccc11000100ttttttttccccoooommmm_case_0,
//       pattern: cccc11000100ttttttttccccoooommmm,
//       rule: MCRR,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case0_TestCase0) {
  ForbiddenTester_Case0 tester;
  tester.Test("cccc11000100ttttttttccccoooommmm");
}

// coproc(11:8)=~101x & op1(25:20)=000101
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRRC_cccc11000101ttttttttccccoooommmm_case_0,
//       pattern: cccc11000101ttttttttccccoooommmm,
//       rule: MRRC,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case1_TestCase1) {
  ForbiddenTester_Case1 tester;
  tester.Test("cccc11000101ttttttttccccoooommmm");
}

// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MCR_cccc1110ooo0nnnnttttccccooo1mmmm_case_0,
//       pattern: cccc1110ooo0nnnnttttccccooo1mmmm,
//       rule: MCR,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case2_TestCase2) {
  ForbiddenTester_Case2 tester;
  tester.Test("cccc1110ooo0nnnnttttccccooo1mmmm");
}

// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: MRC_cccc1110ooo1nnnnttttccccooo1mmmm_case_0,
//       pattern: cccc1110ooo1nnnnttttccccooo1mmmm,
//       rule: MRC,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case3_TestCase3) {
  ForbiddenTester_Case3 tester;
  tester.Test("cccc1110ooo1nnnnttttccccooo1mmmm");
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: STC_cccc110pudw0nnnnddddcccciiiiiiii_case_0,
//       pattern: cccc110pudw0nnnnddddcccciiiiiiii,
//       rule: STC,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case4_TestCase4) {
  ForbiddenTester_Case4 tester;
  tester.Test("cccc110pudw0nnnnddddcccciiiiiiii");
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC_immediate_cccc110pudw1nnnnddddcccciiiiiiii_case_0,
//       pattern: cccc110pudw1nnnnddddcccciiiiiiii,
//       rule: LDC_immediate,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case5_TestCase5) {
  ForbiddenTester_Case5 tester;
  tester.Test("cccc110pudw1nnnnddddcccciiiiiiii");
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: LDC_literal_cccc110pudw11111ddddcccciiiiiiii_case_0,
//       pattern: cccc110pudw11111ddddcccciiiiiiii,
//       rule: LDC_literal,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case6_TestCase6) {
  ForbiddenTester_Case6 tester;
  tester.Test("cccc110pudw11111ddddcccciiiiiiii");
}

// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CDP_cccc1110oooonnnnddddccccooo0mmmm_case_0,
//       pattern: cccc1110oooonnnnddddccccooo0mmmm,
//       rule: CDP,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case7_TestCase7) {
  ForbiddenTester_Case7 tester;
  tester.Test("cccc1110oooonnnnddddccccooo0mmmm");
}

// op1(25:20)=00000x
//    = {actual: Undefined,
//       baseline: Undefined,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_cccc1100000xnnnnxxxxccccxxxoxxxx_case_0,
//       pattern: cccc1100000xnnnnxxxxccccxxxoxxxx,
//       safety: [true => UNDEFINED],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UndefinedTester_Case8_TestCase8) {
  UndefinedTester_Case8 tester;
  tester.Test("cccc1100000xnnnnxxxxccccxxxoxxxx");
}

// op1(25:20)=11xxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SVC_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       pattern: cccc1111iiiiiiiiiiiiiiiiiiiiiiii,
//       rule: SVC,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case9_TestCase9) {
  ForbiddenTester_Case9 tester;
  tester.Test("cccc1111iiiiiiiiiiiiiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
