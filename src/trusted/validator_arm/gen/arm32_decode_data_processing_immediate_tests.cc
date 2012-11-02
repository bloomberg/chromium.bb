/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
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
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'MaskedBinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16}}
//
// Representaive case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       baseline: MaskedBinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV}}
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
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool BinaryRegisterImmediateTestTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BinaryRegisterImmediateTestTester::ApplySanityChecks(inst, decoder));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(16))));

  return true;
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16}}
//
// Representaive case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV}}
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
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool BinaryRegisterImmediateTestTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BinaryRegisterImmediateTestTester::ApplySanityChecks(inst, decoder));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(16))));

  return true;
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16}}
//
// Representaive case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV}}
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
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool BinaryRegisterImmediateTestTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BinaryRegisterImmediateTestTester::ApplySanityChecks(inst, decoder));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(16))));

  return true;
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16}}
//
// Representaive case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV}}
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
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op(24:20)=~10111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool BinaryRegisterImmediateTestTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BinaryRegisterImmediateTestTester::ApplySanityChecks(inst, decoder));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(16))));

  return true;
}

// Neutral case:
// inst(24:20)=0000x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0001x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', '(inst(19:16)=1111 && inst(20)=0) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, (Rn(19:16)=1111 && S(20)=0) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op(24:20)=~0010x */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: (Rn(19:16)=1111 && S(20)=0) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x000F0000) == 0x000F0000) && ((inst.Bits() & 0x00100000) == 0x00000000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=1111
//    = {baseline: 'Unary1RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0010x & Rn(19:16)=1111
//    = {Rd: Rd(15:12),
//       baseline: Unary1RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary1RegisterImmediateOpTesterCase7
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase7(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op(24:20)=~0010x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(24:20)=0011x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', '(inst(19:16)=1111 && inst(20)=0) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, (Rn(19:16)=1111 && S(20)=0) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op(24:20)=~0100x */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: (Rn(19:16)=1111 && S(20)=0) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x000F0000) == 0x000F0000) && ((inst.Bits() & 0x00100000) == 0x00000000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=1111
//    = {baseline: 'Unary1RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0100x & Rn(19:16)=1111
//    = {Rd: Rd(15:12),
//       baseline: Unary1RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary1RegisterImmediateOpTesterCase10
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase10(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op(24:20)=~0100x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(24:20)=0101x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0110x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0111x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1100x
//    = {baseline: 'Binary2RegisterImmediateOpDynCodeReplace',
//       constraints: & not (inst(15:12)=1111 && inst(20)=1) ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOpDynCodeReplace,
//       constraints: & not (Rd(15:12)=1111 && S(20)=1) ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op(24:20)=~1100x */) return false;

  // Check pattern restrictions of row.
  if ((((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000)) /* (Rd(15:12)=1111 && S(20)=1) */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Unary1RegisterImmediateOpTesterCase15
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase15(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1110x
//    = {baseline: 'MaskedBinary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: MaskedBinary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
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
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary2RegisterImmediateOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Unary1RegisterImmediateOpTesterCase17
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase17(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // safety: (Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR
  EXPECT_TRUE((!(((inst.Bits() & 0x0000F000) == 0x0000F000) && ((inst.Bits() & 0x00100000) == 0x00100000))));

  // safety: Rd(15:12)=1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000);

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'MaskedBinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16},
//       rule: 'TST_immediate_A1'}
//
// Representative case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       baseline: MaskedBinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       rule: TST_immediate_A1}
class MaskedBinaryRegisterImmediateTestTester_Case0
    : public BinaryRegisterImmediateTestTesterCase0 {
 public:
  MaskedBinaryRegisterImmediateTestTester_Case0()
    : BinaryRegisterImmediateTestTesterCase0(
      state_.MaskedBinaryRegisterImmediateTest_TST_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16},
//       rule: 'TEQ_immediate_A1'}
//
// Representative case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       rule: TEQ_immediate_A1}
class BinaryRegisterImmediateTestTester_Case1
    : public BinaryRegisterImmediateTestTesterCase1 {
 public:
  BinaryRegisterImmediateTestTester_Case1()
    : BinaryRegisterImmediateTestTesterCase1(
      state_.BinaryRegisterImmediateTest_TEQ_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16},
//       rule: 'CMP_immediate_A1'}
//
// Representative case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       rule: CMP_immediate_A1}
class BinaryRegisterImmediateTestTester_Case2
    : public BinaryRegisterImmediateTestTesterCase2 {
 public:
  BinaryRegisterImmediateTestTester_Case2()
    : BinaryRegisterImmediateTestTesterCase2(
      state_.BinaryRegisterImmediateTest_CMP_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16},
//       rule: 'CMN_immediate_A1'}
//
// Representative case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       rule: CMN_immediate_A1}
class BinaryRegisterImmediateTestTester_Case3
    : public BinaryRegisterImmediateTestTesterCase3 {
 public:
  BinaryRegisterImmediateTestTester_Case3()
    : BinaryRegisterImmediateTestTesterCase3(
      state_.BinaryRegisterImmediateTest_CMN_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'AND_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: AND_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpTester_Case4
    : public Binary2RegisterImmediateOpTesterCase4 {
 public:
  Binary2RegisterImmediateOpTester_Case4()
    : Binary2RegisterImmediateOpTesterCase4(
      state_.Binary2RegisterImmediateOp_AND_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'EOR_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: EOR_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpTester_Case5
    : public Binary2RegisterImmediateOpTesterCase5 {
 public:
  Binary2RegisterImmediateOpTester_Case5()
    : Binary2RegisterImmediateOpTesterCase5(
      state_.Binary2RegisterImmediateOp_EOR_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'SUB_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', '(inst(19:16)=1111 && inst(20)=0) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       rule: SUB_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, (Rn(19:16)=1111 && S(20)=0) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpTester_Case6
    : public Binary2RegisterImmediateOpTesterCase6 {
 public:
  Binary2RegisterImmediateOpTester_Case6()
    : Binary2RegisterImmediateOpTesterCase6(
      state_.Binary2RegisterImmediateOp_SUB_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=1111
//    = {baseline: 'Unary1RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'ADR_A2',
//       safety: ['inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0010x & Rn(19:16)=1111
//    = {Rd: Rd(15:12),
//       baseline: Unary1RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       rule: ADR_A2,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary1RegisterImmediateOpTester_Case7
    : public Unary1RegisterImmediateOpTesterCase7 {
 public:
  Unary1RegisterImmediateOpTester_Case7()
    : Unary1RegisterImmediateOpTesterCase7(
      state_.Unary1RegisterImmediateOp_ADR_A2_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'RSB_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: RSB_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpTester_Case8
    : public Binary2RegisterImmediateOpTesterCase8 {
 public:
  Binary2RegisterImmediateOpTester_Case8()
    : Binary2RegisterImmediateOpTesterCase8(
      state_.Binary2RegisterImmediateOp_RSB_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'ADD_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', '(inst(19:16)=1111 && inst(20)=0) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       rule: ADD_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, (Rn(19:16)=1111 && S(20)=0) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpTester_Case9
    : public Binary2RegisterImmediateOpTesterCase9 {
 public:
  Binary2RegisterImmediateOpTester_Case9()
    : Binary2RegisterImmediateOpTesterCase9(
      state_.Binary2RegisterImmediateOp_ADD_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=1111
//    = {baseline: 'Unary1RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'ADR_A1',
//       safety: ['inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0100x & Rn(19:16)=1111
//    = {Rd: Rd(15:12),
//       baseline: Unary1RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       rule: ADR_A1,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
class Unary1RegisterImmediateOpTester_Case10
    : public Unary1RegisterImmediateOpTesterCase10 {
 public:
  Unary1RegisterImmediateOpTester_Case10()
    : Unary1RegisterImmediateOpTesterCase10(
      state_.Unary1RegisterImmediateOp_ADR_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'ADC_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: ADC_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpTester_Case11
    : public Binary2RegisterImmediateOpTesterCase11 {
 public:
  Binary2RegisterImmediateOpTester_Case11()
    : Binary2RegisterImmediateOpTesterCase11(
      state_.Binary2RegisterImmediateOp_ADC_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'SBC_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: SBC_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpTester_Case12
    : public Binary2RegisterImmediateOpTesterCase12 {
 public:
  Binary2RegisterImmediateOpTester_Case12()
    : Binary2RegisterImmediateOpTesterCase12(
      state_.Binary2RegisterImmediateOp_SBC_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = {baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'RSC_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: RSC_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpTester_Case13
    : public Binary2RegisterImmediateOpTesterCase13 {
 public:
  Binary2RegisterImmediateOpTester_Case13()
    : Binary2RegisterImmediateOpTesterCase13(
      state_.Binary2RegisterImmediateOp_RSC_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = {baseline: 'Binary2RegisterImmediateOpDynCodeReplace',
//       constraints: & not (inst(15:12)=1111 && inst(20)=1) ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'ORR_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Binary2RegisterImmediateOpDynCodeReplace,
//       constraints: & not (Rd(15:12)=1111 && S(20)=1) ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: ORR_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Binary2RegisterImmediateOpDynCodeReplaceTester_Case14
    : public Binary2RegisterImmediateOpTesterCase14 {
 public:
  Binary2RegisterImmediateOpDynCodeReplaceTester_Case14()
    : Binary2RegisterImmediateOpTesterCase14(
      state_.Binary2RegisterImmediateOpDynCodeReplace_ORR_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'MOV_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: MOV_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Unary1RegisterImmediateOpDynCodeReplaceTester_Case15
    : public Unary1RegisterImmediateOpTesterCase15 {
 public:
  Unary1RegisterImmediateOpDynCodeReplaceTester_Case15()
    : Unary1RegisterImmediateOpTesterCase15(
      state_.Unary1RegisterImmediateOpDynCodeReplace_MOV_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = {baseline: 'MaskedBinary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'BIC_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: MaskedBinary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: BIC_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class MaskedBinary2RegisterImmediateOpTester_Case16
    : public Binary2RegisterImmediateOpTesterCase16 {
 public:
  MaskedBinary2RegisterImmediateOpTester_Case16()
    : Binary2RegisterImmediateOpTesterCase16(
      state_.MaskedBinary2RegisterImmediateOp_BIC_immediate_A1_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'MVN_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       rule: MVN_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
class Unary1RegisterImmediateOpDynCodeReplaceTester_Case17
    : public Unary1RegisterImmediateOpTesterCase17 {
 public:
  Unary1RegisterImmediateOpDynCodeReplaceTester_Case17()
    : Unary1RegisterImmediateOpTesterCase17(
      state_.Unary1RegisterImmediateOpDynCodeReplace_MVN_immediate_A1_instance_)
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
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'TestIfAddressMasked',
//       baseline: 'MaskedBinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16},
//       pattern: 'cccc00110001nnnn0000iiiiiiiiiiii',
//       rule: 'TST_immediate_A1'}
//
// Representative case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       actual: TestIfAddressMasked,
//       baseline: MaskedBinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       pattern: cccc00110001nnnn0000iiiiiiiiiiii,
//       rule: TST_immediate_A1}
TEST_F(Arm32DecoderStateTests,
       MaskedBinaryRegisterImmediateTestTester_Case0_TestCase0) {
  MaskedBinaryRegisterImmediateTestTester_Case0 baseline_tester;
  NamedTestIfAddressMasked_TST_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110001nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16},
//       pattern: 'cccc00110011nnnn0000iiiiiiiiiiii',
//       rule: 'TEQ_immediate_A1'}
//
// Representative case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       actual: DontCareInst,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       pattern: cccc00110011nnnn0000iiiiiiiiiiii,
//       rule: TEQ_immediate_A1}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case1_TestCase1) {
  BinaryRegisterImmediateTestTester_Case1 baseline_tester;
  NamedDontCareInst_TEQ_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110011nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16},
//       pattern: 'cccc00110101nnnn0000iiiiiiiiiiii',
//       rule: 'CMP_immediate_A1'}
//
// Representative case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       actual: DontCareInst,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       pattern: cccc00110101nnnn0000iiiiiiiiiiii,
//       rule: CMP_immediate_A1}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case2_TestCase2) {
  BinaryRegisterImmediateTestTester_Case2 baseline_tester;
  NamedDontCareInst_CMP_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110101nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'BinaryRegisterImmediateTest',
//       constraints: ,
//       defs: {16},
//       pattern: 'cccc00110111nnnn0000iiiiiiiiiiii',
//       rule: 'CMN_immediate_A1'}
//
// Representative case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       actual: DontCareInst,
//       baseline: BinaryRegisterImmediateTest,
//       constraints: ,
//       defs: {NZCV},
//       pattern: cccc00110111nnnn0000iiiiiiiiiiii,
//       rule: CMN_immediate_A1}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case3_TestCase3) {
  BinaryRegisterImmediateTestTester_Case3 baseline_tester;
  NamedDontCareInst_CMN_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110111nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0000x
//    = {actual: 'Defs12To15',
//       baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0010000snnnnddddiiiiiiiiiiii',
//       rule: 'AND_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Defs12To15,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0010000snnnnddddiiiiiiiiiiii,
//       rule: AND_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case4_TestCase4) {
  Binary2RegisterImmediateOpTester_Case4 baseline_tester;
  NamedDefs12To15_AND_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010000snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0001x
//    = {actual: 'Defs12To15',
//       baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0010001snnnnddddiiiiiiiiiiii',
//       rule: 'EOR_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Defs12To15,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0010001snnnnddddiiiiiiiiiiii,
//       rule: EOR_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case5_TestCase5) {
  Binary2RegisterImmediateOpTester_Case5 baseline_tester;
  NamedDefs12To15_EOR_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010001snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = {actual: 'Defs12To15',
//       baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0010010snnnnddddiiiiiiiiiiii',
//       rule: 'SUB_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', '(inst(19:16)=1111 && inst(20)=0) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Defs12To15,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       pattern: cccc0010010snnnnddddiiiiiiiiiiii,
//       rule: SUB_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, (Rn(19:16)=1111 && S(20)=0) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case6_TestCase6) {
  Binary2RegisterImmediateOpTester_Case6 baseline_tester;
  NamedDefs12To15_SUB_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010010snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=1111
//    = {actual: 'Defs12To15',
//       baseline: 'Unary1RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc001001001111ddddiiiiiiiiiiii',
//       rule: 'ADR_A2',
//       safety: ['inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0010x & Rn(19:16)=1111
//    = {Rd: Rd(15:12),
//       actual: Defs12To15,
//       baseline: Unary1RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       pattern: cccc001001001111ddddiiiiiiiiiiii,
//       rule: ADR_A2,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case7_TestCase7) {
  Unary1RegisterImmediateOpTester_Case7 baseline_tester;
  NamedDefs12To15_ADR_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001001001111ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0011x
//    = {actual: 'Defs12To15',
//       baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0010011snnnnddddiiiiiiiiiiii',
//       rule: 'RSB_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Defs12To15,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0010011snnnnddddiiiiiiiiiiii,
//       rule: RSB_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case8_TestCase8) {
  Binary2RegisterImmediateOpTester_Case8 baseline_tester;
  NamedDefs12To15_RSB_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010011snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = {actual: 'Defs12To15',
//       baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0010100snnnnddddiiiiiiiiiiii',
//       rule: 'ADD_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', '(inst(19:16)=1111 && inst(20)=0) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Defs12To15,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12)],
//       pattern: cccc0010100snnnnddddiiiiiiiiiiii,
//       rule: ADD_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, (Rn(19:16)=1111 && S(20)=0) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case9_TestCase9) {
  Binary2RegisterImmediateOpTester_Case9 baseline_tester;
  NamedDefs12To15_ADD_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010100snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=1111
//    = {actual: 'Defs12To15',
//       baseline: 'Unary1RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc001010001111ddddiiiiiiiiiiii',
//       rule: 'ADR_A1',
//       safety: ['inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0100x & Rn(19:16)=1111
//    = {Rd: Rd(15:12),
//       actual: Defs12To15,
//       baseline: Unary1RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12)],
//       pattern: cccc001010001111ddddiiiiiiiiiiii,
//       rule: ADR_A1,
//       safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case10_TestCase10) {
  Unary1RegisterImmediateOpTester_Case10 baseline_tester;
  NamedDefs12To15_ADR_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001010001111ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0101x
//    = {actual: 'Defs12To15',
//       baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0010101snnnnddddiiiiiiiiiiii',
//       rule: 'ADC_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Defs12To15,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0010101snnnnddddiiiiiiiiiiii,
//       rule: ADC_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case11_TestCase11) {
  Binary2RegisterImmediateOpTester_Case11 baseline_tester;
  NamedDefs12To15_ADC_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010101snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0110x
//    = {actual: 'Defs12To15',
//       baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0010110snnnnddddiiiiiiiiiiii',
//       rule: 'SBC_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Defs12To15,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0010110snnnnddddiiiiiiiiiiii,
//       rule: SBC_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case12_TestCase12) {
  Binary2RegisterImmediateOpTester_Case12 baseline_tester;
  NamedDefs12To15_SBC_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010110snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0111x
//    = {actual: 'Defs12To15',
//       baseline: 'Binary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0010111snnnnddddiiiiiiiiiiii',
//       rule: 'RSC_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Defs12To15,
//       baseline: Binary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0010111snnnnddddiiiiiiiiiiii,
//       rule: RSC_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case13_TestCase13) {
  Binary2RegisterImmediateOpTester_Case13 baseline_tester;
  NamedDefs12To15_RSC_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010111snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1100x
//    = {actual: 'Binary2RegisterImmediateOpDynCodeReplace',
//       baseline: 'Binary2RegisterImmediateOpDynCodeReplace',
//       constraints: & not (inst(15:12)=1111 && inst(20)=1) ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0011100snnnnddddiiiiiiiiiiii',
//       rule: 'ORR_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Binary2RegisterImmediateOpDynCodeReplace,
//       baseline: Binary2RegisterImmediateOpDynCodeReplace,
//       constraints: & not (Rd(15:12)=1111 && S(20)=1) ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0011100snnnnddddiiiiiiiiiiii,
//       rule: ORR_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpDynCodeReplaceTester_Case14_TestCase14) {
  Binary2RegisterImmediateOpDynCodeReplaceTester_Case14 tester;
  tester.Test("cccc0011100snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary1RegisterImmediateOpDynCodeReplace',
//       baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0011101s0000ddddiiiiiiiiiiii',
//       rule: 'MOV_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary1RegisterImmediateOpDynCodeReplace,
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0011101s0000ddddiiiiiiiiiiii,
//       rule: MOV_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpDynCodeReplaceTester_Case15_TestCase15) {
  Unary1RegisterImmediateOpDynCodeReplaceTester_Case15 tester;
  tester.Test("cccc0011101s0000ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1110x
//    = {actual: 'MaskAddress',
//       baseline: 'MaskedBinary2RegisterImmediateOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0011110snnnnddddiiiiiiiiiiii',
//       rule: 'BIC_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: MaskAddress,
//       baseline: MaskedBinary2RegisterImmediateOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0011110snnnnddddiiiiiiiiiiii,
//       rule: BIC_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       MaskedBinary2RegisterImmediateOpTester_Case16_TestCase16) {
  MaskedBinary2RegisterImmediateOpTester_Case16 baseline_tester;
  NamedMaskAddress_BIC_immediate_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011110snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary1RegisterImmediateOpDynCodeReplace',
//       baseline: 'Unary1RegisterImmediateOpDynCodeReplace',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0011111s0000ddddiiiiiiiiiiii',
//       rule: 'MVN_immediate_A1',
//       safety: ['(inst(15:12)=1111 && inst(20)=1) => DECODER_ERROR', 'inst(15:12)=1111 => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       S: S(20),
//       actual: Unary1RegisterImmediateOpDynCodeReplace,
//       baseline: Unary1RegisterImmediateOpDynCodeReplace,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12)],
//       pattern: cccc0011111s0000ddddiiiiiiiiiiii,
//       rule: MVN_immediate_A1,
//       safety: [(Rd(15:12)=1111 && S(20)=1) => DECODER_ERROR, Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpDynCodeReplaceTester_Case17_TestCase17) {
  Unary1RegisterImmediateOpDynCodeReplaceTester_Case17 tester;
  tester.Test("cccc0011111s0000ddddiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
