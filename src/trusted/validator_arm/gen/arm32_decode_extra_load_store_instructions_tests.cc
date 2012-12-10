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
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Store3RegisterOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store3RegisterOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore3RegisterOpTesterCase0
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase0(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterOpTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Pc in {Rt,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // safety: wback && (Rn == Pc) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))))));

  // safety: ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) && ((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000) == 0x01000000));

  // defs: {base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore3RegisterOpTesterCase1
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase1(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterOpTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Pc in {Rt,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // safety: wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) && ((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000) == 0x01000000));

  // defs: {Rt,base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x0
//    = {baseline: 'Store2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store2RegisterImm8Op,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore2RegisterImm8OpTesterCase2
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase2(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8OpTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm8OpTester::ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Rt == Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // safety: wback && (Rn == Pc) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))))));

  // defs: {base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore2RegisterImm8OpTesterCase3
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase3(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8OpTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm8OpTester::ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000) != 0x000F0000);

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x0000F000) >> 12)) == (15))) || ((((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: Rt == Pc => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt,base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = {baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
class LoadRegisterImm8OpTesterCase4
    : public LoadRegisterImm8OpTester {
 public:
  LoadRegisterImm8OpTesterCase4(const NamedClassDecoder& decoder)
    : LoadRegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadRegisterImm8OpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadRegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadRegisterImm8OpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadRegisterImm8OpTester::ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: P == W => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x01000000) >> 24)) != (((inst.Bits() & 0x00200000) >> 21))));

  // safety: Rt == Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterDoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1,inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 || 15 == inst(3:0) || inst(15:12) == inst(3:0) || inst(15:12) + 1 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterDoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, Rt2 == Pc || Rm == Pc || Rm == Rt || Rm == Rt2 => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore3RegisterDoubleOpTesterCase5
    : public LoadStore3RegisterDoubleOpTester {
 public:
  LoadStore3RegisterDoubleOpTesterCase5(const NamedClassDecoder& decoder)
    : LoadStore3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterDoubleOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterDoubleOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterDoubleOpTester::ApplySanityChecks(inst, decoder));

  // safety: Rt(0)=1 => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) != 0x00000001);

  // safety: P(24)=0 && W(21)=1 => UNPREDICTABLE
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Rt2 == Pc || Rm == Pc || Rm == Rt || Rm == Rt2 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x0000F000) >> 12) + 1) == (15))) || ((((inst.Bits() & 0x0000000F)) == (15))) || ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x0000F000) >> 12)))) || ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x0000F000) >> 12) + 1)))));

  // safety: wback && (Rn == Pc) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))))));

  // safety: ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) && ((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000) == 0x01000000));

  // defs: {Rt,Rt2,base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1)).Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore3RegisterOpTesterCase6
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase6(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterOpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Pc in {Rt,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // safety: wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) && ((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000) == 0x01000000));

  // defs: {Rt,base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1,inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (inst(15:12) == inst(19:16) || inst(15:12) + 1 == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm8DoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, wback && (Rn == Rt || Rn == Rt2) => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore2RegisterImm8DoubleOpTesterCase7
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterCase7(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8DoubleOpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm8DoubleOpTester::ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000) != 0x000F0000);

  // safety: Rt(0)=1 => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) != 0x00000001);

  // safety: P(24)=0 && W(21)=1 => UNPREDICTABLE
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: wback && (Rn == Rt || Rn == Rt2) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && (((((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12) + 1)))))));

  // safety: Rt2 == Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12) + 1) != (15)));

  // defs: {Rt,Rt2,base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1)).Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'LoadRegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1},
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       baseline: LoadRegisterImm8DoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2},
//       fields: [Rt(15:12)],
//       safety: [Rt(0)=1 => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE]}
class LoadRegisterImm8DoubleOpTesterCase8
    : public LoadRegisterImm8DoubleOpTester {
 public:
  LoadRegisterImm8DoubleOpTesterCase8(const NamedClassDecoder& decoder)
    : LoadRegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadRegisterImm8DoubleOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadRegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadRegisterImm8DoubleOpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadRegisterImm8DoubleOpTester::ApplySanityChecks(inst, decoder));

  // safety: Rt(0)=1 => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) != 0x00000001);

  // safety: Rt2 == Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12) + 1) != (15)));

  // defs: {Rt,Rt2};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1))));

  return true;
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore2RegisterImm8OpTesterCase9
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase9(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8OpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm8OpTester::ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000) != 0x000F0000);

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x0000F000) >> 12)) == (15))) || ((((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: Rt == Pc => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt,base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
class LoadRegisterImm8OpTesterCase10
    : public LoadRegisterImm8OpTester {
 public:
  LoadRegisterImm8OpTesterCase10(const NamedClassDecoder& decoder)
    : LoadRegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadRegisterImm8OpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadRegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadRegisterImm8OpTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadRegisterImm8OpTester::ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: P == W => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x01000000) >> 24)) != (((inst.Bits() & 0x00200000) >> 21))));

  // safety: Rt == Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Store3RegisterDoubleOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       base: Rn,
