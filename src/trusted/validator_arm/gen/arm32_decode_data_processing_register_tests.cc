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


// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0,
//       uses: {Rn, Rm}}
class Binary2RegisterImmedShiftedTestTesterCase0
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase0(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10001
  if ((inst.Bits() & 0x01F00000)  !=
          0x01100000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmedShiftedTestTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmedShiftedTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0,
//       uses: {Rn, Rm}}
class Binary2RegisterImmedShiftedTestTesterCase1
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase1(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10011
  if ((inst.Bits() & 0x01F00000)  !=
          0x01300000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmedShiftedTestTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmedShiftedTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0,
//       uses: {Rn, Rm}}
class Binary2RegisterImmedShiftedTestTesterCase2
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase2(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10101
  if ((inst.Bits() & 0x01F00000)  !=
          0x01500000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmedShiftedTestTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmedShiftedTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0,
//       uses: {Rn, Rm}}
class Binary2RegisterImmedShiftedTestTesterCase3
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase3(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10111
  if ((inst.Bits() & 0x01F00000)  !=
          0x01700000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmedShiftedTestTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmedShiftedTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase4
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0000x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase5
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase5(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0001x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase6
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase6(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0010x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00400000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase7
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase7(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0011x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00600000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase8
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00800000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase9
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase9(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00A00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase10
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase10(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0110x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00C00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase11
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0111x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00E00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase12
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase12(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01800000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       generated_baseline: LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0,
//       imm5: imm5(11:7),
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpTesterCase13
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase13(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=00000
  if ((inst.Bits() & 0x00000F80)  ==
          0x00000000) return false;
  // op3(6:5)=~00
  if ((inst.Bits() & 0x00000060)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: imm5(11:7)=00000 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00000F80)  !=
          0x00000000);

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       generated_baseline: ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0,
//       imm5: imm5(11:7),
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpTesterCase14
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase14(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=00000
  if ((inst.Bits() & 0x00000F80)  ==
          0x00000000) return false;
  // op3(6:5)=~11
  if ((inst.Bits() & 0x00000060)  !=
          0x00000060) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: imm5(11:7)=00000 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00000F80)  !=
          0x00000000);

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: MOV_register_cccc0001101s0000dddd00000000mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterOpTesterCase15
    : public Unary2RegisterOpTester {
 public:
  Unary2RegisterOpTesterCase15(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=~00000
  if ((inst.Bits() & 0x00000F80)  !=
          0x00000000) return false;
  // op3(6:5)=~00
  if ((inst.Bits() & 0x00000060)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: RRX_cccc0001101s0000dddd00000110mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterOpTesterCase16
    : public Unary2RegisterOpTester {
 public:
  Unary2RegisterOpTesterCase16(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=~00000
  if ((inst.Bits() & 0x00000F80)  !=
          0x00000000) return false;
  // op3(6:5)=~11
  if ((inst.Bits() & 0x00000060)  !=
          0x00000060) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpTesterCase17
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase17(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op3(6:5)=~01
  if ((inst.Bits() & 0x00000060)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpTesterCase18
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase18(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op3(6:5)=~10
  if ((inst.Bits() & 0x00000060)  !=
          0x00000040) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTesterCase19
    : public Binary3RegisterShiftedOpTester {
 public:
  Binary3RegisterShiftedOpTesterCase19(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedOpTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1110x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01C00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedOpTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpTesterCase20
    : public Unary2RegisterShiftedOpTester {
 public:
  Unary2RegisterShiftedOpTesterCase20(const NamedClassDecoder& decoder)
    : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterShiftedOpTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1111x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01E00000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterShiftedOpTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterShiftedOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if S
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0,
//       rule: TST_register,
//       uses: {Rn, Rm}}
class Binary2RegisterImmedShiftedTestTester_Case0
    : public Binary2RegisterImmedShiftedTestTesterCase0 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case0()
    : Binary2RegisterImmedShiftedTestTesterCase0(
      state_.Binary2RegisterImmedShiftedTest_TST_register_instance_)
  {}
};

// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0,
//       rule: TEQ_register,
//       uses: {Rn, Rm}}
class Binary2RegisterImmedShiftedTestTester_Case1
    : public Binary2RegisterImmedShiftedTestTesterCase1 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case1()
    : Binary2RegisterImmedShiftedTestTesterCase1(
      state_.Binary2RegisterImmedShiftedTest_TEQ_register_instance_)
  {}
};

// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0,
//       rule: CMP_register,
//       uses: {Rn, Rm}}
class Binary2RegisterImmedShiftedTestTester_Case2
    : public Binary2RegisterImmedShiftedTestTesterCase2 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case2()
    : Binary2RegisterImmedShiftedTestTesterCase2(
      state_.Binary2RegisterImmedShiftedTest_CMP_register_instance_)
  {}
};

// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0,
//       rule: CMN_register,
//       uses: {Rn, Rm}}
class Binary2RegisterImmedShiftedTestTester_Case3
    : public Binary2RegisterImmedShiftedTestTesterCase3 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case3()
    : Binary2RegisterImmedShiftedTestTesterCase3(
      state_.Binary2RegisterImmedShiftedTest_CMN_register_instance_)
  {}
};

// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0,
//       rule: AND_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case4
    : public Binary3RegisterShiftedOpTesterCase4 {
 public:
  Binary3RegisterShiftedOpTester_Case4()
    : Binary3RegisterShiftedOpTesterCase4(
      state_.Binary3RegisterShiftedOp_AND_register_instance_)
  {}
};

// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0,
//       rule: EOR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case5
    : public Binary3RegisterShiftedOpTesterCase5 {
 public:
  Binary3RegisterShiftedOpTester_Case5()
    : Binary3RegisterShiftedOpTesterCase5(
      state_.Binary3RegisterShiftedOp_EOR_register_instance_)
  {}
};

// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0,
//       rule: SUB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case6
    : public Binary3RegisterShiftedOpTesterCase6 {
 public:
  Binary3RegisterShiftedOpTester_Case6()
    : Binary3RegisterShiftedOpTesterCase6(
      state_.Binary3RegisterShiftedOp_SUB_register_instance_)
  {}
};

// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0,
//       rule: RSB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case7
    : public Binary3RegisterShiftedOpTesterCase7 {
 public:
  Binary3RegisterShiftedOpTester_Case7()
    : Binary3RegisterShiftedOpTesterCase7(
      state_.Binary3RegisterShiftedOp_RSB_register_instance_)
  {}
};

// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0,
//       rule: ADD_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case8
    : public Binary3RegisterShiftedOpTesterCase8 {
 public:
  Binary3RegisterShiftedOpTester_Case8()
    : Binary3RegisterShiftedOpTesterCase8(
      state_.Binary3RegisterShiftedOp_ADD_register_instance_)
  {}
};

// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0,
//       rule: ADC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case9
    : public Binary3RegisterShiftedOpTesterCase9 {
 public:
  Binary3RegisterShiftedOpTester_Case9()
    : Binary3RegisterShiftedOpTesterCase9(
      state_.Binary3RegisterShiftedOp_ADC_register_instance_)
  {}
};

// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0,
//       rule: SBC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case10
    : public Binary3RegisterShiftedOpTesterCase10 {
 public:
  Binary3RegisterShiftedOpTester_Case10()
    : Binary3RegisterShiftedOpTesterCase10(
      state_.Binary3RegisterShiftedOp_SBC_register_instance_)
  {}
};

// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0,
//       rule: RSC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case11
    : public Binary3RegisterShiftedOpTesterCase11 {
 public:
  Binary3RegisterShiftedOpTester_Case11()
    : Binary3RegisterShiftedOpTesterCase11(
      state_.Binary3RegisterShiftedOp_RSC_register_instance_)
  {}
};

// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0,
//       rule: ORR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case12
    : public Binary3RegisterShiftedOpTesterCase12 {
 public:
  Binary3RegisterShiftedOpTester_Case12()
    : Binary3RegisterShiftedOpTesterCase12(
      state_.Binary3RegisterShiftedOp_ORR_register_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       generated_baseline: LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0,
//       imm5: imm5(11:7),
//       rule: LSL_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpImmNotZeroTester_Case13
    : public Unary2RegisterShiftedOpTesterCase13 {
 public:
  Unary2RegisterShiftedOpImmNotZeroTester_Case13()
    : Unary2RegisterShiftedOpTesterCase13(
      state_.Unary2RegisterShiftedOpImmNotZero_LSL_immediate_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       generated_baseline: ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0,
//       imm5: imm5(11:7),
//       rule: ROR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpImmNotZeroTester_Case14
    : public Unary2RegisterShiftedOpTesterCase14 {
 public:
  Unary2RegisterShiftedOpImmNotZeroTester_Case14()
    : Unary2RegisterShiftedOpTesterCase14(
      state_.Unary2RegisterShiftedOpImmNotZero_ROR_immediate_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: MOV_register_cccc0001101s0000dddd00000000mmmm_case_0,
//       rule: MOV_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterOpTester_Case15
    : public Unary2RegisterOpTesterCase15 {
 public:
  Unary2RegisterOpTester_Case15()
    : Unary2RegisterOpTesterCase15(
      state_.Unary2RegisterOp_MOV_register_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: RRX_cccc0001101s0000dddd00000110mmmm_case_0,
//       rule: RRX,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterOpTester_Case16
    : public Unary2RegisterOpTesterCase16 {
 public:
  Unary2RegisterOpTester_Case16()
    : Unary2RegisterOpTesterCase16(
      state_.Unary2RegisterOp_RRX_instance_)
  {}
};

// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0,
//       rule: LSR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpTester_Case17
    : public Unary2RegisterShiftedOpTesterCase17 {
 public:
  Unary2RegisterShiftedOpTester_Case17()
    : Unary2RegisterShiftedOpTesterCase17(
      state_.Unary2RegisterShiftedOp_LSR_immediate_instance_)
  {}
};

// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0,
//       rule: ASR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpTester_Case18
    : public Unary2RegisterShiftedOpTesterCase18 {
 public:
  Unary2RegisterShiftedOpTester_Case18()
    : Unary2RegisterShiftedOpTesterCase18(
      state_.Unary2RegisterShiftedOp_ASR_immediate_instance_)
  {}
};

// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0,
//       rule: BIC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class Binary3RegisterShiftedOpTester_Case19
    : public Binary3RegisterShiftedOpTesterCase19 {
 public:
  Binary3RegisterShiftedOpTester_Case19()
    : Binary3RegisterShiftedOpTesterCase19(
      state_.Binary3RegisterShiftedOp_BIC_register_instance_)
  {}
};

// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0,
//       rule: MVN_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class Unary2RegisterShiftedOpTester_Case20
    : public Unary2RegisterShiftedOpTesterCase20 {
 public:
  Unary2RegisterShiftedOpTester_Case20()
    : Unary2RegisterShiftedOpTesterCase20(
      state_.Unary2RegisterShiftedOp_MVN_register_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0,
//       pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//       rule: TST_register,
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case0_TestCase0) {
  Binary2RegisterImmedShiftedTestTester_Case0 baseline_tester;
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TST_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0,
//       pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//       rule: TEQ_register,
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case1_TestCase1) {
  Binary2RegisterImmedShiftedTestTester_Case1 baseline_tester;
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TEQ_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0,
//       pattern: cccc00010101nnnn0000iiiiitt0mmmm,
//       rule: CMP_register,
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case2_TestCase2) {
  Binary2RegisterImmedShiftedTestTester_Case2 baseline_tester;
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMP_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: Binary2RegisterImmedShiftedTest,
//       constraints: ,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       generated_baseline: CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0,
//       pattern: cccc00010111nnnn0000iiiiitt0mmmm,
//       rule: CMN_register,
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case3_TestCase3) {
  Binary2RegisterImmedShiftedTestTester_Case3 baseline_tester;
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMN_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0000000snnnnddddiiiiitt0mmmm,
//       rule: AND_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case4_TestCase4) {
  Binary3RegisterShiftedOpTester_Case4 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_AND_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0000001snnnnddddiiiiitt0mmmm,
//       rule: EOR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case5_TestCase5) {
  Binary3RegisterShiftedOpTester_Case5 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_EOR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0000010snnnnddddiiiiitt0mmmm,
//       rule: SUB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case6_TestCase6) {
  Binary3RegisterShiftedOpTester_Case6 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SUB_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0000011snnnnddddiiiiitt0mmmm,
//       rule: RSB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case7_TestCase7) {
  Binary3RegisterShiftedOpTester_Case7 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSB_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0000100snnnnddddiiiiitt0mmmm,
//       rule: ADD_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case8_TestCase8) {
  Binary3RegisterShiftedOpTester_Case8 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADD_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0000101snnnnddddiiiiitt0mmmm,
//       rule: ADC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case9_TestCase9) {
  Binary3RegisterShiftedOpTester_Case9 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0000110snnnnddddiiiiitt0mmmm,
//       rule: SBC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case10_TestCase10) {
  Binary3RegisterShiftedOpTester_Case10 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SBC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0000111snnnnddddiiiiitt0mmmm,
//       rule: RSC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case11_TestCase11) {
  Binary3RegisterShiftedOpTester_Case11 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0001100snnnnddddiiiiitt0mmmm,
//       rule: ORR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case12_TestCase12) {
  Binary3RegisterShiftedOpTester_Case12 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ORR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       generated_baseline: LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0,
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii000mmmm,
//       rule: LSL_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpImmNotZeroTester_Case13_TestCase13) {
  Unary2RegisterShiftedOpImmNotZeroTester_Case13 baseline_tester;
  NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_LSL_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddiiiii000mmmm");
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: Unary2RegisterShiftedOpImmNotZero,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       generated_baseline: ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0,
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii110mmmm,
//       rule: ROR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpImmNotZeroTester_Case14_TestCase14) {
  Unary2RegisterShiftedOpImmNotZeroTester_Case14 baseline_tester;
  NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_ROR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddiiiii110mmmm");
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: MOV_register_cccc0001101s0000dddd00000000mmmm_case_0,
//       pattern: cccc0001101s0000dddd00000000mmmm,
//       rule: MOV_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Case15_TestCase15) {
  Unary2RegisterOpTester_Case15 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MOV_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000dddd00000000mmmm");
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: RRX_cccc0001101s0000dddd00000110mmmm_case_0,
//       pattern: cccc0001101s0000dddd00000110mmmm,
//       rule: RRX,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Case16_TestCase16) {
  Unary2RegisterOpTester_Case16 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_RRX actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000dddd00000110mmmm");
}

// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0,
//       pattern: cccc0001101s0000ddddiiiii010mmmm,
//       rule: LSR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpTester_Case17_TestCase17) {
  Unary2RegisterShiftedOpTester_Case17 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_LSR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddiiiii010mmmm");
}

// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0,
//       pattern: cccc0001101s0000ddddiiiii100mmmm,
//       rule: ASR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpTester_Case18_TestCase18) {
  Unary2RegisterShiftedOpTester_Case18 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_ASR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddiiiii100mmmm");
}

// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: Binary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       generated_baseline: BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0,
//       pattern: cccc0001110snnnnddddiiiiitt0mmmm,
//       rule: BIC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedOpTester_Case19_TestCase19) {
  Binary3RegisterShiftedOpTester_Case19 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_BIC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: Unary2RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       generated_baseline: MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0,
//       pattern: cccc0001111s0000ddddiiiiitt0mmmm,
//       rule: MVN_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterShiftedOpTester_Case20_TestCase20) {
  Unary2RegisterShiftedOpTester_Case20 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MVN_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111s0000ddddiiiiitt0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
