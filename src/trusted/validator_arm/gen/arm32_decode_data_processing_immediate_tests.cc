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


// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1,
//       baseline: MaskedBinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), imm12(11:0)],
//       generated_baseline: TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       imm32: ARMExpandImm_C(imm12),
//       sets_Z_if_clear_bits: Rn  ==
//               RegIndex(test_register()) &&
//            (imm32 &&
//            clears_mask())  ==
//               clears_mask(),
//       uses: {Rn}}
class BinaryRegisterImmediateTestTesterCase0
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase0(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~10001
  if ((inst.Bits() & 0x01F00000)  !=
          0x01100000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool BinaryRegisterImmediateTestTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BinaryRegisterImmediateTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(16))));

  return true;
}

// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: TEQ_immediate_cccc00110011nnnn0000iiiiiiiiiiii_case_0,
//       uses: {Rn}}
class BinaryRegisterImmediateTestTesterCase1
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase1(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~10011
  if ((inst.Bits() & 0x01F00000)  !=
          0x01300000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool BinaryRegisterImmediateTestTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BinaryRegisterImmediateTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(16))));

  return true;
}

// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: CMP_immediate_cccc00110101nnnn0000iiiiiiiiiiii_case_0,
//       uses: {Rn}}
class BinaryRegisterImmediateTestTesterCase2
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase2(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~10101
  if ((inst.Bits() & 0x01F00000)  !=
          0x01500000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool BinaryRegisterImmediateTestTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BinaryRegisterImmediateTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(16))));

  return true;
}

// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_0,
//       uses: {Rn}}
class BinaryRegisterImmediateTestTesterCase3
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase3(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~10111
  if ((inst.Bits() & 0x01F00000)  !=
          0x01700000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool BinaryRegisterImmediateTestTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BinaryRegisterImmediateTestTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(16))));

  return true;
}