//       baseline: Store3RegisterDoubleOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, Rt2 == Pc || Rm == Pc => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore3RegisterDoubleOpTesterCase11
    : public LoadStore3RegisterDoubleOpTester {
 public:
  LoadStore3RegisterDoubleOpTesterCase11(const NamedClassDecoder& decoder)
    : LoadStore3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterDoubleOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterDoubleOpTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterDoubleOpTester::ApplySanityChecks(inst, decoder));

  // safety: Rt(0)=1 => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) != 0x00000001);

  // safety: P(24)=0 && W(21)=1 => UNPREDICTABLE
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Rt2 == Pc || Rm == Pc => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x0000F000) >> 12) + 1) == (15))) || ((((inst.Bits() & 0x0000000F)) == (15)))));

  // safety: wback && (Rn == Pc) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))))));

  // safety: ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) && ((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000) == 0x01000000));

  // defs: {base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore3RegisterOpTesterCase12
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase12(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterOpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Pc in {Rt,Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) || (((15) == ((inst.Bits() & 0x0000000F))))));

  // safety: wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) && ((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000) == 0x01000000));

  // defs: {Rt,base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x0
//    = {baseline: 'Store2RegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       base: Rn,
//       baseline: Store2RegisterImm8DoubleOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore2RegisterImm8DoubleOpTesterCase13
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterCase13(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8DoubleOpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm8DoubleOpTester::ApplySanityChecks(inst, decoder));

  // safety: Rt(0)=1 => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001) != 0x00000001);

  // safety: P(24)=0 && W(21)=1 => UNPREDICTABLE
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: wback && (Rn == Pc) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))))));

  // safety: Rt2 == Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12) + 1) != (15)));

  // defs: {base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
