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


// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'Unary1RegisterUse',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:18)=00 => UNPREDICTABLE]}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rn: Rn(3:0),
//       baseline: Unary1RegisterUse,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18), Rn(3:0)],
//       mask: mask(19:18),
//       safety: [mask(19:18)=00 => UNPREDICTABLE,
//         Rn  ==
//               Pc => UNPREDICTABLE],
//       write_nzcvq: mask(1)=1}
class Unary1RegisterUseTesterCase0
    : public Unary1RegisterUseTester {
 public:
  Unary1RegisterUseTesterCase0(const NamedClassDecoder& decoder)
    : Unary1RegisterUseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterUseTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op1(19:16)=~xx00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx
  if ((inst.Bits() & 0x0000FD00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterUseTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterUseTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterUseTester::
               ApplySanityChecks(inst, decoder));

  // safety: mask(19:18)=00 => UNPREDICTABLE
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x00000000);

  // safety: Rn  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000000F)) != (15)));

  // defs: {NZCV
  //       if write_nzcvq
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((((inst.Bits() & 0x000C0000) >> 18) & 0x00000002)  ==
          0x00000002
       ? 16
       : 32)))));

  return true;
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op1(19:16)=~xx01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx
  if ((inst.Bits() & 0x0000FD00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase2
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op1(19:16)=~xx1x
  if ((inst.Bits() & 0x00020000)  !=
          0x00020000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx
  if ((inst.Bits() & 0x0000FD00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase3
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx
  if ((inst.Bits() & 0x0000FD00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {baseline: 'Unary1RegisterSet',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [inst(15:12)=1111 => UNPREDICTABLE,
//         inst(22)=1 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {R: R(22),
//       Rd: Rd(15:12),
//       baseline: Unary1RegisterSet,
//       constraints: ,
//       defs: {Rd},
//       fields: [R(22), Rd(15:12)],
//       safety: [R(22)=1 => FORBIDDEN_OPERANDS,
//         Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterSetTesterCase4
    : public Unary1RegisterSetTester {
 public:
  Unary1RegisterSetTesterCase4(const NamedClassDecoder& decoder)
    : Unary1RegisterSetTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterSetTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~x0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx00x0xxxx0000
  if ((inst.Bits() & 0x000F0D0F)  !=
          0x000F0000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterSetTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterSetTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterSetTester::
               ApplySanityChecks(inst, decoder));

  // safety: R(22)=1 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x00400000)  !=
          0x00400000);

  // safety: Rd(15:12)=1111 => UNPREDICTABLE
  EXPECT_TRUE((inst.Bits() & 0x0000F000)  !=
          0x0000F000);

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase5
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase5(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~1
  if ((inst.Bits() & 0x00000200)  !=
          0x00000200) return false;
  // op(22:21)=~x0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
  if ((inst.Bits() & 0x00000C0F)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase6
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase6(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~1
  if ((inst.Bits() & 0x00000200)  !=
          0x00000200) return false;
  // op(22:21)=~x1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100xxxxxxxxxx
  if ((inst.Bits() & 0x0000FC00)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'BranchToRegister',
//       constraints: ,
//       defs: {15},
//       safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       baseline: BranchToRegister,
//       constraints: ,
//       defs: {Pc},
//       fields: [Rm(3:0)],
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS]}
class BranchToRegisterTesterCase7
    : public BranchToRegisterTester {
 public:
  BranchToRegisterTesterCase7(const NamedClassDecoder& decoder)
    : BranchToRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool BranchToRegisterTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~001
  if ((inst.Bits() & 0x00000070)  !=
          0x00000010) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FFF00) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

bool BranchToRegisterTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BranchToRegisterTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rm(3:0)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000000F)  !=
          0x0000000F);

  // defs: {Pc};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(15))));

  return true;
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPc',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterOpNotRmIsPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterOpNotRmIsPcTesterCase8
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase8(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~001
  if ((inst.Bits() & 0x00000070)  !=
          0x00000010) return false;
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary2RegisterOpNotRmIsPcTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary2RegisterOpNotRmIsPcTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase9
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~010
  if ((inst.Bits() & 0x00000070)  !=
          0x00000020) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FFF00) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'BranchToRegister',
//       constraints: ,
//       defs: {15, 14},
//       safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Lr: 14,
//       Pc: 15,
//       Rm: Rm(3:0),
//       baseline: BranchToRegister,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [Rm(3:0)],
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS]}
class BranchToRegisterTesterCase10
    : public BranchToRegisterTester {
 public:
  BranchToRegisterTesterCase10(const NamedClassDecoder& decoder)
    : BranchToRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool BranchToRegisterTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~011
  if ((inst.Bits() & 0x00000070)  !=
          0x00000030) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FFF00) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

bool BranchToRegisterTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BranchToRegisterTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rm(3:0)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000000F)  !=
          0x0000000F);

  // defs: {Pc, Lr};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(15)).
   Add(Register(14))));

  return true;
}

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase11
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase11(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~110
  if ((inst.Bits() & 0x00000070)  !=
          0x00000060) return false;
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx000000000000xxxx1110
  if ((inst.Bits() & 0x000FFF0F)  !=
          0x0000000E) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = {baseline: 'BreakPointAndConstantPoolHead',
//       constraints: ,
//       defs: {},
//       safety: [inst(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=01
//    = {baseline: BreakPointAndConstantPoolHead,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       inst: inst,
//       safety: [cond(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS]}
class Immediate16UseTesterCase12
    : public Immediate16UseTester {
 public:
  Immediate16UseTesterCase12(const NamedClassDecoder& decoder)
    : Immediate16UseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Immediate16UseTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~111
  if ((inst.Bits() & 0x00000070)  !=
          0x00000070) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return Immediate16UseTester::
      PassesParsePreconditions(inst, decoder);
}

bool Immediate16UseTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Immediate16UseTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond(31:28)=~1110 => UNPREDICTABLE
  EXPECT_TRUE((inst.Bits() & 0xF0000000)  ==
          0xE0000000);

  // safety: not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS
  EXPECT_TRUE(nacl_arm_dec::IsBreakPointAndConstantPoolHead(inst.Bits()));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=111 & op(22:21)=10
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase13
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase13(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~111
  if ((inst.Bits() & 0x00000070)  !=
          0x00000070) return false;
  // op(22:21)=~10
  if ((inst.Bits() & 0x00600000)  !=
          0x00400000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representative case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase14
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase14(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~111
  if ((inst.Bits() & 0x00000070)  !=
          0x00000070) return false;
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx000000000000xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'Unary1RegisterUse',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       rule: 'MSR_register',
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:18)=00 => UNPREDICTABLE]}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rn: Rn(3:0),
//       baseline: Unary1RegisterUse,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18), Rn(3:0)],
//       mask: mask(19:18),
//       rule: MSR_register,
//       safety: [mask(19:18)=00 => UNPREDICTABLE,
//         Rn  ==
//               Pc => UNPREDICTABLE],
//       write_nzcvq: mask(1)=1}
class Unary1RegisterUseTester_Case0
    : public Unary1RegisterUseTesterCase0 {
 public:
  Unary1RegisterUseTester_Case0()
    : Unary1RegisterUseTesterCase0(
      state_.Unary1RegisterUse_MSR_register_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'MSR_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: MSR_register}
class ForbiddenCondDecoderTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenCondDecoderTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.ForbiddenCondDecoder_MSR_register_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'MSR_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: MSR_register}
class ForbiddenCondDecoderTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenCondDecoderTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.ForbiddenCondDecoder_MSR_register_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'MSR_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: MSR_register}
class ForbiddenCondDecoderTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenCondDecoderTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.ForbiddenCondDecoder_MSR_register_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {baseline: 'Unary1RegisterSet',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'MRS',
//       safety: [inst(15:12)=1111 => UNPREDICTABLE,
//         inst(22)=1 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {R: R(22),
//       Rd: Rd(15:12),
//       baseline: Unary1RegisterSet,
//       constraints: ,
//       defs: {Rd},
//       fields: [R(22), Rd(15:12)],
//       rule: MRS,
//       safety: [R(22)=1 => FORBIDDEN_OPERANDS,
//         Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterSetTester_Case4
    : public Unary1RegisterSetTesterCase4 {
 public:
  Unary1RegisterSetTester_Case4()
    : Unary1RegisterSetTesterCase4(
      state_.Unary1RegisterSet_MRS_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'MRS_Banked_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: MRS_Banked_register}
class ForbiddenCondDecoderTester_Case5
    : public UnsafeCondDecoderTesterCase5 {
 public:
  ForbiddenCondDecoderTester_Case5()
    : UnsafeCondDecoderTesterCase5(
      state_.ForbiddenCondDecoder_MRS_Banked_register_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'MSR_Banked_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: MSR_Banked_register}
class ForbiddenCondDecoderTester_Case6
    : public UnsafeCondDecoderTesterCase6 {
 public:
  ForbiddenCondDecoderTester_Case6()
    : UnsafeCondDecoderTesterCase6(
      state_.ForbiddenCondDecoder_MSR_Banked_register_instance_)
  {}
};

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'BranchToRegister',
//       constraints: ,
//       defs: {15},
//       rule: 'Bx',
//       safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       baseline: BranchToRegister,
//       constraints: ,
//       defs: {Pc},
//       fields: [Rm(3:0)],
//       rule: Bx,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS]}
class BranchToRegisterTester_Case7
    : public BranchToRegisterTesterCase7 {
 public:
  BranchToRegisterTester_Case7()
    : BranchToRegisterTesterCase7(
      state_.BranchToRegister_Bx_instance_)
  {}
};

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPc',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'CLZ',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       baseline: Unary2RegisterOpNotRmIsPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       rule: CLZ,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
class Unary2RegisterOpNotRmIsPcTester_Case8
    : public Unary2RegisterOpNotRmIsPcTesterCase8 {
 public:
  Unary2RegisterOpNotRmIsPcTester_Case8()
    : Unary2RegisterOpNotRmIsPcTesterCase8(
      state_.Unary2RegisterOpNotRmIsPc_CLZ_instance_)
  {}
};

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'BXJ'}
//
// Representative case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: BXJ}
class ForbiddenCondDecoderTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenCondDecoderTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.ForbiddenCondDecoder_BXJ_instance_)
  {}
};

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {baseline: 'BranchToRegister',
//       constraints: ,
//       defs: {15, 14},
//       rule: 'BLX_register',
//       safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Lr: 14,
//       Pc: 15,
//       Rm: Rm(3:0),
//       baseline: BranchToRegister,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [Rm(3:0)],
//       rule: BLX_register,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS]}
class BranchToRegisterTester_Case10
    : public BranchToRegisterTesterCase10 {
 public:
  BranchToRegisterTester_Case10()
    : BranchToRegisterTesterCase10(
      state_.BranchToRegister_BLX_register_instance_)
  {}
};

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'ERET'}
//
// Representative case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: ERET}
class ForbiddenCondDecoderTester_Case11
    : public UnsafeCondDecoderTesterCase11 {
 public:
  ForbiddenCondDecoderTester_Case11()
    : UnsafeCondDecoderTesterCase11(
      state_.ForbiddenCondDecoder_ERET_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = {baseline: 'BreakPointAndConstantPoolHead',
//       constraints: ,
//       defs: {},
//       rule: 'BKPT',
//       safety: [inst(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=01
//    = {baseline: BreakPointAndConstantPoolHead,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       inst: inst,
//       rule: BKPT,
//       safety: [cond(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS]}
class BreakPointAndConstantPoolHeadTester_Case12
    : public Immediate16UseTesterCase12 {
 public:
  BreakPointAndConstantPoolHeadTester_Case12()
    : Immediate16UseTesterCase12(
      state_.BreakPointAndConstantPoolHead_BKPT_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'HVC'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=10
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: HVC}
class ForbiddenCondDecoderTester_Case13
    : public UnsafeCondDecoderTesterCase13 {
 public:
  ForbiddenCondDecoderTester_Case13()
    : UnsafeCondDecoderTesterCase13(
      state_.ForbiddenCondDecoder_HVC_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'SMC'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: SMC}
class ForbiddenCondDecoderTester_Case14
    : public UnsafeCondDecoderTesterCase14 {
 public:
  ForbiddenCondDecoderTester_Case14()
    : UnsafeCondDecoderTesterCase14(
      state_.ForbiddenCondDecoder_SMC_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: 'Unary1RegisterUse',
//       baseline: 'Unary1RegisterUse',
//       constraints: ,
//       defs: {16
//            if inst(19:18)(1)=1
//            else 32},
//       pattern: 'cccc00010010mm00111100000000nnnn',
//       rule: 'MSR_register',
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(19:18)=00 => UNPREDICTABLE]}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rn: Rn(3:0),
//       actual: Unary1RegisterUse,
//       baseline: Unary1RegisterUse,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18), Rn(3:0)],
//       mask: mask(19:18),
//       pattern: cccc00010010mm00111100000000nnnn,
//       rule: MSR_register,
//       safety: [mask(19:18)=00 => UNPREDICTABLE,
//         Rn  ==
//               Pc => UNPREDICTABLE],
//       write_nzcvq: mask(1)=1}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterUseTester_Case0_TestCase0) {
  Unary1RegisterUseTester_Case0 tester;
  tester.Test("cccc00010010mm00111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010r10mmmm111100000000nnnn',
//       rule: 'MSR_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case1_TestCase1) {
  ForbiddenCondDecoderTester_Case1 baseline_tester;
  NamedForbidden_MSR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010r10mmmm111100000000nnnn',
//       rule: 'MSR_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case2_TestCase2) {
  ForbiddenCondDecoderTester_Case2 baseline_tester;
  NamedForbidden_MSR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010r10mmmm111100000000nnnn',
//       rule: 'MSR_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case3_TestCase3) {
  ForbiddenCondDecoderTester_Case3 baseline_tester;
  NamedForbidden_MSR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {actual: 'Unary1RegisterSet',
//       baseline: 'Unary1RegisterSet',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc00010r001111dddd000000000000',
//       rule: 'MRS',
//       safety: [inst(15:12)=1111 => UNPREDICTABLE,
//         inst(22)=1 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {R: R(22),
//       Rd: Rd(15:12),
//       actual: Unary1RegisterSet,
//       baseline: Unary1RegisterSet,
//       constraints: ,
//       defs: {Rd},
//       fields: [R(22), Rd(15:12)],
//       pattern: cccc00010r001111dddd000000000000,
//       rule: MRS,
//       safety: [R(22)=1 => FORBIDDEN_OPERANDS,
//         Rd(15:12)=1111 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterSetTester_Case4_TestCase4) {
  Unary1RegisterSetTester_Case4 tester;
  tester.Test("cccc00010r001111dddd000000000000");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010r00mmmmdddd001m00000000',
//       rule: 'MRS_Banked_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r00mmmmdddd001m00000000,
//       rule: MRS_Banked_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case5_TestCase5) {
  ForbiddenCondDecoderTester_Case5 baseline_tester;
  NamedForbidden_MRS_Banked_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r00mmmmdddd001m00000000");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010r10mmmm1111001m0000nnnn',
//       rule: 'MSR_Banked_register'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm1111001m0000nnnn,
//       rule: MSR_Banked_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case6_TestCase6) {
  ForbiddenCondDecoderTester_Case6 baseline_tester;
  NamedForbidden_MSR_Banked_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm1111001m0000nnnn");
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: 'BranchToRegister',
//       baseline: 'BranchToRegister',
//       constraints: ,
//       defs: {15},
//       pattern: 'cccc000100101111111111110001mmmm',
//       rule: 'Bx',
//       safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       actual: BranchToRegister,
//       baseline: BranchToRegister,
//       constraints: ,
//       defs: {Pc},
//       fields: [Rm(3:0)],
//       pattern: cccc000100101111111111110001mmmm,
//       rule: Bx,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_Case7_TestCase7) {
  BranchToRegisterTester_Case7 tester;
  tester.Test("cccc000100101111111111110001mmmm");
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Unary2RegisterOpNotRmIsPc',
//       baseline: 'Unary2RegisterOpNotRmIsPc',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc000101101111dddd11110001mmmm',
//       rule: 'CLZ',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE]}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Unary2RegisterOpNotRmIsPc,
//       baseline: Unary2RegisterOpNotRmIsPc,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc000101101111dddd11110001mmmm,
//       rule: CLZ,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_Case8_TestCase8) {
  Unary2RegisterOpNotRmIsPcTester_Case8 tester;
  tester.Test("cccc000101101111dddd11110001mmmm");
}

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc000100101111111111110010mmmm',
//       rule: 'BXJ'}
//
// Representative case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000100101111111111110010mmmm,
//       rule: BXJ}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case9_TestCase9) {
  ForbiddenCondDecoderTester_Case9 baseline_tester;
  NamedForbidden_BXJ actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110010mmmm");
}

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: 'BranchToRegister',
//       baseline: 'BranchToRegister',
//       constraints: ,
//       defs: {15, 14},
//       pattern: 'cccc000100101111111111110011mmmm',
//       rule: 'BLX_register',
//       safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Lr: 14,
//       Pc: 15,
//       Rm: Rm(3:0),
//       actual: BranchToRegister,
//       baseline: BranchToRegister,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [Rm(3:0)],
//       pattern: cccc000100101111111111110011mmmm,
//       rule: BLX_register,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_Case10_TestCase10) {
  BranchToRegisterTester_Case10 tester;
  tester.Test("cccc000100101111111111110011mmmm");
}

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0001011000000000000001101110',
//       rule: 'ERET'}
//
// Representative case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0001011000000000000001101110,
//       rule: ERET}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case11_TestCase11) {
  ForbiddenCondDecoderTester_Case11 baseline_tester;
  NamedForbidden_ERET actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001011000000000000001101110");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = {actual: 'BreakPointAndConstantPoolHead',
//       baseline: 'BreakPointAndConstantPoolHead',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc00010010iiiiiiiiiiii0111iiii',
//       rule: 'BKPT',
//       safety: [inst(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS]}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=01
//    = {actual: BreakPointAndConstantPoolHead,
//       baseline: BreakPointAndConstantPoolHead,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       inst: inst,
//       pattern: cccc00010010iiiiiiiiiiii0111iiii,
//       rule: BKPT,
//       safety: [cond(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       BreakPointAndConstantPoolHeadTester_Case12_TestCase12) {
  BreakPointAndConstantPoolHeadTester_Case12 tester;
  tester.Test("cccc00010010iiiiiiiiiiii0111iiii");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc00010100iiiiiiiiiiii0111iiii',
//       rule: 'HVC'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=10
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010100iiiiiiiiiiii0111iiii,
//       rule: HVC}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case13_TestCase13) {
  ForbiddenCondDecoderTester_Case13 baseline_tester;
  NamedForbidden_HVC actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100iiiiiiiiiiii0111iiii");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc000101100000000000000111iiii',
//       rule: 'SMC'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000101100000000000000111iiii,
//       rule: SMC}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case14_TestCase14) {
  ForbiddenCondDecoderTester_Case14 baseline_tester;
  NamedForbidden_SMC actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101100000000000000111iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
