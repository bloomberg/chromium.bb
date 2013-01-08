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
// inst(25)=0 & inst(24:20)=0x010
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x010
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~0x010
  if ((inst.Bits() & 0x01700000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x011
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x011
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
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~0x011
  if ((inst.Bits() & 0x01700000)  !=
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x110
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x110
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
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~0x110
  if ((inst.Bits() & 0x01700000)  !=
          0x00600000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x111
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x111
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
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~0x111
  if ((inst.Bits() & 0x01700000)  !=
          0x00700000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x0 & inst(24:20)=~0x010
//    = {baseline: 'Store2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       safety: [inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x0 & op1_repeated(24:20)=~0x010
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store2RegisterImm12Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
class LoadStore2RegisterImm12OpTesterCase4
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase4(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~xx0x0
  if ((inst.Bits() & 0x00500000)  !=
          0x00000000) return false;
  // op1_repeated(24:20)=0x010
  if ((inst.Bits() & 0x01700000)  ==
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: wback &&
  //       (Rn  ==
  //          Pc ||
  //       Rn  ==
  //          Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // defs: {base
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = {baseline: 'LdrImmediateOp',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       safety: [15  ==
//               inst(15:12) => FORBIDDEN_OPERANDS,
//         15  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            inst(15:12)  ==
//               inst(19:16) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: LdrImmediateOp,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [Rn  ==
//               Pc => DECODER_ERROR,
//         P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         wback &&
//            Rn  ==
//               Rt => UNPREDICTABLE,
//         Rt  ==
//               Pc => FORBIDDEN_OPERANDS],
//       wback: P(24)=0 ||
//            W(21)=1}
class LoadStore2RegisterImm12OpTesterCase5
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase5(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~xx0x1
  if ((inst.Bits() & 0x00500000)  !=
          0x00100000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // op1_repeated(24:20)=0x011
  if ((inst.Bits() & 0x01700000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc => DECODER_ERROR
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)));

  // safety: P(24)=0 &&
  //       W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: wback &&
  //       Rn  ==
  //          Rt => UNPREDICTABLE
  EXPECT_TRUE(!((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))));

  // safety: Rt  ==
  //          Pc => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt, base
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=1111 & inst(24:20)=~0x011 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12)}}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Rt: Rt(15:12),
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [Rt(15:12)]}
class LoadStore2RegisterImm12OpTesterCase6
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase6(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~xx0x1
  if ((inst.Bits() & 0x00500000)  !=
          0x00100000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // op1_repeated(24:20)=0x011
  if ((inst.Bits() & 0x01700000)  ==
          0x00300000) return false;
  // $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x01200000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // defs: {Rt};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x0 & inst(24:20)=~0x110
//    = {baseline: 'Store2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store2RegisterImm12Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rt  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
class LoadStore2RegisterImm12OpTesterCase7
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase7(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~xx1x0
  if ((inst.Bits() & 0x00500000)  !=
          0x00400000) return false;
  // op1_repeated(24:20)=0x110
  if ((inst.Bits() & 0x01700000)  ==
          0x00600000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: Rt  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // safety: wback &&
  //       (Rn  ==
  //          Pc ||
  //       Rn  ==
  //          Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // defs: {base
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=~1111 & inst(24:20)=~0x111
//    = {baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            inst(15:12)  ==
//               inst(19:16) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       safety: [Rn  ==
//               Pc => DECODER_ERROR,
//         P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rt  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            Rn  ==
//               Rt => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
class LoadStore2RegisterImm12OpTesterCase8
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase8(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~xx1x1
  if ((inst.Bits() & 0x00500000)  !=
          0x00500000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // op1_repeated(24:20)=0x111
  if ((inst.Bits() & 0x01700000)  ==
          0x00700000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc => DECODER_ERROR
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)));

  // safety: P(24)=0 &&
  //       W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: Rt  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // safety: wback &&
  //       Rn  ==
  //          Rt => UNPREDICTABLE
  EXPECT_TRUE(!((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))));

  // defs: {Rt, base
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(24:20)=~0x111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [Rt(15:12)],
//       safety: [Rt  ==
//               Pc => UNPREDICTABLE]}
class LoadStore2RegisterImm12OpTesterCase9
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase9(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~0
  if ((inst.Bits() & 0x02000000)  !=
          0x00000000) return false;
  // op1(24:20)=~xx1x1
  if ((inst.Bits() & 0x00500000)  !=
          0x00500000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // op1_repeated(24:20)=0x111
  if ((inst.Bits() & 0x01700000)  ==
          0x00700000) return false;
  // $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x01200000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore2RegisterImm12OpTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rt  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x010 & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase10
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~0x010
  if ((inst.Bits() & 0x01700000)  !=
          0x00200000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x011 & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x011 & B(4)=0
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
  // A(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~0x011
  if ((inst.Bits() & 0x01700000)  !=
          0x00300000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x110 & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = {baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase12
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase12(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~0x110
  if ((inst.Bits() & 0x01700000)  !=
          0x00600000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x111 & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x111 & B(4)=0
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
  // A(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~0x111
  if ((inst.Bits() & 0x01700000)  !=
          0x00700000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x0 & inst(4)=0 & inst(24:20)=~0x010
//    = {baseline: 'Store3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store3RegisterImm5Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
class LoadStore3RegisterImm5OpTesterCase14
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase14(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~xx0x0
  if ((inst.Bits() & 0x00500000)  !=
          0x00000000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // op1_repeated(24:20)=0x010
  if ((inst.Bits() & 0x01700000)  ==
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterImm5OpTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterImm5OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: Rm  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000000F)) != (15)));

  // safety: wback &&
  //       (Rn  ==
  //          Pc ||
  //       Rn  ==
  //          Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: ArchVersion()  <
  //          6 &&
  //       wback &&
  //       Rn  ==
  //          Rm => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000)  ==
          0x01000000));

  // defs: {base
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x1 & inst(4)=0 & inst(24:20)=~0x011
//    = {baseline: 'Load3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       safety: [15  ==
//               inst(15:12) => FORBIDDEN_OPERANDS,
//         15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterImm5Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN,
//         Rt  ==
//               Pc => FORBIDDEN_OPERANDS],
//       wback: P(24)=0 ||
//            W(21)=1}
class LoadStore3RegisterImm5OpTesterCase15
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase15(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~xx0x1
  if ((inst.Bits() & 0x00500000)  !=
          0x00100000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // op1_repeated(24:20)=0x011
  if ((inst.Bits() & 0x01700000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterImm5OpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterImm5OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: Rm  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000000F)) != (15)));

  // safety: wback &&
  //       (Rn  ==
  //          Pc ||
  //       Rn  ==
  //          Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: ArchVersion()  <
  //          6 &&
  //       wback &&
  //       Rn  ==
  //          Rm => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000)  ==
          0x01000000));

  // safety: Rt  ==
  //          Pc => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt, base
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x0 & inst(4)=0 & inst(24:20)=~0x110
//    = {baseline: 'Store3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store3RegisterImm5Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
class LoadStore3RegisterImm5OpTesterCase16
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase16(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~xx1x0
  if ((inst.Bits() & 0x00500000)  !=
          0x00400000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // op1_repeated(24:20)=0x110
  if ((inst.Bits() & 0x01700000)  ==
          0x00600000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterImm5OpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterImm5OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: Rm  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000000F)) != (15)));

  // safety: wback &&
  //       (Rn  ==
  //          Pc ||
  //       Rn  ==
  //          Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: ArchVersion()  <
  //          6 &&
  //       wback &&
  //       Rn  ==
  //          Rm => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000)  ==
          0x01000000));

  // defs: {base
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x1 & inst(4)=0 & inst(24:20)=~0x111
//    = {baseline: 'Load3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterImm5Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Pc in {Rt, Rm} => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
class LoadStore3RegisterImm5OpTesterCase17
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase17(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(25)=~1
  if ((inst.Bits() & 0x02000000)  !=
          0x02000000) return false;
  // op1(24:20)=~xx1x1
  if ((inst.Bits() & 0x00500000)  !=
          0x00500000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // op1_repeated(24:20)=0x111
  if ((inst.Bits() & 0x01700000)  ==
          0x00700000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStore3RegisterImm5OpTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterImm5OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: P(24)=0 &&
  //       W(21)=1 => DECODER_ERROR
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)));

  // safety: Pc in {Rt, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // safety: wback &&
  //       (Rn  ==
  //          Pc ||
  //       Rn  ==
  //          Rt) => UNPREDICTABLE
  EXPECT_TRUE(!((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))))));

  // safety: ArchVersion()  <
  //          6 &&
  //       wback &&
  //       Rn  ==
  //          Rm => UNPREDICTABLE
  EXPECT_TRUE(!((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F))))));

  // safety: index => FORBIDDEN
  EXPECT_TRUE(!((inst.Bits() & 0x01000000)  ==
          0x01000000));

  // defs: {Rt, base
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(25)=0 & inst(24:20)=0x010
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'STRT_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x010
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: STRT_A1}
class ForbiddenCondDecoderTester_Case0
    : public UnsafeCondDecoderTesterCase0 {
 public:
  ForbiddenCondDecoderTester_Case0()
    : UnsafeCondDecoderTesterCase0(
      state_.ForbiddenCondDecoder_STRT_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0x011
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'LDRT_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x011
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: LDRT_A1}
class ForbiddenCondDecoderTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenCondDecoderTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.ForbiddenCondDecoder_LDRT_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0x110
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'STRBT_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x110
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: STRBT_A1}
class ForbiddenCondDecoderTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenCondDecoderTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.ForbiddenCondDecoder_STRBT_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0x111
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'LDRBT_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x111
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: LDRBT_A1}
class ForbiddenCondDecoderTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenCondDecoderTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.ForbiddenCondDecoder_LDRBT_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x0 & inst(24:20)=~0x010
//    = {baseline: 'Store2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       rule: 'STR_immediate',
//       safety: [inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx0x0 & op1_repeated(24:20)=~0x010
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store2RegisterImm12Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: STR_immediate,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
class Store2RegisterImm12OpTester_Case4
    : public LoadStore2RegisterImm12OpTesterCase4 {
 public:
  Store2RegisterImm12OpTester_Case4()
    : LoadStore2RegisterImm12OpTesterCase4(
      state_.Store2RegisterImm12Op_STR_immediate_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = {baseline: 'LdrImmediateOp',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       rule: 'LDR_immediate',
//       safety: [15  ==
//               inst(15:12) => FORBIDDEN_OPERANDS,
//         15  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            inst(15:12)  ==
//               inst(19:16) => UNPREDICTABLE]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: LdrImmediateOp,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: LDR_immediate,
//       safety: [Rn  ==
//               Pc => DECODER_ERROR,
//         P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         wback &&
//            Rn  ==
//               Rt => UNPREDICTABLE,
//         Rt  ==
//               Pc => FORBIDDEN_OPERANDS],
//       wback: P(24)=0 ||
//            W(21)=1}
class LdrImmediateOpTester_Case5
    : public LoadStore2RegisterImm12OpTesterCase5 {
 public:
  LdrImmediateOpTester_Case5()
    : LoadStore2RegisterImm12OpTesterCase5(
      state_.LdrImmediateOp_LDR_immediate_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=1111 & inst(24:20)=~0x011 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'LDR_literal'}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Rt: Rt(15:12),
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [Rt(15:12)],
//       rule: LDR_literal}
class Load2RegisterImm12OpTester_Case6
    : public LoadStore2RegisterImm12OpTesterCase6 {
 public:
  Load2RegisterImm12OpTester_Case6()
    : LoadStore2RegisterImm12OpTesterCase6(
      state_.Load2RegisterImm12Op_LDR_literal_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x0 & inst(24:20)=~0x110
//    = {baseline: 'Store2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       rule: 'STRB_immediate',
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store2RegisterImm12Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: STRB_immediate,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rt  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
class Store2RegisterImm12OpTester_Case7
    : public LoadStore2RegisterImm12OpTesterCase7 {
 public:
  Store2RegisterImm12OpTester_Case7()
    : LoadStore2RegisterImm12OpTesterCase7(
      state_.Store2RegisterImm12Op_STRB_immediate_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=~1111 & inst(24:20)=~0x111
//    = {baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       rule: 'LDRB_immediate',
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            inst(15:12)  ==
//               inst(19:16) => UNPREDICTABLE]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       rule: LDRB_immediate,
//       safety: [Rn  ==
//               Pc => DECODER_ERROR,
//         P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rt  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            Rn  ==
//               Rt => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
class Load2RegisterImm12OpTester_Case8
    : public LoadStore2RegisterImm12OpTesterCase8 {
 public:
  Load2RegisterImm12OpTester_Case8()
    : LoadStore2RegisterImm12OpTesterCase8(
      state_.Load2RegisterImm12Op_LDRB_immediate_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(24:20)=~0x111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       rule: 'LDRB_literal',
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [Rt(15:12)],
//       rule: LDRB_literal,
//       safety: [Rt  ==
//               Pc => UNPREDICTABLE]}
class Load2RegisterImm12OpTester_Case9
    : public LoadStore2RegisterImm12OpTesterCase9 {
 public:
  Load2RegisterImm12OpTester_Case9()
    : LoadStore2RegisterImm12OpTesterCase9(
      state_.Load2RegisterImm12Op_LDRB_literal_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=0x010 & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'STRT_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: STRT_A2}
class ForbiddenCondDecoderTester_Case10
    : public UnsafeCondDecoderTesterCase10 {
 public:
  ForbiddenCondDecoderTester_Case10()
    : UnsafeCondDecoderTesterCase10(
      state_.ForbiddenCondDecoder_STRT_A2_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=0x011 & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'LDRT_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x011 & B(4)=0
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: LDRT_A2}
class ForbiddenCondDecoderTester_Case11
    : public UnsafeCondDecoderTesterCase11 {
 public:
  ForbiddenCondDecoderTester_Case11()
    : UnsafeCondDecoderTesterCase11(
      state_.ForbiddenCondDecoder_LDRT_A2_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=0x110 & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'STRBT_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: STRBT_A2}
class ForbiddenCondDecoderTester_Case12
    : public UnsafeCondDecoderTesterCase12 {
 public:
  ForbiddenCondDecoderTester_Case12()
    : UnsafeCondDecoderTesterCase12(
      state_.ForbiddenCondDecoder_STRBT_A2_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=0x111 & inst(4)=0
//    = {baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       rule: 'LDRBT_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x111 & B(4)=0
//    = {baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: LDRBT_A2}
class ForbiddenCondDecoderTester_Case13
    : public UnsafeCondDecoderTesterCase13 {
 public:
  ForbiddenCondDecoderTester_Case13()
    : UnsafeCondDecoderTesterCase13(
      state_.ForbiddenCondDecoder_LDRBT_A2_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x0 & inst(4)=0 & inst(24:20)=~0x010
//    = {baseline: 'Store3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       rule: 'STR_register',
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store3RegisterImm5Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: STR_register,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
class Store3RegisterImm5OpTester_Case14
    : public LoadStore3RegisterImm5OpTesterCase14 {
 public:
  Store3RegisterImm5OpTester_Case14()
    : LoadStore3RegisterImm5OpTesterCase14(
      state_.Store3RegisterImm5Op_STR_register_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x1 & inst(4)=0 & inst(24:20)=~0x011
//    = {baseline: 'Load3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       rule: 'LDR_register',
//       safety: [15  ==
//               inst(15:12) => FORBIDDEN_OPERANDS,
//         15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterImm5Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: LDR_register,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN,
//         Rt  ==
//               Pc => FORBIDDEN_OPERANDS],
//       wback: P(24)=0 ||
//            W(21)=1}
class Load3RegisterImm5OpTester_Case15
    : public LoadStore3RegisterImm5OpTesterCase15 {
 public:
  Load3RegisterImm5OpTester_Case15()
    : LoadStore3RegisterImm5OpTesterCase15(
      state_.Load3RegisterImm5Op_LDR_register_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x0 & inst(4)=0 & inst(24:20)=~0x110
//    = {baseline: 'Store3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       rule: 'STRB_register',
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Store3RegisterImm5Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: STRB_register,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
class Store3RegisterImm5OpTester_Case16
    : public LoadStore3RegisterImm5OpTesterCase16 {
 public:
  Store3RegisterImm5OpTester_Case16()
    : LoadStore3RegisterImm5OpTesterCase16(
      state_.Store3RegisterImm5Op_STRB_register_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x1 & inst(4)=0 & inst(24:20)=~0x111
//    = {baseline: 'Load3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       rule: 'LDRB_register',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       base: Rn,
//       baseline: Load3RegisterImm5Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       rule: LDRB_register,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Pc in {Rt, Rm} => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
class Load3RegisterImm5OpTester_Case17
    : public LoadStore3RegisterImm5OpTesterCase17 {
 public:
  Load3RegisterImm5OpTester_Case17()
    : LoadStore3RegisterImm5OpTesterCase17(
      state_.Load3RegisterImm5Op_LDRB_register_instance_)
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
// inst(25)=0 & inst(24:20)=0x010
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0100u010nnnnttttiiiiiiiiiiii',
//       rule: 'STRT_A1'}
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x010
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0100u010nnnnttttiiiiiiiiiiii,
//       rule: STRT_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case0_TestCase0) {
  ForbiddenCondDecoderTester_Case0 tester;
  tester.Test("cccc0100u010nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x011
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0100u011nnnnttttiiiiiiiiiiii',
//       rule: 'LDRT_A1'}
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x011
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0100u011nnnnttttiiiiiiiiiiii,
//       rule: LDRT_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case1_TestCase1) {
  ForbiddenCondDecoderTester_Case1 tester;
  tester.Test("cccc0100u011nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x110
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0100u110nnnnttttiiiiiiiiiiii',
//       rule: 'STRBT_A1'}
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x110
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0100u110nnnnttttiiiiiiiiiiii,
//       rule: STRBT_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case2_TestCase2) {
  ForbiddenCondDecoderTester_Case2 tester;
  tester.Test("cccc0100u110nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x111
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0100u111nnnnttttiiiiiiiiiiii',
//       rule: 'LDRBT_A1'}
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x111
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0100u111nnnnttttiiiiiiiiiiii,
//       rule: LDRBT_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case3_TestCase3) {
  ForbiddenCondDecoderTester_Case3 tester;
  tester.Test("cccc0100u111nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x0 & inst(24:20)=~0x010
//    = {actual: 'Store2RegisterImm12Op',
//       baseline: 'Store2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       pattern: 'cccc010pu0w0nnnnttttiiiiiiiiiiii',
//       rule: 'STR_immediate',
//       safety: [inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x0 & op1_repeated(24:20)=~0x010
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Store2RegisterImm12Op,
//       base: Rn,
//       baseline: Store2RegisterImm12Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc010pu0w0nnnnttttiiiiiiiiiiii,
//       rule: STR_immediate,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Case4_TestCase4) {
  Store2RegisterImm12OpTester_Case4 tester;
  tester.Test("cccc010pu0w0nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = {actual: 'LdrImmediateOp',
//       baseline: 'LdrImmediateOp',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       pattern: 'cccc010pu0w1nnnnttttiiiiiiiiiiii',
//       rule: 'LDR_immediate',
//       safety: [15  ==
//               inst(15:12) => FORBIDDEN_OPERANDS,
//         15  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            inst(15:12)  ==
//               inst(19:16) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: LdrImmediateOp,
//       base: Rn,
//       baseline: LdrImmediateOp,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc010pu0w1nnnnttttiiiiiiiiiiii,
//       rule: LDR_immediate,
//       safety: [Rn  ==
//               Pc => DECODER_ERROR,
//         P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         wback &&
//            Rn  ==
//               Rt => UNPREDICTABLE,
//         Rt  ==
//               Pc => FORBIDDEN_OPERANDS],
//       wback: P(24)=0 ||
//            W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LdrImmediateOpTester_Case5_TestCase5) {
  LdrImmediateOpTester_Case5 tester;
  tester.Test("cccc010pu0w1nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=1111 & inst(24:20)=~0x011 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'Load2RegisterImm12Op',
//       baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc0101u0011111ttttiiiiiiiiiiii',
//       rule: 'LDR_literal'}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Rt: Rt(15:12),
//       actual: Load2RegisterImm12Op,
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [Rt(15:12)],
//       pattern: cccc0101u0011111ttttiiiiiiiiiiii,
//       rule: LDR_literal}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Case6_TestCase6) {
  Load2RegisterImm12OpTester_Case6 tester;
  tester.Test("cccc0101u0011111ttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x0 & inst(24:20)=~0x110
//    = {actual: 'Store2RegisterImm12Op',
//       baseline: 'Store2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       pattern: 'cccc010pu1w0nnnnttttiiiiiiiiiiii',
//       rule: 'STRB_immediate',
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Store2RegisterImm12Op,
//       base: Rn,
//       baseline: Store2RegisterImm12Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc010pu1w0nnnnttttiiiiiiiiiiii,
//       rule: STRB_immediate,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rt  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Case7_TestCase7) {
  Store2RegisterImm12OpTester_Case7 tester;
  tester.Test("cccc010pu1w0nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=~1111 & inst(24:20)=~0x111
//    = {actual: 'Load2RegisterImm12Op',
//       baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       pattern: 'cccc010pu1w1nnnnttttiiiiiiiiiiii',
//       rule: 'LDRB_immediate',
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE,
//         15  ==
//               inst(19:16) => DECODER_ERROR,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            inst(15:12)  ==
//               inst(19:16) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load2RegisterImm12Op,
//       base: Rn,
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12)],
//       pattern: cccc010pu1w1nnnnttttiiiiiiiiiiii,
//       rule: LDRB_immediate,
//       safety: [Rn  ==
//               Pc => DECODER_ERROR,
//         P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rt  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            Rn  ==
//               Rt => UNPREDICTABLE],
//       wback: P(24)=0 ||
//            W(21)=1}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Case8_TestCase8) {
  Load2RegisterImm12OpTester_Case8 tester;
  tester.Test("cccc010pu1w1nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(24:20)=~0x111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'Load2RegisterImm12Op',
//       baseline: 'Load2RegisterImm12Op',
//       constraints: ,
//       defs: {inst(15:12)},
//       pattern: 'cccc0101u1011111ttttiiiiiiiiiiii',
//       rule: 'LDRB_literal',
//       safety: [15  ==
//               inst(15:12) => UNPREDICTABLE]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       actual: Load2RegisterImm12Op,
//       baseline: Load2RegisterImm12Op,
//       constraints: ,
//       defs: {Rt},
//       fields: [Rt(15:12)],
//       pattern: cccc0101u1011111ttttiiiiiiiiiiii,
//       rule: LDRB_literal,
//       safety: [Rt  ==
//               Pc => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Case9_TestCase9) {
  Load2RegisterImm12OpTester_Case9 tester;
  tester.Test("cccc0101u1011111ttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x010 & inst(4)=0
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0110u010nnnnttttiiiiitt0mmmm',
//       rule: 'STRT_A2'}
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0110u010nnnnttttiiiiitt0mmmm,
//       rule: STRT_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case10_TestCase10) {
  ForbiddenCondDecoderTester_Case10 tester;
  tester.Test("cccc0110u010nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x011 & inst(4)=0
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0110u011nnnnttttiiiiitt0mmmm',
//       rule: 'LDRT_A2'}
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x011 & B(4)=0
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0110u011nnnnttttiiiiitt0mmmm,
//       rule: LDRT_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case11_TestCase11) {
  ForbiddenCondDecoderTester_Case11 tester;
  tester.Test("cccc0110u011nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x110 & inst(4)=0
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0110u110nnnnttttiiiiitt0mmmm',
//       rule: 'STRBT_A2'}
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0110u110nnnnttttiiiiitt0mmmm,
//       rule: STRBT_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case12_TestCase12) {
  ForbiddenCondDecoderTester_Case12 tester;
  tester.Test("cccc0110u110nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x111 & inst(4)=0
//    = {actual: 'ForbiddenCondDecoder',
//       baseline: 'ForbiddenCondDecoder',
//       constraints: ,
//       pattern: 'cccc0110u111nnnnttttiiiiitt0mmmm',
//       rule: 'LDRBT_A2'}
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x111 & B(4)=0
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0110u111nnnnttttiiiiitt0mmmm,
//       rule: LDRBT_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case13_TestCase13) {
  ForbiddenCondDecoderTester_Case13 tester;
  tester.Test("cccc0110u111nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x0 & inst(4)=0 & inst(24:20)=~0x010
//    = {actual: 'Store3RegisterImm5Op',
//       baseline: 'Store3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       pattern: 'cccc011pd0w0nnnnttttiiiiitt0mmmm',
//       rule: 'STR_register',
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Store3RegisterImm5Op,
//       base: Rn,
//       baseline: Store3RegisterImm5Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc011pd0w0nnnnttttiiiiitt0mmmm,
//       rule: STR_register,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_Case14_TestCase14) {
  Store3RegisterImm5OpTester_Case14 tester;
  tester.Test("cccc011pd0w0nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x1 & inst(4)=0 & inst(24:20)=~0x011
//    = {actual: 'Load3RegisterImm5Op',
//       baseline: 'Load3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       pattern: 'cccc011pu0w1nnnnttttiiiiitt0mmmm',
//       rule: 'LDR_register',
//       safety: [15  ==
//               inst(15:12) => FORBIDDEN_OPERANDS,
//         15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load3RegisterImm5Op,
//       base: Rn,
//       baseline: Load3RegisterImm5Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc011pu0w1nnnnttttiiiiitt0mmmm,
//       rule: LDR_register,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN,
//         Rt  ==
//               Pc => FORBIDDEN_OPERANDS],
//       wback: P(24)=0 ||
//            W(21)=1}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_Case15_TestCase15) {
  Load3RegisterImm5OpTester_Case15 tester;
  tester.Test("cccc011pu0w1nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x0 & inst(4)=0 & inst(24:20)=~0x110
//    = {actual: 'Store3RegisterImm5Op',
//       baseline: 'Store3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       pattern: 'cccc011pu1w0nnnnttttiiiiitt0mmmm',
//       rule: 'STRB_register',
//       safety: [15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Store3RegisterImm5Op,
//       base: Rn,
//       baseline: Store3RegisterImm5Op,
//       constraints: ,
//       defs: {base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc011pu1w0nnnnttttiiiiitt0mmmm,
//       rule: STRB_register,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Rm  ==
//               Pc => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_Case16_TestCase16) {
  Store3RegisterImm5OpTester_Case16 tester;
  tester.Test("cccc011pu1w0nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x1 & inst(4)=0 & inst(24:20)=~0x111
//    = {actual: 'Load3RegisterImm5Op',
//       baseline: 'Load3RegisterImm5Op',
//       constraints: ,
//       defs: {inst(15:12), inst(19:16)
//            if inst(24)=0 ||
//            inst(21)=1
//            else 32},
//       pattern: 'cccc011pu1w1nnnnttttiiiiitt0mmmm',
//       rule: 'LDRB_register',
//       safety: [15  ==
//               inst(15:12) ||
//            15  ==
//               inst(3:0) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            inst(24)=0 ||
//            inst(21)=1 &&
//            inst(19:16)  ==
//               inst(3:0) => UNPREDICTABLE,
//         inst(24)=0 &&
//            inst(21)=1 => DECODER_ERROR,
//         inst(24)=0 ||
//            inst(21)=1 &&
//            (15  ==
//               inst(19:16) ||
//            inst(15:12)  ==
//               inst(19:16)) => UNPREDICTABLE,
//         inst(24)=1 => FORBIDDEN]}
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = {None: 32,
//       P: P(24),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Rt: Rt(15:12),
//       W: W(21),
//       actual: Load3RegisterImm5Op,
//       base: Rn,
//       baseline: Load3RegisterImm5Op,
//       constraints: ,
//       defs: {Rt, base
//            if wback
//            else None},
//       fields: [P(24), W(21), Rn(19:16), Rt(15:12), Rm(3:0)],
//       index: P(24)=1,
//       pattern: cccc011pu1w1nnnnttttiiiiitt0mmmm,
//       rule: LDRB_register,
//       safety: [P(24)=0 &&
//            W(21)=1 => DECODER_ERROR,
//         Pc in {Rt, Rm} => UNPREDICTABLE,
//         wback &&
//            (Rn  ==
//               Pc ||
//            Rn  ==
//               Rt) => UNPREDICTABLE,
//         ArchVersion()  <
//               6 &&
//            wback &&
//            Rn  ==
//               Rm => UNPREDICTABLE,
//         index => FORBIDDEN],
//       wback: P(24)=0 ||
//            W(21)=1}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_Case17_TestCase17) {
  Load3RegisterImm5OpTester_Case17 tester;
  tester.Test("cccc011pu1w1nnnnttttiiiiitt0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