class LoadStore2RegisterImm8OpTesterCase14
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase14(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm8OpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm8OpTester::ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000) != 0x000F0000);

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x0000F000) >> 12)) == (15))) || ((((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000))) && (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: Rt == Pc => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt,base if wback else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12))).Add(Register(((((inst.Bits() & 0x01000000) == 0x00000000)) || (((inst.Bits() & 0x00200000) == 0x00200000)) ? ((inst.Bits() & 0x000F0000) >> 16) : 32)))));

  return true;
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
class LoadRegisterImm8OpTesterCase15
    : public LoadRegisterImm8OpTester {
 public:
  LoadRegisterImm8OpTesterCase15(const NamedClassDecoder& decoder)
    : LoadRegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadRegisterImm8OpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadRegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadRegisterImm8OpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadRegisterImm8OpTester::ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 && W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000) == 0x00000000) && ((inst.Bits() & 0x00200000) == 0x00200000)));

  // safety: P == W => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x01000000) >> 24)) != (((inst.Bits() & 0x00200000) >> 21))));

  // safety: Rt == Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Store3RegisterOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'STRH_register',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store3RegisterOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: STRH_register,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class Store3RegisterOpTester_Case0
    : public LoadStore3RegisterOpTesterCase0 {
 public:
  Store3RegisterOpTester_Case0()
    : LoadStore3RegisterOpTesterCase0(
      state_.Store3RegisterOp_STRH_register_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'LDRH_register',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: LDRH_register,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class Load3RegisterOpTester_Case1
    : public LoadStore3RegisterOpTesterCase1 {
 public:
  Load3RegisterOpTester_Case1()
    : LoadStore3RegisterOpTesterCase1(
      state_.Load3RegisterOp_LDRH_register_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x0
//    = {baseline: 'Store2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'STRH_immediate',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE']}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store2RegisterImm8Op,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: STRH_immediate,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
class Store2RegisterImm8OpTester_Case2
    : public LoadStore2RegisterImm8OpTesterCase2 {
 public:
  Store2RegisterImm8OpTester_Case2()
    : LoadStore2RegisterImm8OpTesterCase2(
      state_.Store2RegisterImm8Op_STRH_immediate_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'LDRH_immediate',
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: LDRH_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
class Load2RegisterImm8OpTester_Case3
    : public LoadStore2RegisterImm8OpTesterCase3 {
 public:
  Load2RegisterImm8OpTester_Case3()
    : LoadStore2RegisterImm8OpTesterCase3(
      state_.Load2RegisterImm8Op_LDRH_immediate_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = {baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'LDRH_literal',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       rule: LDRH_literal,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
class LoadRegisterImm8OpTester_Case4
    : public LoadRegisterImm8OpTesterCase4 {
 public:
  LoadRegisterImm8OpTester_Case4()
    : LoadRegisterImm8OpTesterCase4(
      state_.LoadRegisterImm8Op_LDRH_literal_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterDoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1,inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'LDRD_register',
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 || 15 == inst(3:0) || inst(15:12) == inst(3:0) || inst(15:12) + 1 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterDoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: LDRD_register,
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, Rt2 == Pc || Rm == Pc || Rm == Rt || Rm == Rt2 => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class Load3RegisterDoubleOpTester_Case5
    : public LoadStore3RegisterDoubleOpTesterCase5 {
 public:
  Load3RegisterDoubleOpTester_Case5()
    : LoadStore3RegisterDoubleOpTesterCase5(
      state_.Load3RegisterDoubleOp_LDRD_register_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'LDRSB_register',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: LDRSB_register,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class Load3RegisterOpTester_Case6
    : public LoadStore3RegisterOpTesterCase6 {
 public:
  Load3RegisterOpTester_Case6()
    : LoadStore3RegisterOpTesterCase6(
      state_.Load3RegisterOp_LDRSB_register_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1,inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'LDRD_immediate',
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (inst(15:12) == inst(19:16) || inst(15:12) + 1 == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm8DoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: LDRD_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, wback && (Rn == Rt || Rn == Rt2) => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
class Load2RegisterImm8DoubleOpTester_Case7
    : public LoadStore2RegisterImm8DoubleOpTesterCase7 {
 public:
  Load2RegisterImm8DoubleOpTester_Case7()
    : LoadStore2RegisterImm8DoubleOpTesterCase7(
      state_.Load2RegisterImm8DoubleOp_LDRD_immediate_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'LoadRegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1},
//       rule: 'LDRD_literal',
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       baseline: LoadRegisterImm8DoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2},
//       fields: [Rt(15:12)],
//       rule: LDRD_literal,
//       safety: [Rt(0)=1 => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE]}
class LoadRegisterImm8DoubleOpTester_Case8
    : public LoadRegisterImm8DoubleOpTesterCase8 {
 public:
  LoadRegisterImm8DoubleOpTester_Case8()
    : LoadRegisterImm8DoubleOpTesterCase8(
      state_.LoadRegisterImm8DoubleOp_LDRD_literal_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'LDRSB_immediate',
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: LDRSB_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
class Load2RegisterImm8OpTester_Case9
    : public LoadStore2RegisterImm8OpTesterCase9 {
 public:
  Load2RegisterImm8OpTester_Case9()
    : LoadStore2RegisterImm8OpTesterCase9(
      state_.Load2RegisterImm8Op_LDRSB_immediate_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'LDRSB_literal',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       rule: LDRSB_literal,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
class LoadRegisterImm8OpTester_Case10
    : public LoadRegisterImm8OpTesterCase10 {
 public:
  LoadRegisterImm8OpTester_Case10()
    : LoadRegisterImm8OpTesterCase10(
      state_.LoadRegisterImm8Op_LDRSB_literal_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Store3RegisterDoubleOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'STRD_register',
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       base: Rn,
//       baseline: Store3RegisterDoubleOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: STRD_register,
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, Rt2 == Pc || Rm == Pc => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class Store3RegisterDoubleOpTester_Case11
    : public LoadStore3RegisterDoubleOpTesterCase11 {
 public:
  Store3RegisterDoubleOpTester_Case11()
    : LoadStore3RegisterDoubleOpTesterCase11(
      state_.Store3RegisterDoubleOp_STRD_register_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'LDRSH_register',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: LDRSH_register,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
class Load3RegisterOpTester_Case12
    : public LoadStore3RegisterOpTesterCase12 {
 public:
  Load3RegisterOpTester_Case12()
    : LoadStore3RegisterOpTesterCase12(
      state_.Load3RegisterOp_LDRSH_register_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x0
//    = {baseline: 'Store2RegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'STRD_immediate',
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       base: Rn,
//       baseline: Store2RegisterImm8DoubleOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: STRD_immediate,
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
class Store2RegisterImm8DoubleOpTester_Case13
    : public LoadStore2RegisterImm8DoubleOpTesterCase13 {
 public:
  Store2RegisterImm8DoubleOpTester_Case13()
    : LoadStore2RegisterImm8DoubleOpTesterCase13(
      state_.Store2RegisterImm8DoubleOp_STRD_immediate_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       rule: 'LDRSH_immediate',
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: LDRSH_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
class Load2RegisterImm8OpTester_Case14
    : public LoadStore2RegisterImm8OpTesterCase14 {
 public:
  Load2RegisterImm8OpTester_Case14()
    : LoadStore2RegisterImm8OpTesterCase14(
      state_.Load2RegisterImm8Op_LDRSH_immediate_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'LDRSB_literal',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       rule: LDRSB_literal,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
class LoadRegisterImm8OpTester_Case15
    : public LoadRegisterImm8OpTesterCase15 {
 public:
  LoadRegisterImm8OpTester_Case15()
    : LoadRegisterImm8OpTesterCase15(
      state_.LoadRegisterImm8Op_LDRSB_literal_instance_)
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
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Store3RegisterOp',
//       baseline: 'Store3RegisterOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu0w0nnnntttt00001011mmmm',
//       rule: 'STRH_register',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Store3RegisterOp,
//       base: Rn,
//       baseline: Store3RegisterOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc000pu0w0nnnntttt00001011mmmm,
//       rule: STRH_register,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterOpTester_Case0_TestCase0) {
  Store3RegisterOpTester_Case0 tester;
  tester.Test("cccc000pu0w0nnnntttt00001011mmmm");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Load3RegisterOp',
//       baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu0w1nnnntttt00001011mmmm',
//       rule: 'LDRH_register',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load3RegisterOp,
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc000pu0w1nnnntttt00001011mmmm,
//       rule: LDRH_register,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case1_TestCase1) {
  Load3RegisterOpTester_Case1 tester;
  tester.Test("cccc000pu0w1nnnntttt00001011mmmm");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x0
//    = {actual: 'Store2RegisterImm8Op',
//       baseline: 'Store2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu1w0nnnnttttiiii1011iiii',
//       rule: 'STRH_immediate',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Store2RegisterImm8Op,
//       base: Rn,
//       baseline: Store2RegisterImm8Op,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc000pu1w0nnnnttttiiii1011iiii,
//       rule: STRH_immediate,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8OpTester_Case2_TestCase2) {
  Store2RegisterImm8OpTester_Case2 tester;
  tester.Test("cccc000pu1w0nnnnttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {actual: 'Load2RegisterImm8Op',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu1w1nnnnttttiiii1011iiii',
//       rule: 'LDRH_immediate',
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load2RegisterImm8Op,
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc000pu1w1nnnnttttiiii1011iiii,
//       rule: LDRH_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case3_TestCase3) {
  Load2RegisterImm8OpTester_Case3 tester;
  tester.Test("cccc000pu1w1nnnnttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = {actual: 'LoadRegisterImm8Op',
//       baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc000pu1w11111ttttiiii1011iiii',
//       rule: 'LDRH_literal',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: LoadRegisterImm8Op,
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       pattern: cccc000pu1w11111ttttiiii1011iiii,
//       rule: LDRH_literal,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterImm8OpTester_Case4_TestCase4) {
  LoadRegisterImm8OpTester_Case4 tester;
  tester.Test("cccc000pu1w11111ttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Load3RegisterDoubleOp',
//       baseline: 'Load3RegisterDoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1,inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu0w0nnnntttt00001101mmmm',
//       rule: 'LDRD_register',
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 || 15 == inst(3:0) || inst(15:12) == inst(3:0) || inst(15:12) + 1 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       actual: Load3RegisterDoubleOp,
//       base: Rn,
//       baseline: Load3RegisterDoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc000pu0w0nnnntttt00001101mmmm,
//       rule: LDRD_register,
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, Rt2 == Pc || Rm == Pc || Rm == Rt || Rm == Rt2 => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterDoubleOpTester_Case5_TestCase5) {
  Load3RegisterDoubleOpTester_Case5 tester;
  tester.Test("cccc000pu0w0nnnntttt00001101mmmm");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Load3RegisterOp',
//       baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu0w1nnnntttt00001101mmmm',
//       rule: 'LDRSB_register',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load3RegisterOp,
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc000pu0w1nnnntttt00001101mmmm,
//       rule: LDRSB_register,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case6_TestCase6) {
  Load3RegisterOpTester_Case6 tester;
  tester.Test("cccc000pu0w1nnnntttt00001101mmmm");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=~1111
//    = {actual: 'Load2RegisterImm8DoubleOp',
//       baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1,inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu1w0nnnnttttiiii1101iiii',
//       rule: 'LDRD_immediate',
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (inst(15:12) == inst(19:16) || inst(15:12) + 1 == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       actual: Load2RegisterImm8DoubleOp,
//       base: Rn,
//       baseline: Load2RegisterImm8DoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc000pu1w0nnnnttttiiii1101iiii,
//       rule: LDRD_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, wback && (Rn == Rt || Rn == Rt2) => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_Case7_TestCase7) {
  Load2RegisterImm8DoubleOpTester_Case7 tester;
  tester.Test("cccc000pu1w0nnnnttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'LoadRegisterImm8DoubleOp',
//       baseline: 'LoadRegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(15:12),inst(15:12) + 1},
//       pattern: 'cccc0001u1001111ttttiiii1101iiii',
//       rule: 'LDRD_literal',
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       actual: LoadRegisterImm8DoubleOp,
//       baseline: LoadRegisterImm8DoubleOp,
//       constraints: ,
//       defs: {Rt,Rt2},
//       fields: [Rt(15:12)],
//       pattern: cccc0001u1001111ttttiiii1101iiii,
//       rule: LDRD_literal,
//       safety: [Rt(0)=1 => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterImm8DoubleOpTester_Case8_TestCase8) {
  LoadRegisterImm8DoubleOpTester_Case8 tester;
  tester.Test("cccc0001u1001111ttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {actual: 'Load2RegisterImm8Op',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu1w1nnnnttttiiii1101iiii',
//       rule: 'LDRSB_immediate',
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load2RegisterImm8Op,
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc000pu1w1nnnnttttiiii1101iiii,
//       rule: LDRSB_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case9_TestCase9) {
  Load2RegisterImm8OpTester_Case9 tester;
  tester.Test("cccc000pu1w1nnnnttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'LoadRegisterImm8Op',
//       baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc0001u1011111ttttiiii1101iiii',
//       rule: 'LDRSB_literal',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: LoadRegisterImm8Op,
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       pattern: cccc0001u1011111ttttiiii1101iiii,
//       rule: LDRSB_literal,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterImm8OpTester_Case10_TestCase10) {
  LoadRegisterImm8OpTester_Case10 tester;
  tester.Test("cccc0001u1011111ttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Store3RegisterDoubleOp',
//       baseline: 'Store3RegisterDoubleOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu0w0nnnntttt00001111mmmm',
//       rule: 'STRD_register',
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '15 == inst(15:12) + 1 || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       actual: Store3RegisterDoubleOp,
//       base: Rn,
//       baseline: Store3RegisterDoubleOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc000pu0w0nnnntttt00001111mmmm,
//       rule: STRD_register,
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, Rt2 == Pc || Rm == Pc => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterDoubleOpTester_Case11_TestCase11) {
  Store3RegisterDoubleOpTester_Case11 tester;
  tester.Test("cccc000pu0w0nnnntttt00001111mmmm");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Load3RegisterOp',
//       baseline: 'Load3RegisterOp',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu0w1nnnntttt00001111mmmm',
//       rule: 'LDRSH_register',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || 15 == inst(3:0) => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16) || inst(15:12) == inst(19:16)) => UNPREDICTABLE', 'ArchVersion() < 6 && (inst(24)=0) || (inst(21)=1) && inst(19:16) == inst(3:0) => UNPREDICTABLE', 'inst(24)=1 => FORBIDDEN']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load3RegisterOp,
//       base: Rn,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc000pu0w1nnnntttt00001111mmmm,
//       rule: LDRSH_register,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, Pc in {Rt,Rm} => UNPREDICTABLE, wback && (Rn == Pc || Rn == Rt) => UNPREDICTABLE, ArchVersion() < 6 && wback && Rm == Rn => UNPREDICTABLE, index => FORBIDDEN],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case12_TestCase12) {
  Load3RegisterOpTester_Case12 tester;
  tester.Test("cccc000pu0w1nnnntttt00001111mmmm");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x0
//    = {actual: 'Store2RegisterImm8DoubleOp',
//       baseline: 'Store2RegisterImm8DoubleOp',
//       constraints: ,
//       defs: {inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu1w0nnnnttttiiii1111iiii',
//       rule: 'STRD_immediate',
//       safety: ['inst(15:12)(0)=1 => UNPREDICTABLE', 'inst(24)=0 && inst(21)=1 => UNPREDICTABLE', '(inst(24)=0) || (inst(21)=1) && (15 == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) + 1 => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       Rt2: Rt + 1,
//       W: W(21),
//       actual: Store2RegisterImm8DoubleOp,
//       base: Rn,
//       baseline: Store2RegisterImm8DoubleOp,
//       constraints: ,
//       defs: {base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc000pu1w0nnnnttttiiii1111iiii,
//       rule: STRD_immediate,
//       safety: [Rt(0)=1 => UNPREDICTABLE, P(24)=0 && W(21)=1 => UNPREDICTABLE, wback && (Rn == Pc) => UNPREDICTABLE, Rt2 == Pc => UNPREDICTABLE],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8DoubleOpTester_Case13_TestCase13) {
  Store2RegisterImm8DoubleOpTester_Case13 tester;
  tester.Test("cccc000pu1w0nnnnttttiiii1111iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {actual: 'Load2RegisterImm8Op',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12),inst(19:16) if (inst(24)=0) || (inst(21)=1) else 32},
//       pattern: 'cccc000pu1w1nnnnttttiiii1111iiii',
//       rule: 'LDRSH_immediate',
//       safety: ['inst(19:16)=1111 => DECODER_ERROR', 'inst(24)=0 && inst(21)=1 => DECODER_ERROR', '15 == inst(15:12) || ((inst(24)=0) || (inst(21)=1) && inst(15:12) == inst(19:16)) => UNPREDICTABLE', '15 == inst(15:12) => FORBIDDEN_OPERANDS']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load2RegisterImm8Op,
//       base: Rn,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       defs: {Rt,base if wback else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc000pu1w1nnnnttttiiii1111iiii,
//       rule: LDRSH_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR, P(24)=0 && W(21)=1 => DECODER_ERROR, Rt == Pc || (wback && Rn == Rt) => UNPREDICTABLE, Rt == Pc => FORBIDDEN_OPERANDS],
//       wback: (P(24)=0) || (W(21)=1)}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case14_TestCase14) {
  Load2RegisterImm8OpTester_Case14 tester;
  tester.Test("cccc000pu1w1nnnnttttiiii1111iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'LoadRegisterImm8Op',
//       baseline: 'LoadRegisterImm8Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc0001u1011111ttttiiii1111iiii',
//       rule: 'LDRSB_literal',
//       safety: ['inst(24)=0 && inst(21)=1 => DECODER_ERROR', 'inst(21) == inst(24) => UNPREDICTABLE', '15 == inst(15:12) => UNPREDICTABLE']}
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {P: P(24),
//       Pc: 15,
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: LoadRegisterImm8Op,
//       baseline: LoadRegisterImm8Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [P(24), W(21), Rt(15:12)],
//       pattern: cccc0001u1011111ttttiiii1111iiii,
//       rule: LDRSB_literal,
//       safety: [P(24)=0 && W(21)=1 => DECODER_ERROR, P == W => UNPREDICTABLE, Rt == Pc => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterImm8OpTester_Case15_TestCase15) {
  LoadRegisterImm8OpTester_Case15 tester;
  tester.Test("cccc0001u1011111ttttiiii1111iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
