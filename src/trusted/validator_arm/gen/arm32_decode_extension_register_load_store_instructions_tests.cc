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
// inst(24:20)=01x00 & inst(8)=0
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x00 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTesterCase0
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase0(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x00
  if ((inst.Bits() & 0x01B00000)  !=
          0x00800000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterListTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=1 &&
  //       U(23)=0 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=01x00 & inst(8)=1
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x00 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTesterCase1
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase1(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x00
  if ((inst.Bits() & 0x01B00000)  !=
          0x00800000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterListTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=1 &&
  //       U(23)=0 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       regs  >
  //          16 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32)))));

  // safety: VFPSmallRegisterBank() &&
  //       d + regs  >
  //          16 => UNPREDICTABLE
  EXPECT_TRUE(!((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=01x01 & inst(8)=0
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x01 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadStoreVectorRegisterListTesterCase2
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase2(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x01
  if ((inst.Bits() & 0x01B00000)  !=
          0x00900000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorRegisterListTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=0 &&
  //       U(23)=1 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=01x01 & inst(8)=1
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x01 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadStoreVectorRegisterListTesterCase3
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase3(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x01
  if ((inst.Bits() & 0x01B00000)  !=
          0x00900000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorRegisterListTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=0 &&
  //       U(23)=1 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       regs  >
  //          16 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32)))));

  // safety: VFPSmallRegisterBank() &&
  //       d + regs  >
  //          16 => UNPREDICTABLE
  EXPECT_TRUE(!((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=01x10 & inst(8)=0
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x10 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTesterCase4
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase4(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x10
  if ((inst.Bits() & 0x01B00000)  !=
          0x00A00000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterListTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=1 &&
  //       U(23)=0 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=01x10 & inst(8)=1
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x10 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTesterCase5
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase5(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x10
  if ((inst.Bits() & 0x01B00000)  !=
          0x00A00000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterListTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=1 &&
  //       U(23)=0 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       regs  >
  //          16 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32)))));

  // safety: VFPSmallRegisterBank() &&
  //       d + regs  >
  //          16 => UNPREDICTABLE
  EXPECT_TRUE(!((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101 & inst(8)=0
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadStoreVectorRegisterListTesterCase6
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase6(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x11
  if ((inst.Bits() & 0x01B00000)  !=
          0x00B00000) return false;
  // Rn(19:16)=1101
  if ((inst.Bits() & 0x000F0000)  ==
          0x000D0000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorRegisterListTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=0 &&
  //       U(23)=1 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101 & inst(8)=1
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadStoreVectorRegisterListTesterCase7
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase7(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x11
  if ((inst.Bits() & 0x01B00000)  !=
          0x00B00000) return false;
  // Rn(19:16)=1101
  if ((inst.Bits() & 0x000F0000)  ==
          0x000D0000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorRegisterListTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=0 &&
  //       U(23)=1 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       regs  >
  //          16 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32)))));

  // safety: VFPSmallRegisterBank() &&
  //       d + regs  >
  //          16 => UNPREDICTABLE
  EXPECT_TRUE(!((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101 & inst(8)=0
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=1101 & S(8)=0
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       regs: imm8,
//       safety: [regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE]}
class LoadStoreVectorRegisterListTesterCase8
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase8(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x11
  if ((inst.Bits() & 0x01B00000)  !=
          0x00B00000) return false;
  // Rn(19:16)=~1101
  if ((inst.Bits() & 0x000F0000)  !=
          0x000D0000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorRegisterListTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: regs  ==
  //          0 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32)))));

  // defs: {Sp};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(13))));

  return true;
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101 & inst(8)=1
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=1101 & S(8)=1
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       regs: imm8 / 2,
//       safety: [regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE]}
class LoadStoreVectorRegisterListTesterCase9
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase9(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~01x11
  if ((inst.Bits() & 0x01B00000)  !=
          0x00B00000) return false;
  // Rn(19:16)=~1101
  if ((inst.Bits() & 0x000F0000)  !=
          0x000D0000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorRegisterListTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: regs  ==
  //          0 ||
  //       regs  >
  //          16 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32)))));

  // safety: VFPSmallRegisterBank() &&
  //       d + regs  >
  //          16 => UNPREDICTABLE
  EXPECT_TRUE(!((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16)))));

  // defs: {Sp};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(13))));

  return true;
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101 & inst(8)=0
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTesterCase10
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase10(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~10x10
  if ((inst.Bits() & 0x01B00000)  !=
          0x01200000) return false;
  // Rn(19:16)=1101
  if ((inst.Bits() & 0x000F0000)  ==
          0x000D0000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterListTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=1 &&
  //       U(23)=0 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101 & inst(8)=1
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTesterCase11
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase11(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~10x10
  if ((inst.Bits() & 0x01B00000)  !=
          0x01200000) return false;
  // Rn(19:16)=1101
  if ((inst.Bits() & 0x000F0000)  ==
          0x000D0000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterListTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=1 &&
  //       U(23)=0 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       regs  >
  //          16 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32)))));

  // safety: VFPSmallRegisterBank() &&
  //       d + regs  >
  //          16 => UNPREDICTABLE
  EXPECT_TRUE(!((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101 & inst(8)=0
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=1101 & S(8)=0
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       regs: imm8,
//       safety: [regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE]}
class StoreVectorRegisterListTesterCase12
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase12(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~10x10
  if ((inst.Bits() & 0x01B00000)  !=
          0x01200000) return false;
  // Rn(19:16)=~1101
  if ((inst.Bits() & 0x000F0000)  !=
          0x000D0000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterListTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: regs  ==
  //          0 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32)))));

  // defs: {Sp};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(13))));

  return true;
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101 & inst(8)=1
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=1101 & S(8)=1
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       regs: imm8 / 2,
//       safety: [regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE]}
class StoreVectorRegisterListTesterCase13
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase13(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~10x10
  if ((inst.Bits() & 0x01B00000)  !=
          0x01200000) return false;
  // Rn(19:16)=~1101
  if ((inst.Bits() & 0x000F0000)  !=
          0x000D0000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterListTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: regs  ==
  //          0 ||
  //       regs  >
  //          16 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32)))));

  // safety: VFPSmallRegisterBank() &&
  //       d + regs  >
  //          16 => UNPREDICTABLE
  EXPECT_TRUE(!((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16)))));

  // defs: {Sp};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(13))));

  return true;
}

// Neutral case:
// inst(24:20)=10x11 & inst(8)=0
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=10x11 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadStoreVectorRegisterListTesterCase14
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase14(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~10x11
  if ((inst.Bits() & 0x01B00000)  !=
          0x01300000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorRegisterListTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=0 &&
  //       U(23)=1 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=10x11 & inst(8)=1
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=10x11 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadStoreVectorRegisterListTesterCase15
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase15(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~10x11
  if ((inst.Bits() & 0x01B00000)  !=
          0x01300000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorRegisterListTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       U(23)=0 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P(24)=1 &&
  //       W(21)=0 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000)));

  // safety: P  ==
  //          U &&
  //       W(21)=1 => UNDEFINED
  EXPECT_TRUE(!((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: n  ==
  //          Pc &&
  //       wback => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: P(24)=0 &&
  //       U(23)=1 &&
  //       W(21)=1 &&
  //       Rn  ==
  //          Sp => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13)))));

  // safety: regs  ==
  //          0 ||
  //       regs  >
  //          16 ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32)))));

  // safety: VFPSmallRegisterBank() &&
  //       d + regs  >
  //          16 => UNPREDICTABLE
  EXPECT_TRUE(!((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16)))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(24:20)=1xx00 & inst(8)=0
//    = {baseline: 'StoreVectorRegister',
//       constraints: ,
//       defs: {},
//       safety: [15  ==
//               inst(19:16) => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// opcode(24:20)=1xx00 & S(8)=0
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       baseline: StoreVectorRegister,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       n: Rn,
//       safety: [n  ==
//               Pc => FORBIDDEN_OPERANDS]}
class StoreVectorRegisterTesterCase16
    : public StoreVectorRegisterTester {
 public:
  StoreVectorRegisterTesterCase16(const NamedClassDecoder& decoder)
    : StoreVectorRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~1xx00
  if ((inst.Bits() & 0x01300000)  !=
          0x01000000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterTester::
               ApplySanityChecks(inst, decoder));

  // safety: n  ==
  //          Pc => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(24:20)=1xx00 & inst(8)=1
//    = {baseline: 'StoreVectorRegister',
//       constraints: ,
//       defs: {},
//       safety: [15  ==
//               inst(19:16) => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// opcode(24:20)=1xx00 & S(8)=1
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       baseline: StoreVectorRegister,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       n: Rn,
//       safety: [n  ==
//               Pc => FORBIDDEN_OPERANDS]}
class StoreVectorRegisterTesterCase17
    : public StoreVectorRegisterTester {
 public:
  StoreVectorRegisterTesterCase17(const NamedClassDecoder& decoder)
    : StoreVectorRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~1xx00
  if ((inst.Bits() & 0x01300000)  !=
          0x01000000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

bool StoreVectorRegisterTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(StoreVectorRegisterTester::
               ApplySanityChecks(inst, decoder));

  // safety: n  ==
  //          Pc => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(24:20)=1xx01 & inst(8)=0
//    = {baseline: 'LoadVectorRegister',
//       constraints: ,
//       defs: {}}
//
// Representaive case:
// opcode(24:20)=1xx01 & S(8)=0
//    = {baseline: LoadVectorRegister,
//       constraints: ,
//       defs: {}}
class LoadStoreVectorOpTesterCase18
    : public LoadStoreVectorOpTester {
 public:
  LoadStoreVectorOpTesterCase18(const NamedClassDecoder& decoder)
    : LoadStoreVectorOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorOpTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~1xx01
  if ((inst.Bits() & 0x01300000)  !=
          0x01100000) return false;
  // S(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorOpTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorOpTester::
               ApplySanityChecks(inst, decoder));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(24:20)=1xx01 & inst(8)=1
//    = {baseline: 'LoadVectorRegister',
//       constraints: ,
//       defs: {}}
//
// Representaive case:
// opcode(24:20)=1xx01 & S(8)=1
//    = {baseline: LoadVectorRegister,
//       constraints: ,
//       defs: {}}
class LoadStoreVectorOpTesterCase19
    : public LoadStoreVectorOpTester {
 public:
  LoadStoreVectorOpTesterCase19(const NamedClassDecoder& decoder)
    : LoadStoreVectorOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreVectorOpTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opcode(24:20)=~1xx01
  if ((inst.Bits() & 0x01300000)  !=
          0x01100000) return false;
  // S(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreVectorOpTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreVectorOpTester::
               ApplySanityChecks(inst, decoder));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(24:20)=01x00 & inst(8)=0
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=01x00 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTester_Case0
    : public StoreVectorRegisterListTesterCase0 {
 public:
  StoreVectorRegisterListTester_Case0()
    : StoreVectorRegisterListTesterCase0(
      state_.StoreVectorRegisterList_VSTM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x00 & inst(8)=1
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=01x00 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTester_Case1
    : public StoreVectorRegisterListTesterCase1 {
 public:
  StoreVectorRegisterListTester_Case1()
    : StoreVectorRegisterListTesterCase1(
      state_.StoreVectorRegisterList_VSTM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x01 & inst(8)=0
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=01x01 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadVectorRegisterListTester_Case2
    : public LoadStoreVectorRegisterListTesterCase2 {
 public:
  LoadVectorRegisterListTester_Case2()
    : LoadStoreVectorRegisterListTesterCase2(
      state_.LoadVectorRegisterList_VLDM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x01 & inst(8)=1
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=01x01 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadVectorRegisterListTester_Case3
    : public LoadStoreVectorRegisterListTesterCase3 {
 public:
  LoadVectorRegisterListTester_Case3()
    : LoadStoreVectorRegisterListTesterCase3(
      state_.LoadVectorRegisterList_VLDM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x10 & inst(8)=0
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=01x10 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTester_Case4
    : public StoreVectorRegisterListTesterCase4 {
 public:
  StoreVectorRegisterListTester_Case4()
    : StoreVectorRegisterListTesterCase4(
      state_.StoreVectorRegisterList_VSTM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x10 & inst(8)=1
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=01x10 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTester_Case5
    : public StoreVectorRegisterListTesterCase5 {
 public:
  StoreVectorRegisterListTester_Case5()
    : StoreVectorRegisterListTesterCase5(
      state_.StoreVectorRegisterList_VSTM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101 & inst(8)=0
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadVectorRegisterListTester_Case6
    : public LoadStoreVectorRegisterListTesterCase6 {
 public:
  LoadVectorRegisterListTester_Case6()
    : LoadStoreVectorRegisterListTesterCase6(
      state_.LoadVectorRegisterList_VLDM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101 & inst(8)=1
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadVectorRegisterListTester_Case7
    : public LoadStoreVectorRegisterListTesterCase7 {
 public:
  LoadVectorRegisterListTester_Case7()
    : LoadStoreVectorRegisterListTesterCase7(
      state_.LoadVectorRegisterList_VLDM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101 & inst(8)=0
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       rule: 'VPOP',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE]}
//
// Representative case:
// opcode(24:20)=01x11 & Rn(19:16)=1101 & S(8)=0
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       regs: imm8,
//       rule: VPOP,
//       safety: [regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE]}
class LoadVectorRegisterListTester_Case8
    : public LoadStoreVectorRegisterListTesterCase8 {
 public:
  LoadVectorRegisterListTester_Case8()
    : LoadStoreVectorRegisterListTesterCase8(
      state_.LoadVectorRegisterList_VPOP_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101 & inst(8)=1
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       rule: 'VPOP',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE]}
//
// Representative case:
// opcode(24:20)=01x11 & Rn(19:16)=1101 & S(8)=1
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       regs: imm8 / 2,
//       rule: VPOP,
//       safety: [regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE]}
class LoadVectorRegisterListTester_Case9
    : public LoadStoreVectorRegisterListTesterCase9 {
 public:
  LoadVectorRegisterListTester_Case9()
    : LoadStoreVectorRegisterListTesterCase9(
      state_.LoadVectorRegisterList_VPOP_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101 & inst(8)=0
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTester_Case10
    : public StoreVectorRegisterListTesterCase10 {
 public:
  StoreVectorRegisterListTester_Case10()
    : StoreVectorRegisterListTesterCase10(
      state_.StoreVectorRegisterList_VSTM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101 & inst(8)=1
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class StoreVectorRegisterListTester_Case11
    : public StoreVectorRegisterListTesterCase11 {
 public:
  StoreVectorRegisterListTester_Case11()
    : StoreVectorRegisterListTesterCase11(
      state_.StoreVectorRegisterList_VSTM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101 & inst(8)=0
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       rule: 'VPUSH',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE]}
//
// Representative case:
// opcode(24:20)=10x10 & Rn(19:16)=1101 & S(8)=0
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       regs: imm8,
//       rule: VPUSH,
//       safety: [regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE]}
class StoreVectorRegisterListTester_Case12
    : public StoreVectorRegisterListTesterCase12 {
 public:
  StoreVectorRegisterListTester_Case12()
    : StoreVectorRegisterListTesterCase12(
      state_.StoreVectorRegisterList_VPUSH_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101 & inst(8)=1
//    = {baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       rule: 'VPUSH',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE]}
//
// Representative case:
// opcode(24:20)=10x10 & Rn(19:16)=1101 & S(8)=1
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       regs: imm8 / 2,
//       rule: VPUSH,
//       safety: [regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE]}
class StoreVectorRegisterListTester_Case13
    : public StoreVectorRegisterListTesterCase13 {
 public:
  StoreVectorRegisterListTester_Case13()
    : StoreVectorRegisterListTesterCase13(
      state_.StoreVectorRegisterList_VPUSH_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x11 & inst(8)=0
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=10x11 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadVectorRegisterListTester_Case14
    : public LoadStoreVectorRegisterListTesterCase14 {
 public:
  LoadVectorRegisterListTester_Case14()
    : LoadStoreVectorRegisterListTesterCase14(
      state_.LoadVectorRegisterList_VLDM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x11 & inst(8)=1
//    = {baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representative case:
// opcode(24:20)=10x11 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       regs: imm8 / 2,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
class LoadVectorRegisterListTester_Case15
    : public LoadStoreVectorRegisterListTesterCase15 {
 public:
  LoadVectorRegisterListTester_Case15()
    : LoadStoreVectorRegisterListTesterCase15(
      state_.LoadVectorRegisterList_VLDM_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1xx00 & inst(8)=0
//    = {baseline: 'StoreVectorRegister',
//       constraints: ,
//       defs: {},
//       rule: 'VSTR',
//       safety: [15  ==
//               inst(19:16) => FORBIDDEN_OPERANDS]}
//
// Representative case:
// opcode(24:20)=1xx00 & S(8)=0
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       baseline: StoreVectorRegister,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       n: Rn,
//       rule: VSTR,
//       safety: [n  ==
//               Pc => FORBIDDEN_OPERANDS]}
class StoreVectorRegisterTester_Case16
    : public StoreVectorRegisterTesterCase16 {
 public:
  StoreVectorRegisterTester_Case16()
    : StoreVectorRegisterTesterCase16(
      state_.StoreVectorRegister_VSTR_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1xx00 & inst(8)=1
//    = {baseline: 'StoreVectorRegister',
//       constraints: ,
//       defs: {},
//       rule: 'VSTR',
//       safety: [15  ==
//               inst(19:16) => FORBIDDEN_OPERANDS]}
//
// Representative case:
// opcode(24:20)=1xx00 & S(8)=1
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       baseline: StoreVectorRegister,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       n: Rn,
//       rule: VSTR,
//       safety: [n  ==
//               Pc => FORBIDDEN_OPERANDS]}
class StoreVectorRegisterTester_Case17
    : public StoreVectorRegisterTesterCase17 {
 public:
  StoreVectorRegisterTester_Case17()
    : StoreVectorRegisterTesterCase17(
      state_.StoreVectorRegister_VSTR_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1xx01 & inst(8)=0
//    = {baseline: 'LoadVectorRegister',
//       constraints: ,
//       defs: {},
//       rule: 'VLDR'}
//
// Representative case:
// opcode(24:20)=1xx01 & S(8)=0
//    = {baseline: LoadVectorRegister,
//       constraints: ,
//       defs: {},
//       rule: VLDR}
class LoadVectorRegisterTester_Case18
    : public LoadStoreVectorOpTesterCase18 {
 public:
  LoadVectorRegisterTester_Case18()
    : LoadStoreVectorOpTesterCase18(
      state_.LoadVectorRegister_VLDR_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1xx01 & inst(8)=1
//    = {baseline: 'LoadVectorRegister',
//       constraints: ,
//       defs: {},
//       rule: 'VLDR'}
//
// Representative case:
// opcode(24:20)=1xx01 & S(8)=1
//    = {baseline: LoadVectorRegister,
//       constraints: ,
//       defs: {},
//       rule: VLDR}
class LoadVectorRegisterTester_Case19
    : public LoadStoreVectorOpTesterCase19 {
 public:
  LoadVectorRegisterTester_Case19()
    : LoadStoreVectorOpTesterCase19(
      state_.LoadVectorRegister_VLDR_instance_)
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
// inst(24:20)=01x00 & inst(8)=0
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw0nnnndddd1010iiiiiiii',
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x00 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw0nnnndddd1010iiiiiiii,
//       regs: imm8,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case0_TestCase0) {
  StoreVectorRegisterListTester_Case0 tester;
  tester.Test("cccc110pudw0nnnndddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x00 & inst(8)=1
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw0nnnndddd1011iiiiiiii',
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x00 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw0nnnndddd1011iiiiiiii,
//       regs: imm8 / 2,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case1_TestCase1) {
  StoreVectorRegisterListTester_Case1 tester;
  tester.Test("cccc110pudw0nnnndddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x01 & inst(8)=0
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw1nnnndddd1010iiiiiiii',
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x01 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw1nnnndddd1010iiiiiiii,
//       regs: imm8,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case2_TestCase2) {
  LoadVectorRegisterListTester_Case2 tester;
  tester.Test("cccc110pudw1nnnndddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x01 & inst(8)=1
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw1nnnndddd1011iiiiiiii',
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x01 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw1nnnndddd1011iiiiiiii,
//       regs: imm8 / 2,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case3_TestCase3) {
  LoadVectorRegisterListTester_Case3 tester;
  tester.Test("cccc110pudw1nnnndddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x10 & inst(8)=0
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw0nnnndddd1010iiiiiiii',
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x10 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw0nnnndddd1010iiiiiiii,
//       regs: imm8,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case4_TestCase4) {
  StoreVectorRegisterListTester_Case4 tester;
  tester.Test("cccc110pudw0nnnndddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x10 & inst(8)=1
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw0nnnndddd1011iiiiiiii',
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x10 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw0nnnndddd1011iiiiiiii,
//       regs: imm8 / 2,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case5_TestCase5) {
  StoreVectorRegisterListTester_Case5 tester;
  tester.Test("cccc110pudw0nnnndddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101 & inst(8)=0
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw1nnnndddd1010iiiiiiii',
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw1nnnndddd1010iiiiiiii,
//       regs: imm8,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case6_TestCase6) {
  LoadVectorRegisterListTester_Case6 tester;
  tester.Test("cccc110pudw1nnnndddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101 & inst(8)=1
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw1nnnndddd1011iiiiiiii',
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw1nnnndddd1011iiiiiiii,
//       regs: imm8 / 2,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case7_TestCase7) {
  LoadVectorRegisterListTester_Case7 tester;
  tester.Test("cccc110pudw1nnnndddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101 & inst(8)=0
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       pattern: 'cccc11001d111101dddd1010iiiiiiii',
//       rule: 'VPOP',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=1101 & S(8)=0
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       pattern: cccc11001d111101dddd1010iiiiiiii,
//       regs: imm8,
//       rule: VPOP,
//       safety: [regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case8_TestCase8) {
  LoadVectorRegisterListTester_Case8 tester;
  tester.Test("cccc11001d111101dddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101 & inst(8)=1
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       pattern: 'cccc11001d111101dddd1011iiiiiiii',
//       rule: 'VPOP',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=1101 & S(8)=1
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       pattern: cccc11001d111101dddd1011iiiiiiii,
//       regs: imm8 / 2,
//       rule: VPOP,
//       safety: [regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case9_TestCase9) {
  LoadVectorRegisterListTester_Case9 tester;
  tester.Test("cccc11001d111101dddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101 & inst(8)=0
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw0nnnndddd1010iiiiiiii',
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw0nnnndddd1010iiiiiiii,
//       regs: imm8,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case10_TestCase10) {
  StoreVectorRegisterListTester_Case10 tester;
  tester.Test("cccc110pudw0nnnndddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101 & inst(8)=1
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw0nnnndddd1011iiiiiiii',
//       rule: 'VSTM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(23)=0 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw0nnnndddd1011iiiiiiii,
//       regs: imm8 / 2,
//       rule: VSTM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=1 &&
//            U(23)=0 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case11_TestCase11) {
  StoreVectorRegisterListTester_Case11 tester;
  tester.Test("cccc110pudw0nnnndddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101 & inst(8)=0
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       pattern: 'cccc11010d101101dddd1010iiiiiiii',
//       rule: 'VPUSH',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=1101 & S(8)=0
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       pattern: cccc11010d101101dddd1010iiiiiiii,
//       regs: imm8,
//       rule: VPUSH,
//       safety: [regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case12_TestCase12) {
  StoreVectorRegisterListTester_Case12 tester;
  tester.Test("cccc11010d101101dddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101 & inst(8)=1
//    = {actual: 'StoreVectorRegisterList',
//       baseline: 'StoreVectorRegisterList',
//       constraints: ,
//       defs: {13},
//       pattern: 'cccc11010d101101dddd1011iiiiiiii',
//       rule: 'VPUSH',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=1101 & S(8)=1
//    = {D: D(22),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: StoreVectorRegisterList,
//       baseline: StoreVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Sp},
//       fields: [D(22), Vd(15:12), imm8(7:0)],
//       imm8: imm8(7:0),
//       pattern: cccc11010d101101dddd1011iiiiiiii,
//       regs: imm8 / 2,
//       rule: VPUSH,
//       safety: [regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case13_TestCase13) {
  StoreVectorRegisterListTester_Case13 tester;
  tester.Test("cccc11010d101101dddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=10x11 & inst(8)=0
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw1nnnndddd1010iiiiiiii',
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) ||
//            32  <=
//               inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=10x11 & S(8)=0
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: Vd:D,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw1nnnndddd1010iiiiiiii,
//       regs: imm8,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case14_TestCase14) {
  LoadVectorRegisterListTester_Case14 tester;
  tester.Test("cccc110pudw1nnnndddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=10x11 & inst(8)=1
//    = {actual: 'LoadVectorRegisterList',
//       baseline: 'LoadVectorRegisterList',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(21)=1
//            else 32},
//       pattern: 'cccc110pudw1nnnndddd1011iiiiiiii',
//       rule: 'VLDM',
//       safety: [0  ==
//               inst(7:0) / 2 ||
//            16  <=
//               inst(7:0) / 2 ||
//            32  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         15  ==
//               inst(19:16) &&
//            inst(21)=1 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            16  <=
//               inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//         inst(23)  ==
//               inst(24) &&
//            inst(21)=1 => UNDEFINED,
//         inst(24)=0 &&
//            inst(23)=0 &&
//            inst(21)=0 => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(23)=1 &&
//            inst(21)=1 &&
//            13  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=1 &&
//            inst(21)=0 => DECODER_ERROR]}
//
// Representaive case:
// opcode(24:20)=10x11 & S(8)=1
//    = {D: D(22),
//       None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Sp: 13,
//       U: U(23),
//       Vd: Vd(15:12),
//       W: W(21),
//       actual: LoadVectorRegisterList,
//       baseline: LoadVectorRegisterList,
//       constraints: ,
//       d: D:Vd,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [P(24),
//         U(23),
//         D(22),
//         W(21),
//         Rn(19:16),
//         Vd(15:12),
//         imm8(7:0)],
//       imm8: imm8(7:0),
//       n: Rn,
//       pattern: cccc110pudw1nnnndddd1011iiiiiiii,
//       regs: imm8 / 2,
//       rule: VLDM,
//       safety: [P(24)=0 &&
//            U(23)=0 &&
//            W(21)=0 => DECODER_ERROR,
//         P(24)=1 &&
//            W(21)=0 => DECODER_ERROR,
//         P  ==
//               U &&
//            W(21)=1 => UNDEFINED,
//         n  ==
//               Pc &&
//            wback => UNPREDICTABLE,
//         P(24)=0 &&
//            U(23)=1 &&
//            W(21)=1 &&
//            Rn  ==
//               Sp => DECODER_ERROR,
//         regs  ==
//               0 ||
//            regs  >
//               16 ||
//            d + regs  >
//               32 => UNPREDICTABLE,
//         VFPSmallRegisterBank() &&
//            d + regs  >
//               16 => UNPREDICTABLE],
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case15_TestCase15) {
  LoadVectorRegisterListTester_Case15 tester;
  tester.Test("cccc110pudw1nnnndddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=1xx00 & inst(8)=0
//    = {actual: 'StoreVectorRegister',
//       baseline: 'StoreVectorRegister',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc1101ud00nnnndddd1010iiiiiiii',
//       rule: 'VSTR',
//       safety: [15  ==
//               inst(19:16) => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// opcode(24:20)=1xx00 & S(8)=0
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       actual: StoreVectorRegister,
//       baseline: StoreVectorRegister,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       n: Rn,
//       pattern: cccc1101ud00nnnndddd1010iiiiiiii,
//       rule: VSTR,
//       safety: [n  ==
//               Pc => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterTester_Case16_TestCase16) {
  StoreVectorRegisterTester_Case16 tester;
  tester.Test("cccc1101ud00nnnndddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=1xx00 & inst(8)=1
//    = {actual: 'StoreVectorRegister',
//       baseline: 'StoreVectorRegister',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc1101ud00nnnndddd1011iiiiiiii',
//       rule: 'VSTR',
//       safety: [15  ==
//               inst(19:16) => FORBIDDEN_OPERANDS]}
//
// Representaive case:
// opcode(24:20)=1xx00 & S(8)=1
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       actual: StoreVectorRegister,
//       baseline: StoreVectorRegister,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       n: Rn,
//       pattern: cccc1101ud00nnnndddd1011iiiiiiii,
//       rule: VSTR,
//       safety: [n  ==
//               Pc => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterTester_Case17_TestCase17) {
  StoreVectorRegisterTester_Case17 tester;
  tester.Test("cccc1101ud00nnnndddd1011iiiiiiii");
}

// Neutral case:
// inst(24:20)=1xx01 & inst(8)=0
//    = {actual: 'LoadVectorRegister',
//       baseline: 'LoadVectorRegister',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc1101ud01nnnndddd1010iiiiiiii',
//       rule: 'VLDR'}
//
// Representaive case:
// opcode(24:20)=1xx01 & S(8)=0
//    = {actual: LoadVectorRegister,
//       baseline: LoadVectorRegister,
//       constraints: ,
//       defs: {},
//       pattern: cccc1101ud01nnnndddd1010iiiiiiii,
//       rule: VLDR}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterTester_Case18_TestCase18) {
  LoadVectorRegisterTester_Case18 tester;
  tester.Test("cccc1101ud01nnnndddd1010iiiiiiii");
}

// Neutral case:
// inst(24:20)=1xx01 & inst(8)=1
//    = {actual: 'LoadVectorRegister',
//       baseline: 'LoadVectorRegister',
//       constraints: ,
//       defs: {},
//       pattern: 'cccc1101ud01nnnndddd1011iiiiiiii',
//       rule: 'VLDR'}
//
// Representaive case:
// opcode(24:20)=1xx01 & S(8)=1
//    = {actual: LoadVectorRegister,
//       baseline: LoadVectorRegister,
//       constraints: ,
//       defs: {},
//       pattern: cccc1101ud01nnnndddd1011iiiiiiii,
//       rule: VLDR}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterTester_Case19_TestCase19) {
  LoadVectorRegisterTester_Case19 tester;
  tester.Test("cccc1101ud01nnnndddd1011iiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
