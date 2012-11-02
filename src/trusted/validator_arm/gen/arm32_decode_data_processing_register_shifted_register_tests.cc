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
//    = {baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTesterCase0
    : public Binary3RegisterShiftedTestTester {
 public:
  Binary3RegisterShiftedTestTesterCase0(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedTestTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(16))));

  return true;
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTesterCase1
    : public Binary3RegisterShiftedTestTester {
 public:
  Binary3RegisterShiftedTestTesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedTestTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(16))));

  return true;
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTesterCase2
    : public Binary3RegisterShiftedTestTester {
 public:
  Binary3RegisterShiftedTestTesterCase2(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedTestTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(16))));

  return true;
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTesterCase3
    : public Binary3RegisterShiftedTestTester {
 public:
  Binary3RegisterShiftedTestTesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20)=~10111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterShiftedTestTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {NZCV};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(16))));

  return true;
}

// Neutral case:
// inst(24:20)=0000x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase4
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase4(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0001x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase5
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase5(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0010x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase6
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase6(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0011x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase7
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase7(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0100x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase8
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0101x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase9
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase9(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0110x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase10
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase10(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=0111x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase11
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1100x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase12
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase12(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary3RegisterOpTesterCase13
    : public Binary3RegisterOpTester {
 public:
  Binary3RegisterOpTesterCase13(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(6:5)=~00 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary3RegisterOpTesterCase14
    : public Binary3RegisterOpTester {
 public:
  Binary3RegisterOpTesterCase14(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary3RegisterOpTesterCase15
    : public Binary3RegisterOpTester {
 public:
  Binary3RegisterOpTesterCase15(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary3RegisterOpTesterCase16
    : public Binary3RegisterOpTester {
 public:
  Binary3RegisterOpTesterCase16(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rn,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1110x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTesterCase17
    : public Binary4RegisterShiftedOpTester {
 public:
  Binary4RegisterShiftedOpTesterCase17(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterShiftedOpTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) || (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd,NZCV if setflags else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x00100000) == 0x00100000 ? 16 : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Unary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rs(11:8), Rm(3:0)],
//       safety: [Pc in {Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Unary3RegisterShiftedOpTesterCase18
    : public Unary3RegisterShiftedOpTester {
 public:
  Unary3RegisterShiftedOpTesterCase18(const NamedClassDecoder& decoder)
    : Unary3RegisterShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary3RegisterShiftedOpTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary3RegisterShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary3RegisterShiftedOpTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary3RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd,Rm,Rs} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F)))) || (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

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
//    = {baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       rule: 'TST_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       rule: TST_register_shifted_register,
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTester_Case0
    : public Binary3RegisterShiftedTestTesterCase0 {
 public:
  Binary3RegisterShiftedTestTester_Case0()
    : Binary3RegisterShiftedTestTesterCase0(
      state_.Binary3RegisterShiftedTest_TST_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       rule: 'TEQ_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       rule: TEQ_register_shifted_register,
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTester_Case1
    : public Binary3RegisterShiftedTestTesterCase1 {
 public:
  Binary3RegisterShiftedTestTester_Case1()
    : Binary3RegisterShiftedTestTesterCase1(
      state_.Binary3RegisterShiftedTest_TEQ_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       rule: 'CMP_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       rule: CMP_register_shifted_register,
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTester_Case2
    : public Binary3RegisterShiftedTestTesterCase2 {
 public:
  Binary3RegisterShiftedTestTester_Case2()
    : Binary3RegisterShiftedTestTesterCase2(
      state_.Binary3RegisterShiftedTest_CMP_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       rule: 'CMN_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       rule: CMN_register_shifted_register,
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
class Binary3RegisterShiftedTestTester_Case3
    : public Binary3RegisterShiftedTestTesterCase3 {
 public:
  Binary3RegisterShiftedTestTester_Case3()
    : Binary3RegisterShiftedTestTesterCase3(
      state_.Binary3RegisterShiftedTest_CMN_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'AND_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: AND_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case4
    : public Binary4RegisterShiftedOpTesterCase4 {
 public:
  Binary4RegisterShiftedOpTester_Case4()
    : Binary4RegisterShiftedOpTesterCase4(
      state_.Binary4RegisterShiftedOp_AND_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'EOR_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: EOR_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case5
    : public Binary4RegisterShiftedOpTesterCase5 {
 public:
  Binary4RegisterShiftedOpTester_Case5()
    : Binary4RegisterShiftedOpTesterCase5(
      state_.Binary4RegisterShiftedOp_EOR_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'SUB_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: SUB_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case6
    : public Binary4RegisterShiftedOpTesterCase6 {
 public:
  Binary4RegisterShiftedOpTester_Case6()
    : Binary4RegisterShiftedOpTesterCase6(
      state_.Binary4RegisterShiftedOp_SUB_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'RSB_register_shfited_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: RSB_register_shfited_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case7
    : public Binary4RegisterShiftedOpTesterCase7 {
 public:
  Binary4RegisterShiftedOpTester_Case7()
    : Binary4RegisterShiftedOpTesterCase7(
      state_.Binary4RegisterShiftedOp_RSB_register_shfited_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'ADD_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: ADD_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case8
    : public Binary4RegisterShiftedOpTesterCase8 {
 public:
  Binary4RegisterShiftedOpTester_Case8()
    : Binary4RegisterShiftedOpTesterCase8(
      state_.Binary4RegisterShiftedOp_ADD_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'ADC_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: ADC_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case9
    : public Binary4RegisterShiftedOpTesterCase9 {
 public:
  Binary4RegisterShiftedOpTester_Case9()
    : Binary4RegisterShiftedOpTesterCase9(
      state_.Binary4RegisterShiftedOp_ADC_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'SBC_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: SBC_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case10
    : public Binary4RegisterShiftedOpTesterCase10 {
 public:
  Binary4RegisterShiftedOpTester_Case10()
    : Binary4RegisterShiftedOpTesterCase10(
      state_.Binary4RegisterShiftedOp_SBC_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'RSC_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: RSC_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case11
    : public Binary4RegisterShiftedOpTesterCase11 {
 public:
  Binary4RegisterShiftedOpTester_Case11()
    : Binary4RegisterShiftedOpTesterCase11(
      state_.Binary4RegisterShiftedOp_RSC_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'ORR_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: ORR_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case12
    : public Binary4RegisterShiftedOpTesterCase12 {
 public:
  Binary4RegisterShiftedOpTester_Case12()
    : Binary4RegisterShiftedOpTesterCase12(
      state_.Binary4RegisterShiftedOp_ORR_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'LSL_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       rule: LSL_register,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary3RegisterOpTester_Case13
    : public Binary3RegisterOpTesterCase13 {
 public:
  Binary3RegisterOpTester_Case13()
    : Binary3RegisterOpTesterCase13(
      state_.Binary3RegisterOp_LSL_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'LSR_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       rule: LSR_register,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary3RegisterOpTester_Case14
    : public Binary3RegisterOpTesterCase14 {
 public:
  Binary3RegisterOpTester_Case14()
    : Binary3RegisterOpTesterCase14(
      state_.Binary3RegisterOp_LSR_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'ASR_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       rule: ASR_register,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary3RegisterOpTester_Case15
    : public Binary3RegisterOpTesterCase15 {
 public:
  Binary3RegisterOpTester_Case15()
    : Binary3RegisterOpTesterCase15(
      state_.Binary3RegisterOp_ASR_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'ROR_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       rule: ROR_register,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary3RegisterOpTester_Case16
    : public Binary3RegisterOpTesterCase16 {
 public:
  Binary3RegisterOpTester_Case16()
    : Binary3RegisterOpTesterCase16(
      state_.Binary3RegisterOp_ROR_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = {baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'BIC_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: BIC_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Binary4RegisterShiftedOpTester_Case17
    : public Binary4RegisterShiftedOpTesterCase17 {
 public:
  Binary4RegisterShiftedOpTester_Case17()
    : Binary4RegisterShiftedOpTesterCase17(
      state_.Binary4RegisterShiftedOp_BIC_register_shifted_register_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {baseline: 'Unary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       rule: 'MVN_register_shifted_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rs: Rs(11:8),
//       S: S(20),
//       baseline: Unary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rs(11:8), Rm(3:0)],
//       rule: MVN_register_shifted_register,
//       safety: [Pc in {Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
class Unary3RegisterShiftedOpTester_Case18
    : public Unary3RegisterShiftedOpTesterCase18 {
 public:
  Unary3RegisterShiftedOpTester_Case18()
    : Unary3RegisterShiftedOpTesterCase18(
      state_.Unary3RegisterShiftedOp_MVN_register_shifted_register_instance_)
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
//    = {actual: 'Binary3RegisterShiftedTest',
//       baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       pattern: 'cccc00010001nnnn0000ssss0tt1mmmm',
//       rule: 'TST_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       actual: Binary3RegisterShiftedTest,
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       pattern: cccc00010001nnnn0000ssss0tt1mmmm,
//       rule: TST_register_shifted_register,
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case0_TestCase0) {
  Binary3RegisterShiftedTestTester_Case0 tester;
  tester.Test("cccc00010001nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'Binary3RegisterShiftedTest',
//       baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       pattern: 'cccc00010011nnnn0000ssss0tt1mmmm',
//       rule: 'TEQ_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       actual: Binary3RegisterShiftedTest,
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       pattern: cccc00010011nnnn0000ssss0tt1mmmm,
//       rule: TEQ_register_shifted_register,
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case1_TestCase1) {
  Binary3RegisterShiftedTestTester_Case1 tester;
  tester.Test("cccc00010011nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'Binary3RegisterShiftedTest',
//       baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       pattern: 'cccc00010101nnnn0000ssss0tt1mmmm',
//       rule: 'CMP_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       actual: Binary3RegisterShiftedTest,
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       pattern: cccc00010101nnnn0000ssss0tt1mmmm,
//       rule: CMP_register_shifted_register,
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case2_TestCase2) {
  Binary3RegisterShiftedTestTester_Case2 tester;
  tester.Test("cccc00010101nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {actual: 'Binary3RegisterShiftedTest',
//       baseline: 'Binary3RegisterShiftedTest',
//       constraints: ,
//       defs: {16},
//       pattern: 'cccc00010111nnnn0000ssss0tt1mmmm',
//       rule: 'CMN_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       actual: Binary3RegisterShiftedTest,
//       baseline: Binary3RegisterShiftedTest,
//       constraints: ,
//       defs: {NZCV},
//       fields: [Rn(19:16), Rs(11:8), Rm(3:0)],
//       pattern: cccc00010111nnnn0000ssss0tt1mmmm,
//       rule: CMN_register_shifted_register,
//       safety: [Pc in {Rn,Rm,Rs} => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case3_TestCase3) {
  Binary3RegisterShiftedTestTester_Case3 tester;
  tester.Test("cccc00010111nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0000x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0000000snnnnddddssss0tt1mmmm',
//       rule: 'AND_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0000000snnnnddddssss0tt1mmmm,
//       rule: AND_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case4_TestCase4) {
  Binary4RegisterShiftedOpTester_Case4 tester;
  tester.Test("cccc0000000snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0001x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0000001snnnnddddssss0tt1mmmm',
//       rule: 'EOR_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0000001snnnnddddssss0tt1mmmm,
//       rule: EOR_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case5_TestCase5) {
  Binary4RegisterShiftedOpTester_Case5 tester;
  tester.Test("cccc0000001snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0010x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0000010snnnnddddssss0tt1mmmm',
//       rule: 'SUB_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0000010snnnnddddssss0tt1mmmm,
//       rule: SUB_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case6_TestCase6) {
  Binary4RegisterShiftedOpTester_Case6 tester;
  tester.Test("cccc0000010snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0011x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0000011snnnnddddssss0tt1mmmm',
//       rule: 'RSB_register_shfited_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0000011snnnnddddssss0tt1mmmm,
//       rule: RSB_register_shfited_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case7_TestCase7) {
  Binary4RegisterShiftedOpTester_Case7 tester;
  tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0100x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0000100snnnnddddssss0tt1mmmm',
//       rule: 'ADD_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0000100snnnnddddssss0tt1mmmm,
//       rule: ADD_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case8_TestCase8) {
  Binary4RegisterShiftedOpTester_Case8 tester;
  tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0101x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0000101snnnnddddssss0tt1mmmm',
//       rule: 'ADC_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0000101snnnnddddssss0tt1mmmm,
//       rule: ADC_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case9_TestCase9) {
  Binary4RegisterShiftedOpTester_Case9 tester;
  tester.Test("cccc0000101snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0110x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0000110snnnnddddssss0tt1mmmm',
//       rule: 'SBC_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0000110snnnnddddssss0tt1mmmm,
//       rule: SBC_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case10_TestCase10) {
  Binary4RegisterShiftedOpTester_Case10 tester;
  tester.Test("cccc0000110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0111x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0000111snnnnddddssss0tt1mmmm',
//       rule: 'RSC_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0000111snnnnddddssss0tt1mmmm,
//       rule: RSC_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case11_TestCase11) {
  Binary4RegisterShiftedOpTester_Case11 tester;
  tester.Test("cccc0000111snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1100x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0001100snnnnddddssss0tt1mmmm',
//       rule: 'ORR_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0001100snnnnddddssss0tt1mmmm,
//       rule: ORR_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case12_TestCase12) {
  Binary4RegisterShiftedOpTester_Case12 tester;
  tester.Test("cccc0001100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Binary3RegisterOp',
//       baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0001101s0000ddddmmmm0001nnnn',
//       rule: 'LSL_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       actual: Binary3RegisterOp,
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       pattern: cccc0001101s0000ddddmmmm0001nnnn,
//       rule: LSL_register,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case13_TestCase13) {
  Binary3RegisterOpTester_Case13 tester;
  tester.Test("cccc0001101s0000ddddmmmm0001nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Binary3RegisterOp',
//       baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0001101s0000ddddmmmm0011nnnn',
//       rule: 'LSR_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       actual: Binary3RegisterOp,
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       pattern: cccc0001101s0000ddddmmmm0011nnnn,
//       rule: LSR_register,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case14_TestCase14) {
  Binary3RegisterOpTester_Case14 tester;
  tester.Test("cccc0001101s0000ddddmmmm0011nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Binary3RegisterOp',
//       baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0001101s0000ddddmmmm0101nnnn',
//       rule: 'ASR_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       actual: Binary3RegisterOp,
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       pattern: cccc0001101s0000ddddmmmm0101nnnn,
//       rule: ASR_register,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case15_TestCase15) {
  Binary3RegisterOpTester_Case15 tester;
  tester.Test("cccc0001101s0000ddddmmmm0101nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Binary3RegisterOp',
//       baseline: 'Binary3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0001101s0000ddddmmmm0111nnnn',
//       rule: 'ROR_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       S: S(20),
//       actual: Binary3RegisterOp,
//       baseline: Binary3RegisterOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//       pattern: cccc0001101s0000ddddmmmm0111nnnn,
//       rule: ROR_register,
//       safety: [Pc in {Rd,Rn,Rm} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case16_TestCase16) {
  Binary3RegisterOpTester_Case16 tester;
  tester.Test("cccc0001101s0000ddddmmmm0111nnnn");
}

// Neutral case:
// inst(24:20)=1110x
//    = {actual: 'Binary4RegisterShiftedOp',
//       baseline: 'Binary4RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0001110snnnnddddssss0tt1mmmm',
//       rule: 'BIC_register_shifted_register',
//       safety: ['15 == inst(19:16) || 15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Binary4RegisterShiftedOp,
//       baseline: Binary4RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0001110snnnnddddssss0tt1mmmm,
//       rule: BIC_register_shifted_register,
//       safety: [Pc in {Rn,Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case17_TestCase17) {
  Binary4RegisterShiftedOpTester_Case17 tester;
  tester.Test("cccc0001110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {actual: 'Unary3RegisterShiftedOp',
//       baseline: 'Unary3RegisterShiftedOp',
//       constraints: ,
//       defs: {inst(15:12),16 if inst(20)=1 else 32},
//       pattern: 'cccc0001111s0000ddddssss0tt1mmmm',
//       rule: 'MVN_register_shifted_register',
//       safety: ['15 == inst(15:12) || 15 == inst(3:0) || 15 == inst(11:8) => UNPREDICTABLE']}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rs: Rs(11:8),
//       S: S(20),
//       actual: Unary3RegisterShiftedOp,
//       baseline: Unary3RegisterShiftedOp,
//       constraints: ,
//       defs: {Rd,NZCV if setflags else None},
//       fields: [S(20), Rd(15:12), Rs(11:8), Rm(3:0)],
//       pattern: cccc0001111s0000ddddssss0tt1mmmm,
//       rule: MVN_register_shifted_register,
//       safety: [Pc in {Rd,Rm,Rs} => UNPREDICTABLE],
//       setflags: S(20)=1}
TEST_F(Arm32DecoderStateTests,
       Unary3RegisterShiftedOpTester_Case18_TestCase18) {
  Unary3RegisterShiftedOpTester_Case18 tester;
  tester.Test("cccc0001111s0000ddddssss0tt1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