// op(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: AND_immediate_cccc0010000snnnnddddiiiiiiiiiiii_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase4
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase4(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0000x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: EOR_immediate_cccc0010001snnnnddddiiiiiiiiiiii_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase5
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase5(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0001x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=0010x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpAddSub,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: SUB_immediate_cccc0010010snnnnddddiiiiiiiiiiii_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         (Rn(19:16)=1111 &&
//            S(20)=0) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase6
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase6(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0010x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00400000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: (Rn(19:16)=1111 &&
  //       S(20)=0) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x000F0000)  ==
          0x000F0000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00000000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=0010x & Rn(19:16)=1111
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       actual: Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOpPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       generated_baseline: ADR_A2_cccc001001001111ddddiiiiiiiiiiii_case_0,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Pc}}
class Unary1RegisterImmediateOpPcTesterCase7
    : public Unary1RegisterImmediateOpPcTester {
 public:
  Unary1RegisterImmediateOpPcTesterCase7(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpPcTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0010x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00400000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpPcTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpPcTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpPcTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// op(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: RSB_immediate_cccc0010011snnnnddddiiiiiiiiiiii_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase8
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0011x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00600000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=0100x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpAddSub,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         (Rn(19:16)=1111 &&
//            S(20)=0) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase9
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase9(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00800000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 &&
  //       S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000))));

  // safety: (Rn(19:16)=1111 &&
  //       S(20)=0) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x000F0000)  ==
          0x000F0000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00000000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd, NZCV
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=0100x & Rn(19:16)=1111
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       actual: Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOpPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       generated_baseline: ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_0,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Pc}}
class Unary1RegisterImmediateOpPcTesterCase10
    : public Unary1RegisterImmediateOpPcTester {
 public:
  Unary1RegisterImmediateOpPcTesterCase10(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpPcTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00800000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpPcTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpPcTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpPcTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// op(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase11
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00A00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: SBC_immediate_cccc0010110snnnnddddiiiiiiiiiiii_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase12
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase12(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0110x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00C00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: RSC_immediate_cccc0010111snnnnddddiiiiiiiiiiii_case_0,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase13
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase13(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~0111x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00E00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//       generated_baseline: ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase14
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase14(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~1100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01800000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOp12DynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rd(15:12), imm12(11:0)],
//       generated_baseline: MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {}}
class Unary1RegisterImmediateOp12TesterCase15
    : public Unary1RegisterImmediateOp12Tester {
 public:
  Unary1RegisterImmediateOp12TesterCase15(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOp12Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOp12TesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOp12Tester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOp12TesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOp12Tester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1,
//       baseline: MaskedBinary2RegisterImmediateOp,
//       clears_bits: (imm32 &&
//            clears_mask())  ==
//               clears_mask(),
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//       generated_baseline: BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       imm32: ARMExpandImm(imm12),
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTesterCase16
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterCase16(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~1110x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01C00000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOp12DynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rd(15:12), imm12(11:0)],
//       generated_baseline: MVN_immediate_cccc0011111s0000ddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {}}
class Unary1RegisterImmediateOp12TesterCase17
    : public Unary1RegisterImmediateOp12Tester {
 public:
  Unary1RegisterImmediateOp12TesterCase17(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOp12Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOp12TesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(24:20)=~1111x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01E00000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOp12Tester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOp12TesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOp12Tester::
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
  //       if setflags
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1,
//       baseline: MaskedBinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), imm12(11:0)],
//       generated_baseline: TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       imm32: ARMExpandImm_C(imm12),
//       rule: TST_immediate,
//       sets_Z_if_clear_bits: Rn  ==
//               RegIndex(test_register()) &&
//            (imm32 &&
//            clears_mask())  ==
//               clears_mask(),
//       uses: {Rn}}
class MaskedBinaryRegisterImmediateTestTester_Case0
    : public BinaryRegisterImmediateTestTesterCase0 {
 public:
  MaskedBinaryRegisterImmediateTestTester_Case0()
    : BinaryRegisterImmediateTestTesterCase0(
      state_.MaskedBinaryRegisterImmediateTest_TST_immediate_instance_)
  {}
};

// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: TEQ_immediate_cccc00110011nnnn0000iiiiiiiiiiii_case_0,
//       rule: TEQ_immediate,
//       uses: {Rn}}
class BinaryRegisterImmediateTestTester_Case1
    : public BinaryRegisterImmediateTestTesterCase1 {
 public:
  BinaryRegisterImmediateTestTester_Case1()
    : BinaryRegisterImmediateTestTesterCase1(
      state_.BinaryRegisterImmediateTest_TEQ_immediate_instance_)
  {}
};

// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: CMP_immediate_cccc00110101nnnn0000iiiiiiiiiiii_case_0,
//       rule: CMP_immediate,
//       uses: {Rn}}
class BinaryRegisterImmediateTestTester_Case2
    : public BinaryRegisterImmediateTestTesterCase2 {
 public:
  BinaryRegisterImmediateTestTester_Case2()
    : BinaryRegisterImmediateTestTesterCase2(
      state_.BinaryRegisterImmediateTest_CMP_immediate_instance_)
  {}
};

// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_0,
//       rule: CMN_immediate,
//       uses: {Rn}}
class BinaryRegisterImmediateTestTester_Case3
    : public BinaryRegisterImmediateTestTesterCase3 {
 public:
  BinaryRegisterImmediateTestTester_Case3()
    : BinaryRegisterImmediateTestTesterCase3(
      state_.BinaryRegisterImmediateTest_CMN_immediate_instance_)
  {}
};

// op(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: AND_immediate_cccc0010000snnnnddddiiiiiiiiiiii_case_0,
//       rule: AND_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTester_Case4
    : public Binary2RegisterImmediateOpTesterCase4 {
 public:
  Binary2RegisterImmediateOpTester_Case4()
    : Binary2RegisterImmediateOpTesterCase4(
      state_.Binary2RegisterImmediateOp_AND_immediate_instance_)
  {}
};

// op(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: EOR_immediate_cccc0010001snnnnddddiiiiiiiiiiii_case_0,
//       rule: EOR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTester_Case5
    : public Binary2RegisterImmediateOpTesterCase5 {
 public:
  Binary2RegisterImmediateOpTester_Case5()
    : Binary2RegisterImmediateOpTesterCase5(
      state_.Binary2RegisterImmediateOp_EOR_immediate_instance_)
  {}
};

// op(24:20)=0010x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpAddSub,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: SUB_immediate_cccc0010010snnnnddddiiiiiiiiiiii_case_0,
//       rule: SUB_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         (Rn(19:16)=1111 &&
//            S(20)=0) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpAddSubTester_Case6
    : public Binary2RegisterImmediateOpTesterCase6 {
 public:
  Binary2RegisterImmediateOpAddSubTester_Case6()
    : Binary2RegisterImmediateOpTesterCase6(
      state_.Binary2RegisterImmediateOpAddSub_SUB_immediate_instance_)
  {}
};

// op(24:20)=0010x & Rn(19:16)=1111
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       actual: Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOpPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       generated_baseline: ADR_A2_cccc001001001111ddddiiiiiiiiiiii_case_0,
//       rule: ADR_A2,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Pc}}
class Unary1RegisterImmediateOpPcTester_Case7
    : public Unary1RegisterImmediateOpPcTesterCase7 {
 public:
  Unary1RegisterImmediateOpPcTester_Case7()
    : Unary1RegisterImmediateOpPcTesterCase7(
      state_.Unary1RegisterImmediateOpPc_ADR_A2_instance_)
  {}
};

// op(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: RSB_immediate_cccc0010011snnnnddddiiiiiiiiiiii_case_0,
//       rule: RSB_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTester_Case8
    : public Binary2RegisterImmediateOpTesterCase8 {
 public:
  Binary2RegisterImmediateOpTester_Case8()
    : Binary2RegisterImmediateOpTesterCase8(
      state_.Binary2RegisterImmediateOp_RSB_immediate_instance_)
  {}
};

// op(24:20)=0100x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpAddSub,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_0,
//       rule: ADD_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         (Rn(19:16)=1111 &&
//            S(20)=0) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpAddSubTester_Case9
    : public Binary2RegisterImmediateOpTesterCase9 {
 public:
  Binary2RegisterImmediateOpAddSubTester_Case9()
    : Binary2RegisterImmediateOpTesterCase9(
      state_.Binary2RegisterImmediateOpAddSub_ADD_immediate_instance_)
  {}
};

// op(24:20)=0100x & Rn(19:16)=1111
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       actual: Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOpPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       generated_baseline: ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_0,
//       rule: ADR_A1,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Pc}}
class Unary1RegisterImmediateOpPcTester_Case10
    : public Unary1RegisterImmediateOpPcTesterCase10 {
 public:
  Unary1RegisterImmediateOpPcTester_Case10()
    : Unary1RegisterImmediateOpPcTesterCase10(
      state_.Unary1RegisterImmediateOpPc_ADR_A1_instance_)
  {}
};

// op(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_0,
//       rule: ADC_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTester_Case11
    : public Binary2RegisterImmediateOpTesterCase11 {
 public:
  Binary2RegisterImmediateOpTester_Case11()
    : Binary2RegisterImmediateOpTesterCase11(
      state_.Binary2RegisterImmediateOp_ADC_immediate_instance_)
  {}
};

// op(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: SBC_immediate_cccc0010110snnnnddddiiiiiiiiiiii_case_0,
//       rule: SBC_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTester_Case12
    : public Binary2RegisterImmediateOpTesterCase12 {
 public:
  Binary2RegisterImmediateOpTester_Case12()
    : Binary2RegisterImmediateOpTesterCase12(
      state_.Binary2RegisterImmediateOp_SBC_immediate_instance_)
  {}
};

// op(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: RSC_immediate_cccc0010111snnnnddddiiiiiiiiiiii_case_0,
//       rule: RSC_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpTester_Case13
    : public Binary2RegisterImmediateOpTesterCase13 {
 public:
  Binary2RegisterImmediateOpTester_Case13()
    : Binary2RegisterImmediateOpTesterCase13(
      state_.Binary2RegisterImmediateOp_RSC_immediate_instance_)
  {}
};

// op(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//       generated_baseline: ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       rule: ORR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class Binary2RegisterImmediateOpDynCodeReplaceTester_Case14
    : public Binary2RegisterImmediateOpTesterCase14 {
 public:
  Binary2RegisterImmediateOpDynCodeReplaceTester_Case14()
    : Binary2RegisterImmediateOpTesterCase14(
      state_.Binary2RegisterImmediateOpDynCodeReplace_ORR_immediate_instance_)
  {}
};

// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOp12DynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rd(15:12), imm12(11:0)],
//       generated_baseline: MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       rule: MOV_immediate_A1,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {}}
class Unary1RegisterImmediateOp12DynCodeReplaceTester_Case15
    : public Unary1RegisterImmediateOp12TesterCase15 {
 public:
  Unary1RegisterImmediateOp12DynCodeReplaceTester_Case15()
    : Unary1RegisterImmediateOp12TesterCase15(
      state_.Unary1RegisterImmediateOp12DynCodeReplace_MOV_immediate_A1_instance_)
  {}
};

// op(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1,
//       baseline: MaskedBinary2RegisterImmediateOp,
//       clears_bits: (imm32 &&
//            clears_mask())  ==
//               clears_mask(),
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//       generated_baseline: BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       imm32: ARMExpandImm(imm12),
//       rule: BIC_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
class MaskedBinary2RegisterImmediateOpTester_Case16
    : public Binary2RegisterImmediateOpTesterCase16 {
 public:
  MaskedBinary2RegisterImmediateOpTester_Case16()
    : Binary2RegisterImmediateOpTesterCase16(
      state_.MaskedBinary2RegisterImmediateOp_BIC_immediate_instance_)
  {}
};

// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOp12DynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rd(15:12), imm12(11:0)],
//       generated_baseline: MVN_immediate_cccc0011111s0000ddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       rule: MVN_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {}}
class Unary1RegisterImmediateOp12DynCodeReplaceTester_Case17
    : public Unary1RegisterImmediateOp12TesterCase17 {
 public:
  Unary1RegisterImmediateOp12DynCodeReplaceTester_Case17()
    : Unary1RegisterImmediateOp12TesterCase17(
      state_.Unary1RegisterImmediateOp12DynCodeReplace_MVN_immediate_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1,
//       baseline: MaskedBinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), imm12(11:0)],
//       generated_baseline: TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       imm32: ARMExpandImm_C(imm12),
//       pattern: cccc00110001nnnn0000iiiiiiiiiiii,
//       rule: TST_immediate,
//       sets_Z_if_clear_bits: Rn  ==
//               RegIndex(test_register()) &&
//            (imm32 &&
//            clears_mask())  ==
//               clears_mask(),
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       MaskedBinaryRegisterImmediateTestTester_Case0_TestCase0) {
  MaskedBinaryRegisterImmediateTestTester_Case0 baseline_tester;
  NamedActual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1_TST_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110001nnnn0000iiiiiiiiiiii");
}

// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: TEQ_immediate_cccc00110011nnnn0000iiiiiiiiiiii_case_0,
//       pattern: cccc00110011nnnn0000iiiiiiiiiiii,
//       rule: TEQ_immediate,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case1_TestCase1) {
  BinaryRegisterImmediateTestTester_Case1 baseline_tester;
  NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_TEQ_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110011nnnn0000iiiiiiiiiiii");
}

// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: CMP_immediate_cccc00110101nnnn0000iiiiiiiiiiii_case_0,
//       pattern: cccc00110101nnnn0000iiiiiiiiiiii,
//       rule: CMP_immediate,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case2_TestCase2) {
  BinaryRegisterImmediateTestTester_Case2 baseline_tester;
  NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMP_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110101nnnn0000iiiiiiiiiiii");
}

// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Rn: Rn(19:16),
//       actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16)],
//       generated_baseline: CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_0,
//       pattern: cccc00110111nnnn0000iiiiiiiiiiii,
//       rule: CMN_immediate,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case3_TestCase3) {
  BinaryRegisterImmediateTestTester_Case3 baseline_tester;
  NamedActual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1_CMN_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110111nnnn0000iiiiiiiiiiii");
}

// op(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: AND_immediate_cccc0010000snnnnddddiiiiiiiiiiii_case_0,
//       pattern: cccc0010000snnnnddddiiiiiiiiiiii,
//       rule: AND_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case4_TestCase4) {
  Binary2RegisterImmediateOpTester_Case4 baseline_tester;
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_AND_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010000snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: EOR_immediate_cccc0010001snnnnddddiiiiiiiiiiii_case_0,
//       pattern: cccc0010001snnnnddddiiiiiiiiiiii,
//       rule: EOR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case5_TestCase5) {
  Binary2RegisterImmediateOpTester_Case5 baseline_tester;
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_EOR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010001snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0010x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpAddSub,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: SUB_immediate_cccc0010010snnnnddddiiiiiiiiiiii_case_0,
//       pattern: cccc0010010snnnnddddiiiiiiiiiiii,
//       rule: SUB_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         (Rn(19:16)=1111 &&
//            S(20)=0) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpAddSubTester_Case6_TestCase6) {
  Binary2RegisterImmediateOpAddSubTester_Case6 baseline_tester;
  NamedActual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_SUB_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010010snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0010x & Rn(19:16)=1111
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       actual: Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOpPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       generated_baseline: ADR_A2_cccc001001001111ddddiiiiiiiiiiii_case_0,
//       pattern: cccc001001001111ddddiiiiiiiiiiii,
//       rule: ADR_A2,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Pc}}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpPcTester_Case7_TestCase7) {
  Unary1RegisterImmediateOpPcTester_Case7 baseline_tester;
  NamedActual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001001001111ddddiiiiiiiiiiii");
}

// op(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: RSB_immediate_cccc0010011snnnnddddiiiiiiiiiiii_case_0,
//       pattern: cccc0010011snnnnddddiiiiiiiiiiii,
//       rule: RSB_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case8_TestCase8) {
  Binary2RegisterImmediateOpTester_Case8 baseline_tester;
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSB_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010011snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0100x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpAddSub,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_0,
//       pattern: cccc0010100snnnnddddiiiiiiiiiiii,
//       rule: ADD_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         (Rn(19:16)=1111 &&
//            S(20)=0) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpAddSubTester_Case9_TestCase9) {
  Binary2RegisterImmediateOpAddSubTester_Case9 baseline_tester;
  NamedActual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1_ADD_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010100snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0100x & Rn(19:16)=1111
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       actual: Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOpPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       generated_baseline: ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_0,
//       pattern: cccc001010001111ddddiiiiiiiiiiii,
//       rule: ADR_A1,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Pc}}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpPcTester_Case10_TestCase10) {
  Unary1RegisterImmediateOpPcTester_Case10 baseline_tester;
  NamedActual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1_ADR_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001010001111ddddiiiiiiiiiiii");
}

// op(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_0,
//       pattern: cccc0010101snnnnddddiiiiiiiiiiii,
//       rule: ADC_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case11_TestCase11) {
  Binary2RegisterImmediateOpTester_Case11 baseline_tester;
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_ADC_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010101snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: SBC_immediate_cccc0010110snnnnddddiiiiiiiiiiii_case_0,
//       pattern: cccc0010110snnnnddddiiiiiiiiiiii,
//       rule: SBC_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case12_TestCase12) {
  Binary2RegisterImmediateOpTester_Case12 baseline_tester;
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_SBC_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010110snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       generated_baseline: RSC_immediate_cccc0010111snnnnddddiiiiiiiiiiii_case_0,
//       pattern: cccc0010111snnnnddddiiiiiiiiiiii,
//       rule: RSC_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case13_TestCase13) {
  Binary2RegisterImmediateOpTester_Case13 baseline_tester;
  NamedActual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1_RSC_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010111snnnnddddiiiiiiiiiiii");
}

// op(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1,
//       baseline: Binary2RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//       generated_baseline: ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       pattern: cccc0011100snnnnddddiiiiiiiiiiii,
//       rule: ORR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpDynCodeReplaceTester_Case14_TestCase14) {
  Binary2RegisterImmediateOpDynCodeReplaceTester_Case14 baseline_tester;
  NamedActual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1_ORR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011100snnnnddddiiiiiiiiiiii");
}

// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOp12DynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rd(15:12), imm12(11:0)],
//       generated_baseline: MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       pattern: cccc0011101s0000ddddiiiiiiiiiiii,
//       rule: MOV_immediate_A1,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOp12DynCodeReplaceTester_Case15_TestCase15) {
  Unary1RegisterImmediateOp12DynCodeReplaceTester_Case15 baseline_tester;
  NamedActual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MOV_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011101s0000ddddiiiiiiiiiiii");
}

// op(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1,
//       baseline: MaskedBinary2RegisterImmediateOp,
//       clears_bits: (imm32 &&
//            clears_mask())  ==
//               clears_mask(),
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//       generated_baseline: BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       imm32: ARMExpandImm(imm12),
//       pattern: cccc0011110snnnnddddiiiiiiiiiiii,
//       rule: BIC_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       MaskedBinary2RegisterImmediateOpTester_Case16_TestCase16) {
  MaskedBinary2RegisterImmediateOpTester_Case16 baseline_tester;
  NamedActual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1_BIC_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011110snnnnddddiiiiiiiiiiii");
}

// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1,
//       baseline: Unary1RegisterImmediateOp12DynCodeReplace,
//       constraints: ,
//       defs: {Rd, NZCV
//            if setflags
//            else None},
//       dynamic_code_replace_immediates: {imm12},
//       fields: [S(20), Rd(15:12), imm12(11:0)],
//       generated_baseline: MVN_immediate_cccc0011111s0000ddddiiiiiiiiiiii_case_0,
//       imm12: imm12(11:0),
//       pattern: cccc0011111s0000ddddiiiiiiiiiiii,
//       rule: MVN_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOp12DynCodeReplaceTester_Case17_TestCase17) {
  Unary1RegisterImmediateOp12DynCodeReplaceTester_Case17 baseline_tester;
  NamedActual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1_MVN_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011111s0000ddddiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
